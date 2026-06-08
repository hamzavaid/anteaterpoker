# Anteater Poker

**Version:** 1.0 Full Release  
**Date:** May 2026  
**Course:** EECS 22L  
**Institution:** University of California, Irvine  
**Team:** ZotHouse  

## Team Members

- Jim Truong
- Hamza Vaid
- Shogo Stuck
- Ben Choi
- Giovanna Dunker Estruquel

## Overview

Anteater Poker is an online multiplayer poker game based on Texas Hold'em with additional UCI Anteater-themed special ability cards. The game is written in C for the EECS 22L Software Engineering Project.

The program uses a central server to manage the poker table, game state, connected players, bot players, card dealing, legal actions, point tracking, and game updates. Players connect to the server through a graphical client application and play against other users or bot players.

## Full Release Notes

This is version 1.0 of Anteater Poker. This is the final game's implementation with the ability to play poker, add bots, and use special ability cards. This release has expanded the program since the beta, providing all around upgrades to the game and featuring a much smoother user experience, while ironing out many of the bugs and issues that were present in previous versions.

## Anteater Special Ability Cards

Anteater Poker includes special ability cards that give players additional strategic actions during a round. These cards are separate from regular poker cards unless the specific ability says otherwise.

## Package Contents

The user package should contain:

```text
README
COPYRIGHT
INSTALL
bin/
    poker_client
    poker_server
doc/
    Poker_UserManual.pdf
```

The source code package should contain:

```text
README
COPYRIGHT
INSTALL
Makefile
bin/
doc/
    Poker_UserManual.pdf
    Poker_SoftwareSpec.pdf
src/
```

## Installation and Execution

For installation and Execution instructions, see the `INSTALL` file.

For detailed user instructions, see:

```text
doc/Poker_UserManual.pdf
```

For developer and design information, see:

```text
doc/Poker_SoftwareSpec.pdf
```

## Documentation

User documentation is provided in:

```text
doc/Poker_UserManual.pdf
```

Developer and design documentation is provided in:

```text
doc/Poker_SoftwareSpec.pdf
```

## Copyright

Copyright and ownership information is provided in the `COPYRIGHT` file.
