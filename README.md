# CodeQuest

**A Windows desktop C and C++ quiz game built with C++.**

CodeQuest is a custom-drawn Windows game interface for practicing programming concepts. Complete
quests, earn points and XP, unlock the boss challenge, collect power-ups, and track your progress.
Player progress is saved in `players.txt`.

## Build and run

Requires Windows and a C++17 MinGW compiler. Open this folder in VS Code and press **Ctrl+Shift+B**
to build and run the game. The default task compiles the Win32 app and launches `codequest.exe`.
Choose **Terminal > Run Task > CodeQuest: Build** to compile without launching.

Alternatively, run this command in a terminal opened in the project folder:

```
g++ -std=c++17 -Wall -Wextra -O2 -municode -o codequest.exe main.cpp Question.cpp Player.cpp Quiz.cpp Leaderboard.cpp -mwindows
```

Then launch `codequest.exe`. Keep `questions.txt` beside the executable.

## Project structure

| File | Purpose |
|---|---|
| `main.cpp` | Custom-drawn Windows UI, dashboard, quests, profile, leaderboard and reward shop |
| `Question.h/.cpp` | Question model and loader for `questions.txt` |
| `Player.h/.cpp` | Player stats, progression, inventory, badges and save format |
| `Quiz.h/.cpp` | Original console quiz engine and its full quiz modes |
| `Leaderboard.h/.cpp` | Loads and saves players and ranks them by XP |
| `questions.txt` | Question bank |
| `players.txt` | Local saved player progress (created/updated by the game; excluded from Git) |
| `.vscode/tasks.json` | VS Code build and run tasks |

## How to play

- The app loads the saved **Player One** profile, or creates it on first launch.
- Use the sidebar to switch between Dashboard, Player Profile, Leaderboard and Reward Shop.
- The Dashboard offers a Standard Quest, Pointer Challenge and Boss Battle (unlocks at Level 4).
- Click an answer card to answer. Use the Hint or Skip controls during a quest.
- Progress is saved to `players.txt`.

## Quests and scoring

- **Standard Quest:** five easy questions from C and C++.
- **Pointer Challenge:** up to five pointer questions; get at least four correct to pass.
- **Boss Battle:** up to ten hard object-oriented programming questions; unlocks at Level 4.
- Correct answers give question points, XP and one coin. Wrong answers subtract five points and one
  life. Completing a quest grants 25 bonus XP.
- The Reward Shop sells a hint token (30 coins), an extra life (60 coins), and double XP (80 coins).

## Adding questions

Add a line to `questions.txt` (fields separated by `|`; do not use `|` inside a field):

```
CATEGORY|DIFFICULTY|TOPIC|QUESTION|CORRECT|WRONG1|WRONG2|WRONG3
CPP|2|Advanced C++|Which header provides std::sort?|<algorithm>|<sort>|<vector>|<utility>
```

`CATEGORY` is `C` or `CPP`; `DIFFICULTY` is `1` (easy), `2` (medium), or `3` (hard). The first
answer is the correct one; the UI shuffles answers during play. Use the topic `Pointers` for Pointer
Challenge questions and `Object-Oriented Programming` for hard Boss Battle questions.

## Notes

- The desktop UI is a Windows application implemented with the Win32 API.
- The game uses a custom CodeQuest mark in the title bar and a matching Windows app icon. Cards and
  buttons have consistent spacing, depth shadows, hover/pressed feedback, and resize with the window;
  the minimum window size keeps the dashboard layout readable.
- The current GUI uses the default profile name **Player One** and includes a subset of the full
  console quiz engine's progression and special-mode features.
