-- SQLite schema. No server needed - just a file (irrigation.db).
-- Created automatically by app.py and serial_logger.py on first run,
-- but kept here as a reference / for manual setup:
-- sqlite3 irrigation.db < schema.sql

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
);
