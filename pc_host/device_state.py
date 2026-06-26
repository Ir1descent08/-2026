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
    game_state: str = "IDLE"
    game_round_index: int = 0
    game_total_rounds: int = 5
    game_target_key: str = ""
    game_last_result_ms: int = 0
    game_best_result_ms: int = 0
    game_avg_result_ms: int = 0
    game_success_count: int = 0
    game_last_outcome: str = ""
    game_sum_result_ms: int = 0

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
        elif query_name == "GAME":
            if payload == "IDLE":
                if self.game_last_outcome in ("STOP", "DONE"):
                    self.game_state = "IDLE"
                    self.game_round_index = 0
                    self.game_total_rounds = 5
                    self.game_target_key = ""
                    self.game_last_result_ms = 0
                else:
                    self.reset_game_state()
                return
            parts = payload.split()
            if not parts:
                return
            self.game_state = parts[0]
            if (parts[0] == "WAIT") and (len(parts) >= 2) and ("/" in parts[1]):
                current, total = parts[1].split("/", 1)
                self.game_round_index = int(current)
                self.game_total_rounds = int(total)
                self.game_target_key = ""
                self.game_last_outcome = ""
            elif (parts[0] == "GO") and (len(parts) >= 3) and ("/" in parts[1]):
                current, total = parts[1].split("/", 1)
                self.game_round_index = int(current)
                self.game_total_rounds = int(total)
                self.game_target_key = parts[2]
            elif (parts[0] == "RESULT") and (len(parts) >= 3) and ("/" in parts[1]):
                current, total = parts[1].split("/", 1)
                self.game_round_index = int(current)
                self.game_total_rounds = int(total)
                self.game_last_result_ms = int(parts[2])
            elif (parts[0] == "DONE") and (len(parts) >= 5):
                self.game_state = "DONE"
                self.game_round_index = int(parts[1])
                self.game_total_rounds = self.game_round_index
                self.game_target_key = ""
                self.game_last_outcome = "DONE"
                self.game_success_count = int(parts[2])
                self.game_best_result_ms = int(parts[3])
                self.game_avg_result_ms = int(parts[4])
                self.game_sum_result_ms = self.game_avg_result_ms * self.game_success_count

    def apply_shadow_from_command(self, command: str) -> None:
        if command == "*RST":
            self.display_enabled = "ON"
            self.format_value = "LEFT"
            self.alarm_value = "OFF"
            self.mode_value = "DAY"
            self.reset_game_state()
            return
        if command.startswith("*SET:MODE "):
            self.mode_value = command.rsplit(" ", 1)[1]
        if command.startswith("*SET:FORMAT "):
            self.format_value = command.rsplit(" ", 1)[1]
        if command.startswith("*SET:DISPLAY "):
            self.display_enabled = command.rsplit(" ", 1)[1]
        if command == "*SET:ALARM OFF":
            self.alarm_value = "OFF"
        if command == "*SET:GAME START":
            self.reset_game_state()
            self.game_state = "WAIT"
        if command == "*SET:GAME STOP":
            self.reset_game_state()

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
        elif event_name == "GAME":
            self.apply_game_event(payload)

    def reset_game_state(self) -> None:
        self.game_state = "IDLE"
        self.game_round_index = 0
        self.game_total_rounds = 5
        self.game_target_key = ""
        self.game_last_result_ms = 0
        self.game_best_result_ms = 0
        self.game_avg_result_ms = 0
        self.game_success_count = 0
        self.game_last_outcome = ""
        self.game_sum_result_ms = 0

    def apply_game_event(self, payload: str) -> None:
        parts = payload.split()
        if not parts:
            return

        action = parts[0]
        if action == "START":
            self.reset_game_state()
            self.game_state = "WAIT"
            return
        if (action == "READY") and (len(parts) >= 3):
            self.game_state = "GO"
            self.game_round_index = int(parts[1])
            self.game_target_key = parts[2]
            return
        if (action == "HIT") and (len(parts) >= 4):
            self.game_state = "RESULT"
            self.game_last_outcome = "HIT"
            self.game_round_index = int(parts[1])
            self.game_target_key = parts[2]
            self.game_last_result_ms = int(parts[3])
            self.game_success_count += 1
            self.game_sum_result_ms += self.game_last_result_ms
            self.game_avg_result_ms = self.game_sum_result_ms // self.game_success_count
            if (self.game_best_result_ms == 0) or (self.game_last_result_ms < self.game_best_result_ms):
                self.game_best_result_ms = self.game_last_result_ms
            return
        if (action == "MISS") and (len(parts) >= 4):
            self.game_state = "RESULT"
            self.game_last_outcome = "MISS"
            self.game_round_index = int(parts[1])
            self.game_target_key = parts[2]
            self.game_last_result_ms = 0
            return
        if (action == "TIMEOUT") and (len(parts) >= 3):
            self.game_state = "RESULT"
            self.game_last_outcome = "TIMEOUT"
            self.game_round_index = int(parts[1])
            self.game_target_key = parts[2]
            self.game_last_result_ms = 0
            return
        if (action == "DONE") and (len(parts) >= 5):
            self.game_state = "DONE"
            self.game_round_index = int(parts[1])
            self.game_total_rounds = self.game_round_index
            self.game_target_key = ""
            self.game_last_outcome = "DONE"
            self.game_success_count = int(parts[2])
            self.game_best_result_ms = int(parts[3])
            self.game_avg_result_ms = int(parts[4])
            self.game_sum_result_ms = self.game_avg_result_ms * self.game_success_count
            return
        if action == "STOP":
            self.reset_game_state()
            self.game_last_outcome = "STOP"
            return
        if action == "IDLE":
            self.reset_game_state()

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
