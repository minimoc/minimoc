# Contribuer au miniMoc

Le miniMoc est un projet fait main, qui avance avec celles et ceux qui l'utilisent. Toute aide est la bienvenue — et il n'y a pas besoin d'être développeur pour contribuer utilement.

## Les façons d'aider

**Utiliser le miniMoc et raconter.** Le plus précieux, ce sont les retours d'usage réels : ce qui marche, ce qui coince, ce qui manque dans votre setup. Un message détaillé décrivant comment vous vous en servez vaut de l'or, surtout sur l'auto-enregistrement MIDI qui est encore en chantier.

**Signaler un problème.** Ouvrez une *issue* sur le dépôt en décrivant :

- ce que vous faisiez,  
- ce que vous attendiez,  
- ce qui s'est passé à la place,  
- votre matériel (révision du PCB, version du firmware, synthés branchés).

**Proposer une idée.** Une fonctionnalité qui vous manque ? Ouvrez une issue pour en discuter avant de coder quoi que ce soit — ça évite de partir dans une direction qui ne collerait pas au reste.

**Contribuer au code.** Corrections, améliorations, nouvelles fonctions : voir la section ci-dessous.

**Concevoir du matériel.** Le miniMoc n'a pas encore de boîtier complet — juste un *bezel* pour l'écran. Si vous aimez la CAO, un boîtier imprimable serait une contribution très appréciée. Idem pour les améliorations du PCB.

## Contribuer au code

1. *Forkez* le dépôt et créez une branche pour votre modification.  
2. Le firmware se compile avec **l'IDE Arduino \+ Teensyduino**, pour Teensy 4.1. Les dépendances s'installent via le gestionnaire de bibliothèques Arduino (voir le README) — ne les ajoutez pas au dépôt.  
3. Gardez vos changements ciblés : une contribution \= un sujet. C'est plus simple à relire et à intégrer.  
4. Testez sur du vrai matériel quand c'est possible, et dites-le dans votre *pull request*.  
5. Ouvrez la *pull request* en expliquant ce que vous avez changé et pourquoi.

## Un mot sur la licence

Le firmware est sous **PolyForm Noncommercial 1.0.0** et le matériel sous **CC BY-NC 4.0** (usage personnel libre, commercialisation sur accord — voir le README). En proposant une contribution, vous acceptez qu'elle soit distribuée sous ces mêmes licences. Si une contribution soulève une question de licence (par exemple du code venant d'ailleurs), signalez-le clairement dans la pull request.

## L'état d'esprit

Le miniMoc est un projet de passion, mené sur le temps libre. Les réponses peuvent prendre un peu de temps, et toutes les idées ne pourront pas être retenues — mais chaque retour est lu et compte. On construit un outil qui nous ressemble, sans pression et sans esbroufe. Merci d'en faire partie.  
