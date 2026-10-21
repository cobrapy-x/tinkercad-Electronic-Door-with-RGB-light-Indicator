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

if (!isRepeat && fromOurRemote) {
bool tooSoon = (command == lastCommand) &&
(now - lastCommandTime < IR_DEBOUNCE_MS);
...
}


The debounce window is 250 ms.

### Changing the PIN

Pressing FUNC/STOP from the locked state begins a three-step process. Each step has its own colour, which is how the user knows which question is being asked.

| Step | Colour | The system is asking for |
| ---- | ------ | ------------------------ |
| 1 | Magenta | The current PIN |
| 2 | Cyan | A new PIN, 4 to 8 digits |
| 3 | Yellow | The new PIN a second time |

If all three steps succeed, the LED turns white and the new PIN takes effect immediately. The new PIN is required to confirm, so a typo cannot silently lock the owner out. If the current PIN is wrong, the request is refused. If the two new entries do not match, the process returns to step 2. Pressing Power at any point cancels, and the process gives up on its own after 15 seconds of inactivity.

A changed PIN is held in RAM, so it lasts until the simulation is stopped. Storing it permanently requires the EEPROM and is planned for version 4.

### Colour reference

| Colour | Channels on | Meaning |
| ------ | ----------- | ------- |
| Blue | B | Door locked, waiting for a PIN |
| Green | G | Access granted, door open |
| Red | R | Refused, or an entry was rejected |
| Magenta | R + B | Change PIN, step 1 |
| Cyan | G + B | Change PIN, step 2 |
| Yellow | R + G | Change PIN, step 3 |
| White | R + G + B | The PIN was changed |

The RGB LED is a common-cathode type. Its common cathode is connected to ground, so each channel lights when its Arduino pin is driven HIGH. Each channel is either fully on or fully off, giving exactly seven colours: the three channels alone, the three pairs, and all three together. All seven are used.

---

## Design files

| File | Description |
| ---- | ----------- |
| [`electronic_door_lock_v3.ino`](electronic_door_lock_v3.ino) | Arduino C++ program for the IR door lock |
| [`ir_code_reader.ino`](ir_code_reader.ino) | Utility sketch that prints the command for every button pressed |

⚠️ The schematic and board files from versions 1 and 2 describe the keypad circuit and **no longer match this version**. New design files have not yet been produced.

---

## Parts

| Part (Tinkercad name) | Qty | Settings |
| --------------------- | --- | -------- |
| Arduino Uno R3 | 1 | 5 V logic |
| IR remote | 1 | NEC protocol, address `0xBF00` |
| IR sensor | 1 | 3-pin receiver |
| Micro Servo | 1 | 0° locked, 90° unlocked |
| RGB LED | 1 | Common cathode |
| Resistor | 3 | 220 Ω, one for each RGB channel |
| Connecting wires | As required | Used for signal, 5 V and GND connections |

For real hardware, a TSOP-series receiver such as the TSOP38238 and any NEC-protocol remote would work. The remote's own address and command codes will differ from those listed here and must be read off the specific remote before use.

---

## Wiring

### IR receiver

| Receiver pin | Arduino connection |
| ------------ | ------------------ |
| Signal | D2 |
| VCC | 5V |
| GND | GND |

⚠️ The pin order on an IR receiver is not the intuitive one. On common TSOP packages the signal pin sits on the outer edge where ground would be expected. Hover over each pin in Tinkercad to confirm before wiring. Reversing VCC and GND leaves the part silently dead with no indication of the cause.

### Micro servo

| Servo connection | Arduino connection |
| ---------------- | ------------------ |
| Signal | D10 |
| Power | 5V |
| Ground | GND |

### Common-cathode RGB LED

| RGB LED connection | Arduino connection |
| ------------------ | ------------------ |
| Red | D11 through 220 Ω resistor |
| Green | D12 through 220 Ω resistor |
| Blue | D13 through 220 Ω resistor |
| Common cathode | GND |

D3 to D9 are unused in this version.

⚠️ Do not connect the RGB LED channels directly to the Arduino outputs without current-limiting resistors.

⚠️ The common pin of the RGB LED used in this project is the cathode and must be connected to GND. A common-anode RGB LED requires different wiring and inverted control logic.

⚠️ A real high-current door lock or large servo should not be powered directly from an Arduino output pin. A suitable driver and power supply should be used.

---

## Remote reference

Every button shares the address `0xBF00`. The remote is laid out in a grid of three columns, and the command codes follow that grid as `row × 4 + column`, leaving a gap at the end of each row.

| Button | Code | Used for |
| ------ | ---- | -------- |
| Power | `0x00` | Cancel and return to the locked state |
| VOL+ | `0x01` | Not used |
| FUNC/STOP | `0x02` | Start changing the PIN |
| Previous | `0x04` | Clear the digits typed so far |
| Play/Pause | `0x05` | Submit the entry |
| Next | `0x06` | Not used |
| Down | `0x08` | Not used |
| VOL− | `0x09` | Not used |
| Up | `0x0A` | Not used |
| `0` | `0x0C` | Enter a digit |
| EQ | `0x0D` | Not used |
| ST/REPT | `0x0E` | Not used |
| `1` `2` `3` | `0x10` `0x11` `0x12` | Enter a digit |
| `4` `5` `6` | `0x14` `0x15` `0x16` | Enter a digit |
| `7` `8` `9` | `0x18` `0x19` `0x1A` | Enter a digit |

Note that `0` is not grouped with the other digits. It sits at `0x0C`, in the row above the numeric block, alongside EQ and ST/REPT.

The four function buttons are a choice, not a property of the remote. They were picked so the printed icons match what they do: Play/Pause reads as "go", Previous reads as "back".

---

## Test procedure

1. Start the simulation. The RGB LED should turn blue and the servo should remain at 0°.

2. Press any digit. A single `*` should appear in the Serial Monitor. If nothing appears, check the receiver's VCC and GND first.

3. Press `0`. A `*` should appear. This confirms the `0x0C` mapping, which is the only code that does not follow the obvious grouping.

4. Enter `1234` and press Play/Pause. The LED should turn green and the servo should rotate to 90°.

5. Wait 5 seconds. The servo should return to 0° and the LED should return to blue.

6. Enter an incorrect PIN such as `1111` and press Play/Pause. The LED should turn red for 2 seconds, the servo should not move, and the LED should then return to blue.

7. Enter `12`, press Previous, then enter `1234` and press Play/Pause. Access should be granted, confirming that clearing works mid-entry.

8. Press FUNC/STOP. The LED should turn magenta.

9. Enter `1234` and press Play/Pause. The LED should turn cyan.

10. Enter `5678` and press Play/Pause. The LED should turn yellow.

11. Enter `5678` and press Play/Pause. The LED should turn white, then return to blue.

12. Enter `5678` and press Play/Pause. Access should be granted.

13. Enter `1234` and press Play/Pause. Access should be denied, because the old PIN no longer works.

14. Press FUNC/STOP, then press Power. The system should return to the locked state with the PIN unchanged.

15. Press FUNC/STOP, then wait 15 seconds without pressing anything. The process should give up on its own.

16. Press FUNC/STOP, enter the current PIN, then enter a 3-digit new PIN and press Play/Pause. It should be rejected as too short.

17. Press FUNC/STOP, enter the current PIN, then enter two different new PINs. The second entry should be rejected and the process should return to step 2.

18. Hold a digit button down. Exactly one `*` should appear, not a stream. This verifies the repeat filter.

19. Stop and restart the simulation. The PIN should be `1234` again.

20. Open the Serial Monitor throughout. Each state change and access result should be printed, and entered digits should appear only as `*`.

All twenty checks passed on the first run of the completed sketch.

---

## Results and observations

- The IR receiver successfully decoded every button on the remote as NEC protocol.

- All buttons returned the address `0xBF00`, confirming that address filtering is a usable way to reject other remotes.

- The command codes follow the remote's physical grid rather than its printed labels. The `0` key is at `0x0C`, separated from `1` to `9`, which is the kind of detail that cannot be assumed and must be read off the hardware.

- Replacing the keypad with IR required changes to exactly four things: the include, the input configuration block, one line in `setup()`, and one line in `loop()`. No function below `handleKey()` was modified.

- The default PIN `1234` was correctly recognised over IR.

- A new PIN set through the remote took effect immediately and the previous PIN stopped working.

- The repeat filter correctly reduced a held button to a single digit.

- The micro servo rotated from 0° to 90° when access was granted and returned to 0° afterwards.

- A changed PIN did not survive stopping and restarting the simulation, which is expected, since it is only held in RAM.

- Input pin usage dropped from eight to one:

    $n_{pins} = 8 \rightarrow 1$

- The programmed unlocked time is:

    $t_{unlock} = 5000\text{ ms} = 5\text{ s}$

- The programmed result-colour time is:

    $t_{result} = 2000\text{ ms} = 2\text{ s}$

- The IR debounce window is:

    $t_{debounce} = 250\text{ ms}$

- No electrical voltage or current measurements were recorded for the completed circuit.

---

## Known issues and limitations

### IR is a broadcast medium

This is the most significant difference between version 2 and version 3, and it is a property of the technology rather than a bug in the implementation.

A keypad requires physical contact with the door. A remote does not. Anyone within line of sight can transmit, and the NEC codes used here are neither secret nor difficult to discover — a receiver costing very little will read them in seconds, and a universal remote can replay them.

The address check rejects frames from other remotes, but an attacker who knows the address can send whatever they like. Address filtering prevents accidental interference, not deliberate attack.

A real wireless lock addresses this with rolling codes, where the transmitter and receiver advance through a shared pseudo-random sequence so a captured code is never valid twice. That is well beyond the scope of this project but it is the correct answer to the problem.

### Timer conflict

The IRremote library claims Timer2 on an Uno, which disables PWM output on D3 and D11. D11 is the red channel of the RGB LED.

This costs nothing here, because all three channels are driven with `digitalWrite` and are either fully on or fully off. It does mean that `analogWrite` colour mixing is no longer possible on this board without moving the LED or using a microcontroller with more timers. Combining IR, a servo on Timer1, and three PWM colour channels does not fit on an Uno.

### Other limitations

- A changed PIN is lost when the simulation stops. There is no permanent storage.
- There is no lockout after repeated incorrect attempts. This matters more than it did in version 2, since an attacker no longer has to be standing at the door.
- The PIN is held in plain text in RAM. A real access-control system would store a hash rather than the PIN itself.
- The RGB LED communicates the system state but cannot display messages or show how many digits have been entered.
- Yellow and white rely on the three dice inside the RGB LED being evenly matched. On real low-cost parts they usually are not, so these two colours would look less accurate on physical hardware than they do in simulation.
- The command codes are specific to this remote. A different remote requires re-reading them with the utility sketch.
- The micro servo represents a door lock. The simulation does not test the mechanical forces required to operate a real door lock.
- The circuit has not been verified as a physical door security system.
- No electrical current or voltage measurements were recorded for the final design.

---

## Things I learned

- An input device can be replaced without touching the logic that consumes its output, provided the boundary between them was drawn in the right place. Version 2 did this by accident; version 3 is the proof that it worked.

- IR remote codes carry an address as well as a command. The address identifies the remote, which makes a crude form of pairing possible for free.

- Button codes follow the remote's internal grid, not its labels. The `0` key was not where it appeared it should be.

- NEC repeat frames and genuine duplicate frames are different things and need different filters. Handling only one of them is worse than handling neither, because the failure is intermittent rather than obvious.

- The IRremote library changed incompatibly at version 3. Nearly every tutorial online is written for the 2.x API, and version 4.x requires `IRremote.hpp` rather than `IRremote.h`, `IrReceiver.decode()` rather than `irrecv.decode(&results)`, and reads the button from `decodedIRData.command` rather than from a single packed 32-bit value.

- Libraries claim hardware timers, and timers are a finite resource. Two libraries that each work alone can quietly disable features of the other.

- Making something wireless weakens its security model even when the code is identical. The convenience and the vulnerability are the same property.

---

## Future improvements

- Store the PIN in EEPROM so it survives a power cycle. This is the next version.
- Add a maximum number of incorrect attempts followed by a temporary lockout.
- Store a hash of the PIN rather than the PIN itself.
- Add a 16x2 parallel LCD. The keypad previously occupied D2 to D9 and those pins are now free, so the parallel display that the I2C attempts were a substitute for will finally fit.
- Display masked characters such as `****` while the PIN is being entered, and show the PIN-change prompts in words instead of colours.
- Add a buzzer for audible feedback, which matters more for a remote than for a keypad since there is no tactile confirmation of a press.
- Investigate rolling codes as a replacement for fixed NEC commands.
- Add a door-position sensor to detect whether the door is actually open or closed.
- Produce new schematic and board files for the IR circuit.
- Build a physical prototype and measure its supply voltage and current consumption.

---

## Version history

| Version | Summary |
| ------- | ------- |
| v1 | Fixed PIN `1234`, three RGB colours, LCD attempted and removed |
| v2 | User-changeable PIN, seven RGB colours, state machine, non-blocking timing |
| v3 | Keypad replaced by IR remote, address filtering, repeat rejection, logic unchanged |

---

## License

MIT
