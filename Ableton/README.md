# AutoTrackCreator - POC Ableton Live

Script MIDI Remote Script pour Ableton Live qui crée automatiquement une piste MIDI avec un routage configurable.

Compatible avec Ableton Live Lite 11 et versions supérieures.

## Installation

### Automatique

```bash
python install.py
```

Options :
- `--symlink` : crée un lien symbolique au lieu de copier (pratique pour le dev, nécessite admin)
- `--system` : installe dans le chemin système au lieu du chemin utilisateur (nécessite admin)

### Manuelle

Copier le dossier `AutoTrackCreator/` dans le répertoire User Remote Scripts d'Ableton :

```
%APPDATA%\Ableton\Live 11.x.x\Preferences\User Remote Scripts\
```

## Configuration

### Activer dans Ableton

1. (Re)démarrer Ableton Live
2. Aller dans **Preferences > Link/Tempo/MIDI**
3. Dans un slot Control Surface, sélectionner **AutoTrackCreator**
4. Input/Output MIDI : laisser sur "None"

### Configurer le routage MIDI

Éditer le fichier `config.json` dans le dossier installé :

```json
{
  "track_name": "Auto-MIDI",
  "midi_input": "Nom du peripherique input",
  "midi_input_channel": "All Channels",
  "midi_output": "Nom du peripherique output",
  "midi_output_channel": "All Channels"
}
```

Pour connaître les noms exacts des périphériques disponibles, consulter le fichier `Log.txt` après le premier lancement. Le script y liste tous les périphériques détectés.

Laisser un champ vide (`""`) pour garder le routage par défaut d'Ableton.

### Chemin du Log.txt

```
%APPDATA%\Ableton\Live 11.x.x\Preferences\Log.txt
```

Chercher les lignes contenant `AutoTrackCreator` pour voir les messages du script.

## Workflow typique

1. `python install.py`
2. Ouvrir Ableton, activer AutoTrackCreator dans les préférences
3. La piste "Auto-MIDI" est créée, Log.txt liste les périphériques
4. Éditer `config.json` avec les noms souhaités
5. Relancer Ableton (ou désélectionner/resélectionner le Control Surface)
6. La piste est créée avec le bon routage

## Limitations

- **Live Lite** : maximum 8 pistes MIDI + 8 pistes Audio
- Pas de rechargement à chaud : il faut relancer Ableton ou basculer le Control Surface pour recharger le script
- Python 3.7 (version embarquée dans Ableton 11)
