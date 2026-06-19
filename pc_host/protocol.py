# pc_host/protocol.py
from dataclasses import dataclass


@dataclass(frozen=True)
class ParsedMessage:
    kind: str
    raw: str
    payload: str = ""
    event_name: str = ""
    uptime_s: int = 0


def parse_line(line: str) -> ParsedMessage:
    text = line.strip()
    if text.startswith("*PONG "):
        parts = text.split()
        try:
            uptime = int(parts[1]) if len(parts) > 1 else 0
        except ValueError:
            uptime = 0
        return ParsedMessage(kind="pong", raw=text, uptime_s=uptime)
    if text.startswith("ERROR"):
        return ParsedMessage(kind="error", raw=text, payload=text[6:].strip())
    if text.startswith("OK"):
        payload = text[3:] if text.startswith("OK ") else ""
        return ParsedMessage(kind="ok", raw=text, payload=payload)
    if text.startswith("*EVT:"):
        tail = text[5:]
        parts = tail.split(" ", 1)
        event_name = parts[0]
        payload = parts[1] if len(parts) == 2 else ""
        return ParsedMessage(kind="event", raw=text, event_name=event_name, payload=payload)
    return ParsedMessage(kind="unknown", raw=text)
