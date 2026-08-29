# IoT smart irrigation and monitoring system

A sensor-driven irrigation and environment monitoring system built around
an Arduino and a Raspberry Pi. Soil moisture, temperature, and light are
read continuously and used to control a solenoid valve, a heating/cooling
relay, and a lighting relay. A Raspberry Pi exposes live readings over a
web dashboard.

`Arduino` `C++` `Raspberry Pi` `Python` `Flask` `SQLite` `MQTT (planned)` `Embedded systems`

## Status

This repo covers two layers, and they are at different stages:

- **Core system** - soil moisture, temperature, light, and water level
  sensing, with pump/solenoid/relay control. This was physically built
  and tested as a bachelor's project.
- **Extended design** - humidity, rain, pH, and flow sensing, dual-channel
  temperature control, protection components (flyback diodes, dividers,
  decoupling), and an MQTT/cloud connectivity layer. This is designed and
  documented (schematic-level and in firmware) but **not yet physically
  built or tested**. It's flagged as such throughout this README and in
  `docs/`.

## Features

- Soil-moisture-triggered irrigation via a solenoid valve
- Rain and humidity used as overrides, so irrigation doesn't run
  needlessly (extended design)
- Temperature-based heating and cooling on two independent relay channels
- Light-based LED/relay control
- Water tank level detection via a 4-probe ladder, with automatic refill
- Water usage tracking via a flow sensor (extended design)
- Local web dashboard on the Raspberry Pi; MQTT/cloud publishing planned
  (extended design)

## System architecture

The system is organized into four layers: sensing, control, actuation,
and connectivity. See `docs/architecture.png` for the full block diagram
and `docs/circuit-diagram.png` for wiring.

```
Sensors -> Microcontroller (decision logic) -> Actuators
                    |
              Raspberry Pi -> Dashboard / MQTT / Cloud
```

## Hardware

| Component | Purpose | Status |
|---|---|---|
| Soil moisture sensor | Triggers irrigation | Built & tested |
| NTC temperature sensor | Drives heating/cooling | Built & tested |
| LDR (light sensor) | Drives lighting relay | Built & tested |
| Water level probes (4-stage) | Tank refill logic | Built & tested |
| Solenoid valve | Field irrigation | Built & tested |
| Relay modules | Switch loads | Built & tested |
| Water pump + motor driver | Fills tank | Built & tested |
| Raspberry Pi 3 (hardware + serial link) | Local dashboard host | Built & tested |
| DHT22 (humidity) | Irrigation override | Designed, not built |
| Rain sensor | Irrigation override | Designed, not built |
| Soil pH sensor | Nutrient monitoring | Designed, not built |
| Water flow sensor | Usage tracking | Designed, not built |
| Flyback diodes | Protects transistors from inductive spikes | Designed correction |
| Voltage divider resistors | Required for LDR and NTC readings | Designed correction |
| Decoupling capacitor | Protects MCU from switching noise | Designed correction |

Full pin-level wiring is in `docs/connection-map.md`.

**Note on the dashboard software specifically:** the original project's
dashboard was written in PHP/Apache/MySQL and was part of the tested
build. It's since been rewritten in Python (Flask/SQLite) for a simpler,
single-language stack — that rewrite has been tested locally with sample
data but not deployed against the real Arduino/Pi hardware. The Pi
hardware and serial connection themselves were part of the original
tested build; the current dashboard *code* is new.

## How it works

See `docs/working-mechanism.md` for the full technical explainer,
including the irrigation override logic, the NTC-to-Celsius conversion,
and the water level and flow subsystems.

## Repository structure

```
iot-smart-irrigation-system/
├── README.md
├── firmware/
│   └── irrigation_controller.ino
├── raspberrypi/
│   ├── dashboard/
│   │   ├── app.py
│   │   ├── serial_logger.py
│   │   └── templates/
│   │       └── index.html
│   └── setup.md
├── docs/
│   ├── working-mechanism.md
│   ├── connection-map.md
│   ├── architecture.png
│   └── circuit-diagram.png
├── results/
│   └── test-log.md
└── LICENSE
```

## Setup

1. Flash `firmware/irrigation_controller.ino` to the Arduino (Arduino IDE
   or `arduino-cli`).
2. Wire components per `docs/connection-map.md`.
3. On the Raspberry Pi, install Python dependencies and run the dashboard
   (Flask + SQLite, no Apache/MySQL needed) — see `raspberrypi/setup.md`.
4. Connect Arduino to the Pi via USB for serial data.
5. Power on and confirm sensor readings appear on the dashboard.

## Results

From the original built-and-tested core system: the tank refill sequence
was verified across all four probe stages, with the pump activating on
empty and shutting off at full. Soil-moisture-triggered irrigation and
light/temperature-driven relay switching were verified to respond
correctly to sensor thresholds during testing. See `results/test-log.md`
for details.

The extended sensors and connectivity layer have not been tested, since
they exist only at the design/firmware level.

## Known limitations

- Extended sensors (humidity, rain, pH, flow) are unverified — pH in
  particular requires physical calibration against buffer solutions
  before its readings mean anything.
- The dashboard (Flask/SQLite) was rewritten from the original tested
  PHP/MySQL version and has only been verified locally with sample data,
  not against the real hardware end-to-end.
- Local dashboard only; MQTT/cloud publishing is designed but not wired
  up in the current dashboard code.
- No data logging analysis or charts yet — readings are stored over
  time in SQLite, but the dashboard only shows the latest row.

## License

MIT
