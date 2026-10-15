"""Reference look angles and pass times from Skyfield, used as an independent
oracle for the Sgp4Propagator unit tests.

Prints C++ initializers that are pasted into
IssTracker/Components/Sgp4Propagator/test/ut/OracleData.hpp.

Usage:
    python3 tools/oracle.py tools/data/iss-2026-10-11.tle
"""

import sys
from datetime import datetime, timezone

from skyfield.api import EarthSatellite, load, wgs84

# name, latitude, longitude, altitude (m), pass search start, sample times (UTC)
SCENARIOS = [
    ("VIRTUAL_70", 50.4728, -107.8420, 0.0, "2026-10-10T17:35:00",
     ["2026-10-10T17:41:30", "2026-10-10T17:44:00", "2026-10-10T17:46:50",
      "2026-10-10T17:49:30", "2026-10-10T17:52:00"]),
    ("BUENOS_AIRES_86", -34.60, -58.38, 25.0, "2026-10-11T22:00:00",
     ["2026-10-11T22:10:00", "2026-10-11T22:12:30", "2026-10-11T22:14:27",
      "2026-10-11T22:16:30", "2026-10-11T22:19:00"]),
]


def utc(ts, text):
    return ts.from_datetime(datetime.fromisoformat(text).replace(tzinfo=timezone.utc))


def unix(t):
    return t.utc_datetime().timestamp()


def main():
    lines = [line.rstrip() for line in open(sys.argv[1]) if line.strip()]
    ts = load.timescale()
    sat = EarthSatellite(lines[1], lines[2], lines[0].strip(), ts)

    for name, lat, lon, alt_m, search_start, samples in SCENARIOS:
        observer = wgs84.latlon(lat, lon, elevation_m=alt_m)
        print(f"// {name}: observer {lat}, {lon}, {alt_m} m")
        print(f"const LookSample {name}_SAMPLES[] = {{")
        for text in samples:
            alt, az, distance = (sat - observer).at(utc(ts, text)).altaz()
            print(f'    {{"{text}", {az.degrees:.4f}, {alt.degrees:.4f}, {distance.km:.3f}}},')
        print("};")

        start = utc(ts, search_start)
        times, events = sat.find_events(observer, start, ts.tt_jd(start.tt + 1), altitude_degrees=0.0)
        aos, tca, los = times[:3]
        assert list(events[:3]) == [0, 1, 2]
        max_el = (sat - observer).at(tca).altaz()[0].degrees
        print(f"const PassReference {name}_PASS = {{{unix(aos):.1f}, {unix(tca):.1f}, {unix(los):.1f}, {max_el:.4f}}};")
        print(f"// AOS {aos.utc_strftime('%H:%M:%S.%f')[:-4]}  TCA {tca.utc_strftime('%H:%M:%S.%f')[:-4]}  "
              f"LOS {los.utc_strftime('%H:%M:%S.%f')[:-4]} UTC\n")


if __name__ == "__main__":
    main()
