# pc_host/services/weather_service.py
from urllib.parse import urljoin


def fetch_weather(session, location: str, base_url: str = "https://wttr.in") -> tuple[int, str]:
    response = session.get(urljoin(base_url, f"/{location}"), params={"format": "j1"}, timeout=5)
    response.raise_for_status()
    payload = response.json()["current_condition"][0]
    return int(payload["temp_C"]), payload["weatherDesc"][0]["value"]
