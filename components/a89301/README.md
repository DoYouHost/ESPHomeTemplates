# A89301

ESPHome component for the Allegro **A89301** — a 50 V, three-phase sensorless FOC BLDC motor
controller with an I²C interface.

The driver is exposed as a `fan` entity (on/off, speed, direction) plus sensors for speed, current,
duty cycle, supply voltage, temperature and the operating state read back over I²C. The BRAKE input
is driven from a GPIO as a safety interlock and is not exposed as a user-facing entity.

## Hardware notes

Read these before wiring the board — the A89301 has a few traps.

**SPD is SCL, FG is SDA.** In I²C mode the driver reuses the speed-input pin as the clock and the
tachometer output as the data line. Once the first I²C command has been received, FG no longer
produces speed pulses; the speed is read from register 120 instead. SCL needs no pull-up (the
A89301 never drives it), SDA needs 3.3–10 kΩ to VREG (2.8 V) or to 3.3 V.

Until that first command arrives, FG still behaves as a tachometer output, so a driver that is
already spinning will be pulling the SDA line up and down and no I²C transfer can get through. Set
`FG_PIN_DIS` (EEPROM 14[4]) in the configuration application to dedicate the pin to SDA
permanently. With the recommended BRAKE pull-up this also resolves itself on an ESP reset — the
board brakes, the motor stops, FG goes quiet — but relying on that is fragile.

**Pull the BRAKE pin high in hardware.** BRAKE is active-high and overrides the speed demand. ESP32
GPIOs float until `setup()` runs, so without an external pull-up there is a window during boot in
which the motor is neither braked nor commanded. Fit a pull-up (e.g. 10 kΩ to 3.3 V) so the board
defaults to braking, and let the ESP pull it low to release.

**Program the EEPROM first.** This component only *reads* the motor parameters — rated voltage,
rated current, rated speed, resistance, sense resistor, startup profile, PID gains. Those must
already be in the driver's EEPROM, programmed with Allegro's A89301 configuration application.
Running with wrong parameters can damage the driver or the motor.

**The I²C clock is slow.** 7 kHz to 200 kHz. Set `frequency: 100kHz` (or lower) on the `i2c:` bus.

**Standby kills the I²C link.** Standby mode powers down the entire IC, including the I²C
interface, and it is entered whenever the speed demand drops below the OFF threshold. The component
therefore sets `STANDBY_DIS` (register 85 bit 15) at startup. Leave `disable_standby: true` unless
you have a way to power-cycle the driver.

## Configuration

```yaml
external_components:
  - source: github://MorganMLGman/DoYouHost.ESPHomeTemplates
    components: [a89301]

i2c:
  sda: GPIO21
  scl: GPIO22
  frequency: 100kHz

a89301:
  id: motor_drv
  pole_pairs: 6
  brake_pin: GPIO16
  update_interval: 5s

fan:
  - platform: a89301
    a89301_id: motor_drv
    name: "Motor"
    restore_mode: RESTORE_DEFAULT_OFF
```

### `a89301:` (hub)

| Option | Default | Description |
| --- | --- | --- |
| `pole_pairs` | **required** | Motor pole pairs. Register 120 reports the *electrical* frequency, so this is needed to compute RPM (`RPM = Hz × 60 / pole_pairs`). A 12-pole motor has 6 pole pairs. |
| `address` | `0x55` | Fixed by the A89301. |
| `brake_pin` | – | GPIO driving the BRAKE input. Required when `brake_when_off` is true. |
| `brake_when_off` | `true` | Engage the brake once the motor has been commanded off. |
| `brake_delay` | `2s` | How long to wait after zeroing the demand before engaging the brake. Braking a spinning motor stresses the MOSFETs, so give it time to coast down. |
| `disable_standby` | `true` | Set `STANDBY_DIS` so a zero demand does not power down the I²C interface. |
| `watchdog_interval` | `1s` | How often to verify the driver, independently of `update_interval`. `0s` disables it. |
| `sense_resistor` | read from register 84 | Override the shunt value used to scale the current readings, e.g. `15mΩ`. |
| `over_voltage_threshold` | `47V` | VBB at or above this raises the `over_voltage` binary sensor. |
| `under_voltage_threshold` | `5.5V` | VBB at or below this raises the `under_voltage` binary sensor. |
| `update_interval` | `5s` | Status readback interval. |

The brake is engaged unconditionally — ignoring `brake_when_off` — whenever the driver is
unreachable: at boot before the first successful transfer, and after three consecutive failed I²C
transfers. It is released again only after the link is back and the fan has been commanded on.

### The watchdog

`watchdog_interval` polls register 81 on its own schedule, so the reaction time of the safety
interlock does not depend on how often you read the sensors. A single read answers two questions.

*Does the driver still respond?* Three consecutive failures engage the brake. Because a failed read
does not refresh the timestamp, the retries happen back to back rather than one per interval, so a
genuine loss of communication brakes within milliseconds instead of `3 × update_interval`.

*Is the driver still under I²C speed control?* This is the failure that matters most and the one a
liveness check alone would miss. If VBB browns out, the A89301 reloads its EEPROM and reverts to
**SPD-pin speed control — and SPD is the SCL line.** An idle I²C bus holds SCL high, which in analog
or PWM mode reads as a 100 % speed demand, so the motor spins up to full speed while still
acknowledging every I²C transfer perfectly. Bit 9 of register 81 (`I2C_SPEED_MODE`) is clear in that
state; the component brakes, re-initializes the driver and re-applies the commanded state.

Set `watchdog_interval: 0s` only if something else guarantees the driver cannot be reset
independently of the ESP.

### `fan:`

Standard `fan` options plus:

| Option | Default | Description |
| --- | --- | --- |
| `speed_count` | `100` | Number of speed levels. |
| `min_demand` | `10%` | Demand corresponding to speed level 1. Levels are mapped linearly onto `min_demand`…100 %. The A89301 refuses to spin below its speed-input OFF threshold (10 % by default), so mapping the bottom of the range away from zero keeps every selectable level usable. |

`restore_mode` is honoured, but the restored state is applied only once the driver has been
successfully configured over I²C, not at `setup()`. The same path re-applies the state after a
recovered communication loss.

Direction is changed through register 72 bit 14; the A89301 supports reversing while running and
reports state 10 (*Changing direction*) while it does so.

### `sensor:`

| Key | Unit | Source |
| --- | --- | --- |
| `speed` | Hz | Register 120 × 0.530 — electrical frequency |
| `rpm` | RPM | `speed × 60 / pole_pairs` |
| `bus_current` | A | Register 121, scaled by the sense resistor |
| `q_axis_current` | A | Register 122 — phase current peak × 0.866 |
| `supply_voltage` | V | Register 123 ÷ 5 |
| `temperature` | °C | Register 124 − 53 |
| `speed_demand` | % | Register 125, 0–511 |
| `duty` | % | Register 126, 0–511 — the demand the driver is actually applying |

### `binary_sensor:`

All derived from `state_top_level` (register 127 [15:12]) except the last two, which the A89301 does
not report as status bits and which are computed from the VBB readback.

| Key | True when |
| --- | --- |
| `fault` | State is lock, OCP, OTP or bad system |
| `spinning` | State 3 |
| `starting` | State 1, 2 or 9 (first cycle / IPD / windmill) |
| `standby` | State 0, 7 or 15 |
| `braking` | State 6 or 14 |
| `lock` | State 5 — rotor not following |
| `over_current` | State 11 |
| `over_temperature` | State 12 |
| `system_error` | State 13 — VREG or VCP abnormal |
| `over_voltage` | VBB ≥ `over_voltage_threshold` |
| `under_voltage` | VBB ≤ `under_voltage_threshold` |

### `text_sensor:`

`operation_state` publishes the decoded `state_top_level` as text.

## Register reference

Shadow registers mirror the EEPROM at `EEPROM address + 64`; writes to them take effect immediately
and are lost on power cycle. Reads are a two-step transaction — register address, **STOP**, then a
separate read — the A89301 does not accept a repeated START.

| Register | Field | Meaning |
| --- | --- | --- |
| 72 | `[14]` | `DIRECTION`; 1 = A→B→C, 0 = A→C→B |
| 81 | `[9]` / `[8:0]` | `I2C_SPEED_MODE` / `SPEED_DEMAND` 0–511 |
| 84 | `[15:8]` | `SENSE_RESISTOR`; mΩ = value / 3.7 |
| 85 | `[15]` | `STANDBY_DIS` |
| 120–126 | – | Readback: speed, bus current, q-axis current, VBB, temperature, demand, command |
| 127 | `[15:12]` | `state_top_level` |

Sources: A89301 datasheet (Allegro MicroSystems) and application note UM-A89301 — the state table is
documented only in the latter.

## Verifying on hardware

The readback registers 120–124 are read as full 16-bit words. The datasheet does not state their
field widths, so if a reading looks implausible (for example a temperature in the thousands), enable
`level: VERBOSE` on the logger and check the `Readback 120..127` line — it prints all eight raw
words, from which the actual width can be determined and the masks corrected.
