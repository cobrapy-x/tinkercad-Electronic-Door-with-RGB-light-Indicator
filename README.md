# Electronic Door Lock with RGB Indicator (Tinkercad)

This project is a password-controlled electronic door lock simulated in Tinkercad. An Arduino Uno reads a PIN and a micro servo represents the physical locking mechanism.

An RGB LED provides all status feedback. Seven different colours tell the user whether the door is locked, whether access was granted or denied, and which step of the PIN-change process is in progress. The project demonstrates the basic operating principle behind electronic access systems used in doors, lockers, cabinets, and other security systems.

**Version 3** replaces the 4x4 keypad with an infrared remote and receiver, making the lock wireless. The user-changeable PIN from version 2 is retained. The default PIN is still `1234`.

🔗 **Tinkercad simulation:** [open the circuit](https://www.tinkercad.com/things/i7V48scyurK-electronic-door-with-rgb-light-indicator-version3)

![Circuit in Tinkercad](Electronic_Door_Lock_v3.png)

---

## What changed in version 3

| Area | Version 2 | Version 3 |
| ---- | --------- | --------- |
| Input device | 4x4 matrix keypad | IR remote and IR receiver |
| Input pins used | D2 to D9 (eight pins) | D2 only (one pin) |
| Range | Contact with the keypad | Line of sight across a room |
| Input layer | `keypad.getKey()` | `readIRKey()`, NEC decode |
| Remote filtering | Not applicable | Address check and repeat rejection |
| State machine | Six states | Six states, unchanged |
| PIN change | Working | Working, unchanged |
| RGB indicator | Seven colours | Seven colours, unchanged |
| Spare pins | None | D3 to D9 free |

Versions 1 and 2 remain available as releases so the earlier projects can still be viewed and simulated.

### The point of this version

The entire input device was replaced and nothing below the input layer changed.

Version 2 was written so that `handleKey(char)` receives a character and acts on it. It does not know, and does not need to know, where that character came from. The keypad produced characters; the IR layer now produces the same characters. Everything downstream — the state machine, the PIN comparison, the colours, the timers — compiled and ran without modification.

This was not planned in advance. It is a consequence of version 2 happening to put a clean boundary between reading input and deciding what to do about it, and it is the most useful thing this project has demonstrated so far.

---

## How it works

The IR receiver on D2 decodes NEC-protocol frames from the remote. Each frame carries an address identifying the remote and a command identifying the button. The sketch rejects any frame whose address is not `0xBF00`, so a different remote in the room cannot drive the lock.

A valid command is looked up in a table that maps it to a character. That character is passed to `handleKey()`, which behaves exactly as it did in version 2. Number buttons are added to the entry, Previous clears it, Play/Pause submits it, FUNC/STOP begins a PIN change, and Power cancels.

The PIN starts as `1234` every time the simulation starts.

When the entered PIN is correct, the RGB LED turns green and the servo rotates from 0° to 90°. This represents the door unlocking. After 5 seconds, the servo returns to 0° and the LED returns to blue. If the PIN is incorrect, the LED turns red for 2 seconds and the servo does not move.

### Filtering repeated presses

NEC marks a held button with a repeat flag, but a held button also sends one genuine frame before the repeats begin, and a slightly long press often sends a second genuine frame. The repeat flag alone is therefore not enough, and the same command arriving twice in quick succession is rejected as well.

Both filters are needed. Without them, an entry of `1234` easily becomes `11223344` and the lock never opens.
