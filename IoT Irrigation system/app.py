"""
app.py - Flask dashboard
-------------------------
Replaces the PHP/Apache version. Queries the latest row from the
SQLite database and renders it. Run with:
    flask --app app run --host=0.0.0.0
"""

import sqlite3
from flask import Flask, render_template, g

DB_PATH = "irrigation.db"

app = Flask(__name__)


def get_db():
    if "db" not in g:
        g.db = sqlite3.connect(DB_PATH)
        g.db.row_factory = sqlite3.Row
    return g.db


@app.teardown_appcontext
def close_db(exception):
    db = g.pop("db", None)
    if db is not None:
        db.close()


@app.route("/")
def dashboard():
    db = get_db()
    row = db.execute(
        "SELECT * FROM sensor_readings ORDER BY reading_time DESC LIMIT 1"
    ).fetchone()
    return render_template("index.html", reading=row)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
