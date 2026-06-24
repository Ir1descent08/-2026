# pc_host/device_state.py
from dataclasses import dataclass


EDIT_TIMEOUT_MS = 5000


@dataclass
class DeviceState:
    port_name: str = ""
    connected: bool = False
    ready: bool = False
    display_enabled: str = "UNKNOWN"
    format_value: str = "UNKNOWN"
    alarm_value: str = "UNKNOWN"
    mode_value: str = "UNKNOWN"
    date_value: str = "UNKNOWN"
    time_value: str = "UNKNOWN"
    led_hex: str = "00"
    seg_text: str = "        "
    seg_dp_hex: str = "00"
    last_key_event: str = ""
    last_error: str = ""
    last_status: str = ""
    last_uptime_s: int = 0
    last_rtt_ms: int = 0
    ui_now_ms: int = 0
    edit_mode: int = 0
    edit_field: int = 0
    edit_deadline_ms: int = 0

    def apply_query_result(self, query_name: str, payload: str) -> None:
        if query_name == "DISPLAY":
            self.display_enabled = payload
        elif query_name == "FORMAT":
            self.format_value = payload
        elif query_name == "ALARM":
            self.alarm_value = payload
        elif query_name == "MODE":
            self.mode_value = payload
        elif query_name == "DATE":
            self.date_value = payload
        elif query_name == "TIME":
            self.time_value = payload

    def apply_shadow_from_command(self, command: str) -> None:
        if command == "*RST":
            self.display_enabled = "ON"
            self.format_value = "LEFT"
            self.alarm_value = "OFF"
            self.mode_value = "DAY"
            return
        if command.startswith("*SET:MODE "):
            self.mode_value = command.rsplit(" ", 1)[1]
        if command.startswith("*SET:FORMAT "):
            self.format_value = command.rsplit(" ", 1)[1]
        if command.startswith("*SET:DISPLAY "):
            self.display_enabled = command.rsplit(" ", 1)[1]
        if command == "*SET:ALARM OFF":
            self.alarm_value = "OFF"
        if command.startswith("*SET:LED "):
            self.led_hex = command.rsplit(" ", 1)[1]

    def apply_event(self, event_name: str, payload: str) -> None:
        if event_name == "KEY":
            self.last_key_event = payload
        elif event_name == "DISP" and len(payload) >= 10:
            self.seg_text = payload[:8]
            self.seg_dp_hex = payload[-2:]
        elif event_name == "LED":
            self.led_hex = payload[-2:]
        elif event_name == "MODE":
            self.mode_value = payload

    def apply_key_shadow(self, key_name: str, now_ms: int) -> None:
        self.last_key_event = key_name
        if key_name == "FUNC":
            if self.edit_mode == 0:
                self.edit_mode = 1
            else:
                self.edit_mode += 1
                if self.edit_mode > 3:
                    self.edit_mode = 0
            self.edit_field = 0
        elif (key_name == "SHIFT") and (self.edit_mode != 0):
            self.edit_field = (self.edit_field + 1) % 3
        elif (key_name == "SAVE") and (self.edit_mode != 0):
            self.edit_mode = 0
            self.edit_field = 0

        if (key_name in ("FUNC", "SHIFT", "ADD")) and (self.edit_mode != 0):
            self.edit_deadline_ms = now_ms + EDIT_TIMEOUT_MS
        elif self.edit_mode == 0:
            self.edit_deadline_ms = 0

    def expire_transient_state(self, now_ms: int) -> None:
        self.ui_now_ms = now_ms
        if (self.edit_mode != 0) and (self.edit_deadline_ms != 0) and (now_ms >= self.edit_deadline_ms):
            self.edit_mode = 0
            self.edit_field = 0
            self.edit_deadline_ms = 0
