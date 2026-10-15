"""Download the current ISS TLE from CelesTrak and save it for the deployment.

The file is saved where sgp4Propagator.LOAD_TLE_FILE "tle/iss.tle" finds it
when fprime-gds runs from IssTracker/TrackerDeployment. Both lines are
checked (length and checksum) before anything is written, so a failed
download never replaces a good file.

CelesTrak updates the ISS TLE a few times a day; fetching once before each
tracking session is enough.

Usage:
    python3 tools/fetch_tle.py [output_path]
"""

import sys
import urllib.request
from datetime import datetime, timedelta, timezone
from pathlib import Path

URL = "https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=TLE"
DEFAULT_OUTPUT = Path(__file__).resolve().parent.parent / "IssTracker" / "TrackerDeployment" / "tle" / "iss.tle"
LINE_LENGTH = 69


def checksum_ok(line):
    """TLE checksum: sum of the digits, with each '-' counting as 1, modulo 10."""
    total = sum(int(c) if c.isdigit() else 1 if c == "-" else 0 for c in line[:68])
    return line[68].isdigit() and total % 10 == int(line[68])


def epoch(line1):
    """Epoch from columns 19-32 of line 1: two-digit year and fractional day of year."""
    year = int(line1[18:20])
    year += 2000 if year < 57 else 1900
    day = float(line1[20:32])
    return datetime(year, 1, 1, tzinfo=timezone.utc) + timedelta(days=day - 1)


def main():
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_OUTPUT

    request = urllib.request.Request(URL, headers={"User-Agent": "fprime-labs iss-tracker"})
    with urllib.request.urlopen(request, timeout=20) as response:
        text = response.read().decode("ascii", errors="replace")

    lines = [line.rstrip() for line in text.splitlines() if line.strip()]
    line1 = next((line for line in lines if line.startswith("1 ")), None)
    line2 = next((line for line in lines if line.startswith("2 ")), None)
    if line1 is None or line2 is None:
        sys.exit(f"No TLE in the response:\n{text[:200]}")
    for number, line in ((1, line1), (2, line2)):
        if len(line) != LINE_LENGTH:
            sys.exit(f"Line {number} has {len(line)} characters, expected {LINE_LENGTH}")
        if not checksum_ok(line):
            sys.exit(f"Line {number} fails its checksum")

    output.parent.mkdir(parents=True, exist_ok=True)
    name = lines[0] if not lines[0].startswith("1 ") else "ISS (ZARYA)"
    output.write_text(f"{name}\n{line1}\n{line2}\n")

    tle_epoch = epoch(line1)
    age_hours = (datetime.now(timezone.utc) - tle_epoch).total_seconds() / 3600
    print(f"Saved {output}")
    print(f"Epoch {tle_epoch:%Y-%m-%d %H:%M} UTC ({age_hours:.1f} h old)")


if __name__ == "__main__":
    main()
