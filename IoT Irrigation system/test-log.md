# Test log

Covers the core system only (soil moisture, temperature, light, water
level) — the part that was physically built and tested. The extended
sensors (humidity, rain, pH, flow) have no test data since they were
never built; see the README's Status section.

**Note on the data below:** the original project did not preserve raw
logged values, only a qualitative description of test outcomes. The
tables here reconstruct representative values consistent with that
description and the sensor specs, for illustration. Replace with actual
logged readings if you still have them or rebuild and re-test.

## Soil moisture -> irrigation

| Trial | Soil condition | Raw ADC value | Threshold | Solenoid state |
|---|---|---|---|---|
| 1 | Dry | 620 | 400 | Open (irrigating) |
| 2 | Damp | 410 | 400 | Open (irrigating) |
| 3 | Wet | 180 | 400 | Closed |
| 4 | Saturated | 90 | 400 | Closed |

## Water tank level -> refill pump

| Probe stage | Probes wet | Pump state |
|---|---|---|
| 0 (empty) | 0 of 4 | On |
| 1 | 1 of 4 | On |
| 2 | 2 of 4 | On |
| 3 (full) | 4 of 4 | Off |

## Temperature -> heating/cooling relay

| Trial | Measured temp (C) | High threshold | Low threshold | Relay state |
|---|---|---|---|---|
| 1 | 34.2 | 30 | 15 | Cooling on |
| 2 | 22.5 | 30 | 15 | Both off |
| 3 | 11.8 | 30 | 15 | Heating on |

## Light (LDR) -> lighting relay

| Trial | Condition | Raw ADC value | Threshold | Relay state |
|---|---|---|---|---|
| 1 | Bright daylight | 780 | 300 | Off |
| 2 | Overcast | 340 | 300 | Off |
| 3 | Dusk | 210 | 300 | On |
| 4 | Dark | 60 | 300 | On |

## Observations

- Solenoid and pump response was immediate on each threshold crossing,
  no debounce issues observed at the tested sample rate.
- Water level transitions were clean across all four probe stages with
  no false triggers from splash or condensation during testing.
- Temperature and light thresholds held consistently across repeated
  trials at the same conditions.

## Not tested

Humidity override, rain override, soil pH, water flow tracking, and the
MQTT/cloud connectivity layer — all designed-only, no physical build to
generate real data from.

## Projected test plan for designed extensions

**These are expected outcomes derived from the firmware logic, not
results — none of this has been run.** Included to show what validation
would look like once the hardware exists, not as a substitute for it.
Presenting this table as real results would misrepresent the project.

| Subsystem | Test scenario | Expected outcome | Basis |
|---|---|---|---|
| Rain override | Soil reads dry, rain sensor wet | Solenoid stays closed | `shouldIrrigate` requires `!rainDetected` |
| Rain override | Soil reads dry, rain sensor dry | Solenoid opens (if humidity also low) | Same condition, inverse |
| Humidity override | Soil dry, no rain, humidity > 80% | Solenoid stays closed | `shouldIrrigate` requires `!humidityHigh` |
| Humidity override | Soil dry, no rain, humidity < 80% | Solenoid opens | Same condition, inverse |
| Soil pH | Probe in pH 7 buffer solution | Reading should settle near pH 7 after calibration | `readPH()` requires slope/offset calibration first — will NOT read correctly with placeholder values |
| Soil pH | Probe in pH 4 buffer solution | Reading should settle near pH 4 after calibration | Same |
| Water flow | Known volume (e.g. 1L) passed through sensor | `totalLitres` increments by ~1.0, within sensor's rated accuracy | `FLOW_PULSES_PER_LITRE` constant — needs verifying against the actual sensor's datasheet or a manual calibration pour |
| Water flow | No flow | `totalLitres` stays constant, pulse count reads 0 | Interrupt-driven counter with no incoming pulses |
| Connectivity | Arduino connected via USB to Pi | Pi receives one CSV line per second matching sensor readings | Serial baud rate and format match `Serial.begin(9600)` and print order in firmware |
| Connectivity | MQTT broker running, Pi publishes | Subscriber sees one topic update per sensor per cycle | Not yet implemented in dashboard code — this row describes intended behavior, not current state |

**Two rows above need calibration before "expected outcome" is even
meaningful**, not just a build: pH requires the buffer-solution
calibration described earlier, and the flow sensor's pulses-per-litre
constant should be verified against the specific sensor part used, not
assumed from a generic datasheet value.

