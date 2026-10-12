"""Find a virtual observer for which a real ISS pass peaks at a chosen elevation
due north, so the whole pass is centred on azimuth 0.

The observer is placed directly south of the ISS ground track's northernmost
point, where the track runs east-west. Used to build the simulated test pass
for the rig.

Usage:
    python3 tools/find_sim_pass.py tools/data/iss-2026-10-11.tle [target_el_deg]
"""

import sys

import numpy as np
from skyfield.api import EarthSatellite, load, wgs84


def load_tle(path):
    lines = [line.rstrip() for line in open(path) if line.strip()]
    ts = load.timescale()
    return ts, EarthSatellite(lines[1], lines[2], lines[0].strip(), ts)


def pass_for(ts, sat, observer, t_center):
    start = ts.tt_jd(t_center.tt - 12 / 1440)
    end = ts.tt_jd(t_center.tt + 12 / 1440)
    times, events = sat.find_events(observer, start, end, altitude_degrees=0.0)
    if list(events) != [0, 1, 2]:
        return None
    rise, culmination, set_ = times
    samples = ts.tt_jd(rise.tt + np.arange(int((set_ - rise) * 86400) + 1) / 86400)
    alt, az, distance = (sat - observer).at(samples).altaz()
    az_signed = (az.degrees + 180) % 360 - 180  # -180..180, 0 = north
    az_rate = np.abs(np.diff(np.unwrap(np.radians(az.degrees)))) * 180 / np.pi
    return {
        "rise": rise,
        "culmination": culmination,
        "set": set_,
        "max_el": alt.degrees.max(),
        "az_min": az_signed.min(),
        "az_max": az_signed.max(),
        "max_az_rate": az_rate.max(),
        "min_range": distance.km.min(),
    }


def main():
    ts, sat = load_tle(sys.argv[1])
    target_el = float(sys.argv[2]) if len(sys.argv) > 2 else 70.0

    # Northernmost points of the ground track within +/-12 h of the TLE epoch
    jd = np.arange(sat.epoch.tt - 0.5, sat.epoch.tt + 0.5, 5.0 / 86400)
    lat = wgs84.subpoint(sat.at(ts.tt_jd(jd))).latitude.degrees
    peaks = [i for i in range(1, len(lat) - 1) if lat[i - 1] < lat[i] >= lat[i + 1]]

    for i in peaks[:3]:
        t_center = ts.tt_jd(jd[i])
        track = wgs84.subpoint(sat.at(t_center))

        # Bisect how far south of the track the observer must be for the target peak
        near, far = 0.0, 15.0
        for _ in range(40):
            mid = (near + far) / 2
            p = pass_for(ts, sat, wgs84.latlon(track.latitude.degrees - mid, track.longitude.degrees), t_center)
            if p is None or p["max_el"] < target_el:
                far = mid
            else:
                near = mid

        obs_lat, obs_lon = track.latitude.degrees - near, track.longitude.degrees
        p = pass_for(ts, sat, wgs84.latlon(obs_lat, obs_lon), t_center)
        print(f"observer   {obs_lat:.4f}, {obs_lon:.4f}, 0 m")
        print(f"  rise {p['rise'].utc_strftime('%Y-%m-%d %H:%M:%S')}  "
              f"peak {p['culmination'].utc_strftime('%H:%M:%S')}  "
              f"set {p['set'].utc_strftime('%H:%M:%S')} UTC "
              f"({(p['set'] - p['rise']) * 1440:.1f} min)")
        print(f"  max el {p['max_el']:.2f} deg, az {p['az_min']:+.1f} to {p['az_max']:+.1f} deg (0 = north)")
        print(f"  max az rate {p['max_az_rate']:.2f} deg/s, min range {p['min_range']:.0f} km\n")


if __name__ == "__main__":
    main()
