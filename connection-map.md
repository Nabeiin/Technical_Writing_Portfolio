# Connection map

Pin-level wiring for every component, grouped by layer. Components marked
"designed" have not been physically built or tested — see the README's
Status section.

## Sensing layer

| Component | Wiring | Support part |
|---|---|---|
| Soil moisture sensor | VCC->5V, GND->GND, AO->A0 | - |
| Temperature sensor (NTC) | One leg->5V, other leg->A1 and->10k->GND (divider) | 10k divider resistor |
| LDR | One leg->5V, other leg->A2 and->10k->GND (divider) | 10k divider resistor |
| LDR -> relay path | Divider junction -> transistor base | 1k base resistor |
| Water level probes (4x) | Common probe->5V, each sense probe->D4-D7 | Pull-down resistor per probe |
| DHT22 (designed) | VCC->5V, GND->GND, DATA->D11 | 10k pull-up on DATA line |
| Rain sensor (designed) | VCC->5V, GND->GND, AO->A3 | - |
| Soil pH sensor (designed) | VCC->5V, GND->GND, AO->A4 | Calibrate against pH 4/7 buffers before use |
| Water flow sensor (designed) | VCC->5V, GND->GND, pulse out->D3 (interrupt pin) | - |

## Control layer

| Component | Wiring | Support part |
|---|---|---|
| Microcontroller (Arduino) | Central hub for all sensors/actuators | - |
| Decoupling capacitor (designed) | Across 5V/GND, close to MCU power pins | 0.1uF ceramic |
| 5V voltage regulator | Between 12V supply and low-voltage circuitry | - |

## Actuation layer

| Component | Wiring | Support part |
|---|---|---|
| Water pump | Via motor driver (higher current than MCU pins supply) | Motor driver IC |
| Relay (light/fan load) | Coil driven by transistor from LDR/temp logic | Flyback diode (1N4007), reverse-biased across coil |
| Solenoid valve | Powered via relay contact from moisture logic | Flyback diode across solenoid coil |
| Indicator LEDs | In series between pin/probe output and GND | 220-330 ohm resistor per LED |
| Switching transistor | Base driven from sensor logic | Base resistor (1k) |

## Connectivity layer

| Link | Notes |
|---|---|
| Arduino <-> Raspberry Pi | USB serial (Arduino's USB-to-serial chip into Pi's USB port). Avoids the 5V (Arduino) vs 3.3V (Pi GPIO) logic mismatch entirely. |
| Arduino <-> Pi, GPIO alternative | Only if wiring directly to Pi GPIO pins: put a voltage divider (1k + 2k) on the Arduino->Pi line. Pi->Arduino direction is safe without a divider (3.3V still reads as HIGH on Arduino). |
| Raspberry Pi -> MQTT broker (designed) | Over Wi-Fi/Ethernet; broker can run locally on the Pi or in the cloud |
| MQTT broker -> Cloud dashboard (designed) | Internet-facing, e.g. ThingSpeak/Blynk/Node-RED |
