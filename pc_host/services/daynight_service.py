# pc_host/services/daynight_service.py
from astral import Observer
from astral.sun import sun


def compute_mode(observer: Observer, when, timezone_name: str) -> str:
    events = sun(observer, date=when.date(), tzinfo=timezone_name)
    return "DAY" if events["sunrise"] <= when <= events["sunset"] else "NIGHT"
