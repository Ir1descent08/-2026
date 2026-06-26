import unittest
from pc_host.device_state import DeviceState


class DeviceStateTests(unittest.TestCase):
    def test_apply_query_result_updates_known_fields(self):
        state = DeviceState()
        state.apply_query_result("FORMAT", "LEFT")
        state.apply_query_result("DISPLAY", "ON")
        self.assertEqual(state.format_value, "LEFT")
        self.assertEqual(state.display_enabled, "ON")

    def test_apply_shadow_tracks_mode_and_reset_defaults(self):
        state = DeviceState()
        state.apply_shadow_from_command("*SET:MODE NIGHT")
        state.apply_shadow_from_command("*SET:LED AA")
        state.apply_shadow_from_command("*SET:FORMAT RIGHT")
        self.assertEqual(state.led_hex, "00")
        state.apply_shadow_from_command("*RST")
        self.assertEqual(state.mode_value, "DAY")
        self.assertEqual(state.format_value, "LEFT")
        self.assertEqual(state.display_enabled, "ON")
        self.assertEqual(state.alarm_value, "OFF")

    def test_apply_display_event_splits_text_and_dp_mask(self):
        state = DeviceState()
        state.apply_event("DISP", "1234567804")
        self.assertEqual(state.seg_text, "12345678")
        self.assertEqual(state.seg_dp_hex, "04")

    def test_key_event_only_updates_last_key(self):
        state = DeviceState()
        state.apply_event("KEY", "FUNC")
        self.assertEqual(state.last_key_event, "FUNC")
        self.assertEqual(state.seg_text, "        ")

    def test_apply_key_shadow_tracks_edit_state_and_timeout(self):
        state = DeviceState()
        state.apply_key_shadow("FUNC", 1000)
        self.assertEqual(state.edit_mode, 1)
        self.assertEqual(state.edit_field, 0)
        state.apply_key_shadow("SHIFT", 1200)
        self.assertEqual(state.edit_field, 1)
        state.expire_transient_state(7000)
        self.assertEqual(state.edit_mode, 0)
        self.assertEqual(state.edit_field, 0)
        self.assertEqual(state.ui_now_ms, 7000)

    def test_save_key_exits_edit_state(self):
        state = DeviceState(edit_mode=2, edit_field=1, edit_deadline_ms=9000)
        state.apply_key_shadow("SAVE", 2000)
        self.assertEqual(state.edit_mode, 0)
        self.assertEqual(state.edit_field, 0)
        self.assertEqual(state.edit_deadline_ms, 0)

    def test_game_events_update_state(self):
        state = DeviceState()
        state.apply_event("GAME", "START")
        self.assertEqual(state.game_state, "WAIT")
        state.apply_event("GAME", "READY 2 FUNC")
        self.assertEqual(state.game_state, "GO")
        self.assertEqual(state.game_round_index, 2)
        self.assertEqual(state.game_target_key, "FUNC")
        state.apply_event("GAME", "HIT 2 FUNC 284")
        self.assertEqual(state.game_last_outcome, "HIT")
        self.assertEqual(state.game_last_result_ms, 284)
        self.assertEqual(state.game_best_result_ms, 284)
        self.assertEqual(state.game_success_count, 1)
        self.assertEqual(state.game_avg_result_ms, 284)
        state.apply_event("GAME", "HIT 3 SHIFT 316")
        self.assertEqual(state.game_success_count, 2)
        self.assertEqual(state.game_avg_result_ms, 300)
        state.apply_event("GAME", "DONE 5 4 231 308")
        self.assertEqual(state.game_state, "DONE")
        self.assertEqual(state.game_success_count, 4)
        self.assertEqual(state.game_avg_result_ms, 308)

    def test_game_shadow_updates_for_start_and_stop(self):
        state = DeviceState()
        state.apply_shadow_from_command("*SET:GAME START")
        self.assertEqual(state.game_state, "WAIT")
        state.apply_shadow_from_command("*SET:GAME STOP")
        self.assertEqual(state.game_state, "IDLE")


if __name__ == "__main__":
    unittest.main()
