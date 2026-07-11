# MiniMocPlug — Création dynamique de pistes via SysEx du MINIMOC

> Ce document décrit l'extension future du script AutoTrackCreator pour recevoir des commandes SysEx du MINIMOC et créer des pistes dynamiquement.

## Principe

Le MINIMOC envoie des messages MIDI SysEx à Ableton. Le script AutoTrackCreator les intercepte, parse le payload JSON, et crée les pistes avec le routage demandé.

## Protocole SysEx

### Format du message

```
F0 7D 01 <commande> <payload JSON en ASCII> F7
```

| Octet | Valeur | Signification |
|---|---|---|
| `F0` | Fixe | Début SysEx |
| `7D` | Fixe | Manufacturer ID non-commercial (réservé par la spec MIDI, usage privé) |
| `01` | Fixe | Device ID AutoTrackCreator |
| `<cmd>` | Variable | Commande (voir ci-dessous) |
| Payload | Variable | Chaîne JSON encodée en ASCII 7-bit |
| `F7` | Fixe | Fin SysEx |

### Commandes

| Commande | Octet | Payload | Description |
|---|---|---|---|
| Créer piste | `0x01` | JSON | Crée une piste avec la config spécifiée |
| Supprimer piste | `0x02` | JSON | Supprime une piste par nom |
| Ping | `0x03` | Aucun | Heartbeat — le script répond avec un acquittement |

### Payload "Créer piste"

```json
{
  "type": "midi",
  "port": 3,
  "name": "Synth",
  "channel": 1
}
```

| Champ | Type | Requis | Valeurs | Description |
|---|---|---|---|---|
| `type` | string | Oui | `"midi"` ou `"audio"` | Type de piste |
| `port` | integer | Oui (MIDI) | 2-16 | Numéro de port MINIMOC → dérivé en `MIDIIN{port} (MINIMOC)` / `MIDIOUT{port} (MINIMOC)` |
| `name` | string | Oui | ASCII | Nom affiché de la piste |
| `channel` | integer | Non | 1-16 | Canal MIDI. 0 ou absent = "All Channels" |

### Payload "Supprimer piste"

```json
{
  "name": "Synth"
}
```

### Réponse d'acquittement (vers le MINIMOC)

```
F0 7D 01 7F <status> <JSON> F7
```

- `status` : `0x00` = succès, `0x01` = erreur
- JSON : `{"cmd":1,"ok":true}` ou `{"cmd":1,"ok":false,"error":"message"}`

## Exemples SysEx concrets

### Ping
```
F0 7D 01 03 F7
```

### Créer une piste MIDI "Synth" sur le port 3, canal 1
```
F0 7D 01 01 7B 22 74 79 70 65 22 3A 22 6D 69 64 69 22 2C 22 70 6F 72 74 22 3A 33 2C 22 6E 61 6D 65 22 3A 22 53 79 6E 74 68 22 2C 22 63 68 61 6E 6E 65 6C 22 3A 31 7D F7
```
(= `{"type":"midi","port":3,"name":"Synth","channel":1}` en hexadécimal ASCII)

### Supprimer la piste "Synth"
```
F0 7D 01 02 {"name":"Synth"} F7
```

## Modifications du script à réaliser

### 1. `AutoTrackCreator.py`

**Nouvelles constantes :**
```python
SYSEX_HEADER = (0xF0, 0x7D, 0x01)
CMD_CREATE_TRACK = 0x01
CMD_DELETE_TRACK = 0x02
CMD_PING = 0x03
SYSEX_RESPONSE = 0x7F
STATUS_OK = 0x00
STATUS_ERROR = 0x01
```

**Modifications du `__init__` :**
- Ajouter `self._dynamic_tracks = []` pour tracker les pistes créées dynamiquement
- Conditionner la création initiale sur `config.get('create_initial_track', True)`

**Nouvelles méthodes :**

- `handle_sysex(midi_bytes)` — override du parent `ControlSurface`. Vérifie l'en-tête `F0 7D 01`, extrait la commande et le payload, dispatche vers le bon handler.

- `_handle_command(command, payload_bytes)` — Parse le JSON du payload et dispatche vers `_handle_create_track` ou `_handle_delete_track`.

- `_handle_create_track(payload)` — Crée la piste (MIDI ou Audio), stocke la référence dans `_dynamic_tracks`, puis diffère le routage via `schedule_message(DEFERRED_DELAY_TICKS, callback, (track, routing_config))`.

- `_build_routing_config(track_type, port, channel)` — Convertit le numéro de port en noms de périphériques via les templates `port_name_format` du config.json. Ex: port=3 → `"MIDIIN3 (MINIMOC)"`.

- `_deferred_dynamic_setup(params)` — Callback différé qui applique le routage via `_apply_routing()` (méthode existante réutilisée).

- `_handle_delete_track(payload)` — Supprime par nom, uniquement parmi `_dynamic_tracks`.

- `_send_ack(command, success, error_msg)` — Envoie un SysEx d'acquittement via `self._send_midi()`.

**Méthodes existantes réutilisées sans modification :**
- `_apply_routing()`, `_set_routing()`, `_log_available_routing()`, `_load_config()`

### 2. `config.json` — Nouveau schéma

```json
{
  "create_initial_track": true,
  "track_name": "Auto-MIDI",
  "midi_input": "MIDIIN2 (MINIMOC)",
  "midi_input_channel": "All Channels",
  "midi_output": "MIDIOUT2 (MINIMOC)",
  "midi_output_channel": "",
  "port_name_format": {
    "input": "MIDIIN{port} (MINIMOC)",
    "output": "MIDIOUT{port} (MINIMOC)"
  }
}
```

- `create_initial_track` : si `true`, crée la piste au démarrage (comportement actuel). `false` pour tout gérer via SysEx.
- `port_name_format` : templates pour dériver les noms de périphériques à partir du numéro de port. Adaptable à d'autres routeurs MIDI.

### 3. `__init__.py` — Ajouter `get_capabilities()`

```python
from _Framework.Capabilities import (
    PORTS_KEY, NOTES_CC, SCRIPT, REMOTE,
    inport, outport,
)

def get_capabilities():
    return {
        PORTS_KEY: [
            inport(props=[NOTES_CC, SCRIPT, REMOTE]),
            outport(props=[SCRIPT, REMOTE]),
        ]
    }

def create_instance(c_instance):
    from .AutoTrackCreator import AutoTrackCreator
    return AutoTrackCreator(c_instance)
```

### 4. `install.py`

Mettre à jour les instructions "Next steps" pour mentionner l'assignation du MIDI Input.

## Contrainte importante : port de contrôle dédié

Le port MIDI assigné comme Control Surface Input est **consommé** par le Control Surface — il n'apparaîtra plus comme entrée MIDI disponible sur les pistes.

**Recommandation :** Dédier le **port 2 du MINIMOC** au contrôle SysEx. Les ports 3-16 restent disponibles pour le routage musical des pistes.

## Configuration dans Ableton

1. Preferences > Link/Tempo/MIDI
2. Control Surface : **AutoTrackCreator**
3. MIDI Input : **MIDIIN2 (MINIMOC)** (port de contrôle)
4. MIDI Output : **MIDIOUT2 (MINIMOC)** (pour les acquittements, optionnel)

## Vérification

1. `python install.py` → copier vers Ableton
2. Redémarrer Ableton, configurer AutoTrackCreator avec MIDI Input = `MIDIIN2 (MINIMOC)`
3. Envoyer un ping : `F0 7D 01 03 F7` → vérifier "Ping received" dans Log.txt
4. Créer une piste : `F0 7D 01 01 {"type":"midi","port":3,"name":"Synth","channel":1} F7`
5. Vérifier qu'une piste "Synth" apparaît, routée sur MIDIIN3/MIDIOUT3 (MINIMOC), canal 1
6. Tester la suppression : `F0 7D 01 02 {"name":"Synth"} F7`
7. Tester les erreurs : limite de pistes (Live Lite: 8 MIDI + 8 Audio), JSON invalide

## Notes techniques

- **Timing** : Le routage est appliqué en différé (`schedule_message`, 10 ticks) car les propriétés de routage ne sont pas disponibles immédiatement après la création de la piste.
- **Python 3.7** : Pas de f-strings, utiliser `.format()`. Pas de walrus operator.
- **Taille SysEx** : Garder les noms de pistes courts (< 50 caractères). Les payloads typiques font ~50 octets, bien dans les limites.
- **Messages simultanés** : Si le MINIMOC envoie plusieurs commandes rapidement, espacer d'au moins ~100ms pour éviter de surcharger Ableton.
- **Référence de piste invalide** : Dans le callback différé, vérifier que la piste existe encore avant d'appliquer le routage (l'utilisateur pourrait la supprimer manuellement entre-temps).
