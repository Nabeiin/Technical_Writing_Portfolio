# Raspberry Pi dashboard setup (Python-only: Flask + SQLite)

**Status: designed, not deployed.** Untested end-to-end; treat as a
starting point.

## 1. Install dependencies

```bash
sudo apt-get update
sudo apt-get install -y python3-pip
pip3 install flask pyserial
```

No Apache, no MySQL server, no PHP — Flask serves the page itself, and
SQLite is just a file (`irrigation.db`) created automatically on first
run. One language, two small scripts.

## 2. Confirm the Arduino's serial device name

```bash
ls /dev/tty*
```

Update `SERIAL_PORT` in `serial_logger.py` if it's not `/dev/ttyUSB0`.

## 3. Run the serial logger

This is the process that actually pulls data from the Arduino and
writes it to `irrigation.db`. Creates the database and table
automatically on first run.

```bash
python3 serial_logger.py
```

Run it as a background service so it survives reboots and restarts on
crash:

```ini
# /etc/systemd/system/irrigation-logger.service
[Unit]
Description=Irrigation system serial logger
After=network.target

[Service]
ExecStart=/usr/bin/python3 /home/pi/dashboard/serial_logger.py
WorkingDirectory=/home/pi/dashboard
Restart=always
User=pi

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable irrigation-logger
sudo systemctl start irrigation-logger
```

## 4. Run the dashboard

```bash
flask --app app run --host=0.0.0.0
```

Visit `http://<raspberry-pi-ip>:5000/` from any device on the same
network. For a persistent service, use the same systemd pattern as
above with `ExecStart=/usr/bin/flask --app app run --host=0.0.0.0`, or
run it behind `gunicorn` for production use.

## 5. Verify

Check the dashboard shows a "Last updated" timestamp within the last
few seconds. If it says "No readings yet," check the logger's terminal
output or service status, and confirm the Arduino is actually connected
via USB.

## Not yet implemented

MQTT publishing (cloud dashboard layer) — see the README's Status
section. This setup only covers the local dashboard.
