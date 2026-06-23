# pc_host/commands.py
from dataclasses import dataclass
from typing import Optional

BOOT_GUARD_MS = 10_000
COMMAND_INTERVAL_MS = 500
VALID_KEY_NAMES = (
    "FUNC", "SHIFT", "ADD", "SAVE", "DISP",
    "SPEED", "FORMAT", "EXT", "USER1", "USER2",
)
KEY_LAYOUT_ROWS = (
    ("USER2", "EXT", "FORMAT", "SPEED", "DISP"),
    ("USER1", "FUNC", "SHIFT", "ADD", "SAVE"),
)
ABBREV_DEMO_COMMAND = "*GET:DISP"
MIXED_CASE_DEMO_COMMAND = "*gEt:FoRmAt"
DEMO_PRESETS = (
    ("DATE YEAR", "*SET:DATE YEAR 2026"),
    ("DATE MONTH", "*SET:DATE MONTH 06"),
    ("DATE DATE", "*SET:DATE DATE 18"),
    ("DATE YEAR MONTH", "*SET:DATE YEAR MONTH 2026 06"),
    ("DATE YEAR DATE", "*SET:DATE YEAR DATE 2026 18"),
    ("DATE MONTH DATE", "*SET:DATE MONTH DATE 06 18"),
    ("DATE FULL", "*SET:DATE YEAR MONTH DATE 2026 06 18"),
    ("TIME HOUR", "*SET:TIME HOUR 12"),
    ("TIME MINUTE", "*SET:TIME MINUTE 34"),
    ("TIME SECOND", "*SET:TIME SECOND 56"),
    ("TIME HOUR MINUTE", "*SET:TIME HOUR MINUTE 12 34"),
    ("TIME HOUR SECOND", "*SET:TIME HOUR SECOND 12 56"),
    ("TIME MINUTE SECOND", "*SET:TIME MINUTE SECOND 34 56"),
    ("TIME FULL", "*SET:TIME HOUR MINUTE SECOND 12 34 56"),
    ("ALARM HOUR", "*SET:ALARM HOUR 07"),
    ("ALARM MINUTE", "*SET:ALARM MINUTE 30"),
    ("ALARM SECOND", "*SET:ALARM SECOND 00"),
    ("ALARM HOUR MINUTE", "*SET:ALARM HOUR MINUTE 07 30"),
    ("ALARM HOUR SECOND", "*SET:ALARM HOUR SECOND 07 00"),
    ("ALARM MINUTE SECOND", "*SET:ALARM MINUTE SECOND 30 00"),
    ("ALARM FULL", "*SET:ALARM HOUR MINUTE SECOND 07 30 00"),
    ("ALARM OFF", "*SET:ALARM OFF"),
    ("PING", "*PING"),
)


@dataclass(frozen=True)
class CommandRequest:
    text: str
    requires_ready: bool = True
    followups_on_ok: tuple[str, ...] = ()


def initial_sync_requests() -> list[CommandRequest]:
    # Post-PONG query phase only. The boot-guard ping is sent by CommandScheduler.
    return [
        CommandRequest("*GET:DISPLAY"),
        CommandRequest("*GET:FORMAT"),
        CommandRequest("*GET:DATE"),
        CommandRequest("*GET:TIME"),
        CommandRequest("*GET:ALARM"),
    ]


def query_name_for(command: str) -> Optional[str]:
    if command.startswith("*GET:DISPLAY"):
        return "DISPLAY"
    if command.startswith("*GET:FORMAT"):
        return "FORMAT"
    if command.startswith("*GET:DATE"):
        return "DATE"
    if command.startswith("*GET:TIME"):
        return "TIME"
    if command.startswith("*GET:ALARM"):
        return "ALARM"
    return None


def build_followups(command: str) -> tuple[str, ...]:
    if command == "*RST":
        return tuple(item.text for item in initial_sync_requests())
    if command.startswith("*SET:DATE"):
        return ("*GET:DATE",)
    if command.startswith("*SET:TIME"):
        return ("*GET:TIME",)
    if command.startswith("*SET:ALARM"):
        return ("*GET:ALARM",)
    if command.startswith("*SET:DISPLAY"):
        return ("*GET:DISPLAY",)
    if command.startswith("*SET:FORMAT"):
        return ("*GET:FORMAT",)
    if command == "*SET:KEY DISP":
        return ("*GET:DISPLAY",)
    if command == "*SET:KEY FORMAT":
        return ("*GET:FORMAT",)
    return ()
