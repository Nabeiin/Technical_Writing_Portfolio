#!/usr/bin/env python3
"""
serial_logger.py
-----------------
Reads one CSV line per second from the Arduino over USB serial and
inserts it into the sensor_readings table (SQLite). Run this as a
background service (see setup.md) - it's the piece that actually gets
data from the microcontroller into the database; the Flask app only
reads what's already there, it doesn't listen to the serial port.

CSV field order must match the Serial.print() sequence in
firmware/irrigation_controller.ino exactly (11 fields):
soil, temp, light, humidity, rain, ph, water_level, litres, irrigating,
too_hot, too_cold
"""

import serial
import sqlite3
import time

SERIAL_PORT = "/dev/ttyUSB0"   # confirm with: ls /dev/tty*
BAUD_RATE = 9600
DB_PATH = "irrigation.db"

INSERT_QUERY = """
    INSERT INTO sensor_readings
    (soil_moisture, temperature_c, light_level, humidity_pct, rain_detected,
     ph_value, water_level_stage, total_litres, solenoid_open, cooling_on, heating_on)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
"""

CREATE_TABLE = """
    CREATE TABLE IF NOT EXISTS sensor_readings (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        reading_time TEXT NOT NULL DEFAULT (datetime('now', 'localtime')),
        soil_moisture INTEGER NOT NULL,
        temperature_c REAL NOT NULL,
        light_level INTEGER NOT NULL,
        humidity_pct REAL,
        rain_detected INTEGER,
        ph_value REAL,
        water_level_stage INTEGER NOT NULL,
        total_litres REAL,
        solenoid_open INTEGER NOT NULL,
        cooling_on INTEGER NOT NULL,
        heating_on INTEGER NOT NULL
    )
"""


def parse_line(line):
    parts = line.strip().split(",")
    if len(parts) != 11:
        return None  # malformed line, e.g. a boot message - skip it
    (soil, temp, light, humidity, rain, ph, water_level, litres,
     irrigating, too_hot, too_cold) = parts
    return (
        int(soil), float(temp), int(light),
        float(humidity) if humidity != "nan" else None,
        int(bool(int(rain))), float(ph), int(water_level), float(litres),
        int(bool(int(irrigating))), int(bool(int(too_hot))), int(bool(int(too_cold))),
    )


def main():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=5)
    db = sqlite3.connect(DB_PATH)
    db.execute(CREATE_TABLE)
    db.commit()

    print(f"Listening on {SERIAL_PORT} at {BAUD_RATE} baud, writing to {DB_PATH}...")

    while True:
        try:
            raw_line = ser.readline().decode("utf-8", errors="ignore")
            values = parse_line(raw_line)
            if values is None:
                continue

            db.execute(INSERT_QUERY, values)
            db.commit()

        except (ValueError, UnicodeDecodeError):
            continue  # skip a bad line rather than crashing the logger
        except sqlite3.Error as db_err:
            print(f"Database error: {db_err}")
            time.sleep(5)  # brief backoff before retrying


if __name__ == "__main__":
    main()
