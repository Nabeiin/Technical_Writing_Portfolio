# Working mechanism

This document explains how each subsystem makes its decisions. For pin-level
wiring, see `connection-map.md`. For the full firmware, see
`../firmware/irrigation_controller.ino`.

## Overview

The system reads soil moisture, temperature, light, water level, humidity,
rain, soil pH, and water flow. Each sensor feeds a threshold-based (or,
for water level and flow, count-based) decision that drives one actuator.
The irrigation path checks rain and humidity as overrides before acting.
A separate section below covers the small support and protection
components (resistors, capacitor, diodes) that don't sense or actuate
anything but the system doesn't work correctly without.

## Sequential flow: sensing -> control -> actuation -> connectivity

The sections below explain each subsystem individually, but it's worth
stating the pipeline explicitly, since that's the actual architecture:

1. **Sensing layer.** All eight sensors (soil moisture, temperature,
   light, water level, humidity, rain, pH, flow) take a physical
   quantity — resistance, capacitance, a voltage, a pulse train — and
   turn it into a value the microcontroller can read on an analog or
   digital pin. Nothing here makes a decision; this layer only measures.

2. **Control layer.** The microcontroller reads every sensor value once
   per loop cycle, applies the threshold or override logic described
   below (dry soil, but check rain and humidity first; too hot or too
   cold, on separate channels; count wet probes), and decides what each
   actuator should do. The voltage regulator and decoupling capacitor
   here don't participate in that decision-making — they keep the
   control layer electrically stable while it happens (see Support and
   protection components).

3. **Actuation layer.** The microcontroller's decisions are carried out
   physically: the solenoid opens or stays shut, the pump runs or stops,
   the heating or cooling relay energizes, the light relay switches. Each
   of these is a higher-current or higher-voltage load than the
   microcontroller can drive directly, which is why relays, a motor
   driver, and a switching transistor sit between the control layer and
   the actual hardware — and why the flyback diodes exist at this
   boundary specifically.

4. **Connectivity layer.** Independent of the control loop above, the
   microcontroller also writes one line of sensor readings to serial each
   cycle. The Raspberry Pi picks that line up, serves it on a local
   dashboard, and — in the designed extension — would publish it to an
   MQTT broker for remote/cloud access. This layer only reports on what
   the other three layers are doing; it has no influence over the
   irrigation, temperature, or lighting decisions themselves.

Layers 1-3 form a closed loop that repeats every cycle. Layer 4 is a
one-way tap off that loop — it observes, it doesn't act back into the
system.

## Soil moisture -> irrigation

**Sensing principle:** the probe pair acts as a variable resistor. Dry soil
is a poor conductor, so resistance is high and the sensor's analog output
is low. Wet soil conducts well, so resistance drops and the output rises.

**Signal path:** analog pin A0, range 0-1023.

**Control logic:** the soil reading alone is not enough to open the
solenoid. Three conditions are checked in order:

1. Is the soil dry (above `SOIL_DRY_THRESHOLD`)?
2. If dry — is rain currently detected? If yes, skip irrigation; the rain
   will do the job.
3. If no rain — is ambient humidity above `HUMIDITY_HIGH_PCT`? If yes,
   skip irrigation; high humidity plus already-adequate moisture risks
   fungal disease.

Only if soil is dry, there's no rain, and humidity isn't already high does
the solenoid open. This override chain is designed content — the original
build only checked soil moisture directly.

**Actuator response:** solenoid valve opens (relay energized) or stays
closed.

## Temperature -> heating/cooling

**Sensing principle:** the NTC thermistor's resistance falls as temperature
rises (negative temperature coefficient). Reading it through a voltage
divider produces a value the ADC can interpret; converting that to an
actual Celsius value requires the beta-equation approximation, not a
straight-line formula, since the thermistor's resistance-temperature curve
is nonlinear.

**Signal path:** analog pin A1, converted in `readTemperatureC()`.

**Control logic:** two independent comparisons — `tempC > TEMP_HIGH_C`
(too hot) and `tempC < TEMP_LOW_C` (too cold). These can never both be
true, since they compare against different thresholds.

**Actuator response:** two separate relay channels, `PIN_COOLING` and
`PIN_HEATING`. The original design used one shared relay pin for both
loads, which cannot actually distinguish "too hot" from "too cold" — this
was corrected in the firmware.

## Humidity (DHT22) -> irrigation override input (designed extension)

**What it measures:** ambient relative humidity, plus temperature as a
secondary reading (unused here since the NTC already covers temperature).

**Sensing principle:** the DHT22 contains a capacitive humidity sensing
element — a moisture-absorbing dielectric between two electrodes, whose
capacitance changes with humidity. Unlike the analog sensors elsewhere in
this system, the DHT22 does the analog-to-digital conversion internally
and sends a finished reading out over a single data line using a
timing-based protocol (bit values are encoded as pulse durations, not
voltage levels).

**Signal path:** digital pin D11, read via the DHT library, which handles
the timing protocol.

**Why it's here:** soil moisture alone can't tell you whether watering is
a good idea right now. High ambient humidity combined with already-wet
soil raises fungal disease risk rather than helping the crop — this
sensor gives the irrigation logic (see above) the second input it needs
to make that call.

## Rain detection -> irrigation override input (designed extension)

**What it measures:** whether rain is currently falling.

**Sensing principle:** works on the same principle as the soil moisture
sensor — a resistive plate exposed to open air. Water bridging the plate's
traces lowers resistance, raising the analog output; a dry plate reads
low.

**Signal path:** analog pin A3.

**Why it's here:** without it, the irrigation logic would open the
solenoid on a dry-soil reading even while rain was actively falling on the
same field — wasted water and an obviously wrong decision a farmer would
never make manually. This sensor lets the override chain treat rain as a
hard stop, checked before humidity.



**Sensing principle:** the LDR's resistance falls as light increases. Read
through a voltage divider (a bare LDR produces no usable voltage on its
own).

**Signal path:** analog pin A2.

**Control logic:** single comparison against `LIGHT_LOW_THRESHOLD`.

**Actuator response:** light relay on when below threshold, off otherwise.

## Water level -> tank refill pump

Unlike the sensors above, this isn't a threshold check — it's a count.
Four probes are placed at increasing heights in the tank. Water is
conductive, so each submerged probe closes its own circuit, read as a
digital HIGH. The number of HIGH probes indicates how full the tank is
(0 = empty, 4 = full — the original build used a 9-probe ladder for finer
resolution; this firmware simplifies to 4 stages, documented in code).

**Actuator response:** pump runs while stage is below the "full" count,
stops once all probes are wet.

## Soil pH -> nutrient monitoring (designed extension)

**What it measures:** soil acidity/alkalinity, which governs how well
crops can actually absorb the nutrients already in the soil — even
correctly watered, fertilized soil underperforms outside the right pH
range for a given crop.

**Sensing principle:** an ion-selective glass electrode generates a small
voltage proportional to hydrogen ion concentration in the surrounding
soil solution. That voltage is amplified by the probe module into a range
the ADC can read.

**Signal path:** analog pin A4, converted in `readPH()`.

**Why calibration is non-negotiable here:** every other analog sensor in
this system reads a physical quantity fairly directly — soil moisture and
light both map roughly linearly from raw ADC value to a usable threshold.
pH doesn't. The relationship between the electrode's output voltage and
actual pH is sensor-specific and drifts over time, so the probe must be
calibrated against known pH 4 and pH 7 buffer solutions before its
readings mean anything. The slope/offset values in the firmware are
placeholders for exactly this reason — they're not a substitute for doing
that calibration.

## Water flow -> usage tracking (designed extension)

The flow sensor outputs a pulse per unit volume passed. Because pulses
arrive faster than the main loop runs, they're counted via a hardware
interrupt (`flowISR`) rather than a regular read. Once per second, the
accumulated pulse count is divided by the sensor's calibration constant
(`FLOW_PULSES_PER_LITRE`) and added to a running total. This is the only
subsystem that produces a cumulative value rather than a live decision.

## Support and protection components

These parts don't sense or actuate anything themselves, but the system
doesn't work correctly — or survive — without them. Grouped here since
they're easy to treat as an afterthought, which is exactly how they got
left out of the original report.

**LDR and NTC voltage divider resistors (10k each)**
*What:* a fixed resistor in series with the LDR/thermistor, forming a
voltage divider. *How:* neither device outputs a voltage on its own —
they only change resistance. Placing a fixed resistor in series and
reading the midpoint voltage is what converts "changing resistance" into
"a voltage the ADC can read." *Why 10k:* chosen to be close to the
sensor's resistance at typical operating conditions, which keeps the
divider sensitive in the range that actually matters.

**Transistor base resistor (1k)**
*What:* a resistor between the LDR's divider output and the switching
transistor's base. *How:* limits current into the base — without it, the
transistor draws excessive current and either burns out or fails to
switch cleanly. *Why:* any time a low-power signal (like the LDR reading)
switches a higher-power load (the relay) through a transistor, the base
needs current limiting.

**Flyback diode (1N4007), across the relay coil and the solenoid coil**
*What:* a diode placed in reverse bias directly across each inductive
load. *How:* while the coil is energized, the diode is reverse-biased and
does nothing. When power is cut, the collapsing magnetic field induces a
voltage spike (back-EMF) that can be many times the supply voltage; the
diode gives that spike a safe path to dissipate through the coil instead
of arcing through the switching transistor. *Why it matters:* this was
missing from the original report entirely. Any inductive load — a relay
coil, a solenoid, a motor — needs this. Skipping it is how switching
transistors get destroyed during testing, often without an obvious cause.

**LED current-limiting resistors (220-330 ohm)**
*What:* a resistor in series with each indicator LED. *How:* LEDs have
very low internal resistance, so connected directly across a 5V supply
they draw far more current than they're rated for and burn out almost
immediately. The resistor limits current to a safe level. *Why 220-330
ohm:* a reasonable range for a standard LED at 5V; exact value depends on
the LED's forward voltage and desired brightness.

**DHT22 pull-up resistor (10k)**
*What:* a resistor between the DHT22's data line and 5V. *How:* the
DHT22's one-wire protocol needs the line to sit at a defined HIGH state
between transmissions; without a pull-up, the line floats and reads are
unreliable. *Why:* standard requirement for this class of digital sensor,
called out in the datasheet.

**Decoupling capacitor (0.1uF ceramic), near the microcontroller's power
pins**
*What:* a small capacitor across Vcc and GND, placed physically close to
the MCU. *How:* absorbs high-frequency electrical noise generated when
nearby relays switch, preventing that noise from causing the MCU to reset
or misread a sensor at the moment a relay clicks. *Why:* standard practice
on any board switching inductive loads near sensitive digital logic —
cheap insurance against intermittent, hard-to-diagnose faults.

**Voltage regulator (5V output), between the 12V supply and the
low-voltage circuitry**
*What:* a component that takes the higher supply voltage used by relays
and the pump (12V) and steps it down to a stable 5V for the
microcontroller and sensors. *How:* it continuously compares its output
against a fixed reference and adjusts to hold that output steady even as
the input fluctuates, dissipating the difference as heat (which is why
it's typically paired with a small heat sink). *Why:* without it, the
12V rail that powers the pump/relays could find its way into circuitry
built for 5V — through a wiring mistake, a shared ground fault, or simply
running the wrong wire to the wrong pin — and destroy the microcontroller
or sensors. It's the boundary that keeps the high-current and
low-current halves of the circuit from taking each other down.

**Serial line protection (Arduino <-> Raspberry Pi)**
*What:* either a USB connection (preferred) or a voltage divider on the
Arduino-to-Pi line if wiring directly to GPIO. *How:* Arduino outputs 5V
logic; Raspberry Pi GPIO pins are 3.3V-only and can be damaged by a direct
5V input. USB serial sidesteps the issue entirely since it goes through
the Arduino's onboard USB-to-serial chip, not raw GPIO. *Why it's called
out:* this is a real way to damage a Raspberry Pi, not a theoretical
concern — worth deciding explicitly rather than wiring GPIO-to-GPIO by
default.



The Arduino sends one CSV line of readings over USB serial each cycle.
The Raspberry Pi reads this line and serves it on a local dashboard via
Apache/PHP. Publishing to an MQTT broker for remote/cloud access is
designed but not implemented in the current dashboard code — see the
README's "Known limitations."
