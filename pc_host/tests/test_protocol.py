import unittest
from pc_host.commands import VALID_KEY_NAMES, build_followups, initial_sync_requests
from pc_host.device_state import DeviceState
from pc_host.protocol import parse_line


class ProtocolTests(unittest.TestCase):
    def test_parse_pong(self):
        message = parse_line("*PONG 42")
        self.assertEqual(message.kind, "pong")
        self.assertEqual(message.uptime_s, 42)

    def test_parse_pong_malformed_non_numeric(self):
        message = parse_line("*PONG garbage")
        self.assertEqual(message.kind, "pong")
        self.assertEqual(message.uptime_s, 0)

    def test_parse_pong_malformed_with_extra_text(self):
        # Uptime token is not numeric (e.g., corrupted data)
        message = parse_line("*PONG 12abc")
        self.assertEqual(message.kind, "pong")
        self.assertEqual(message.uptime_s, 0)

    def test_parse_key_event(self):
        message = parse_line("*EVT:KEY USER1")
        self.assertEqual(message.kind, "event")
        self.assertEqual(message.event_name, "KEY")
        self.assertEqual(message.payload, "USER1")


class CommandRulesTests(unittest.TestCase):
    def test_key_list_has_10_values(self):
        self.assertEqual(len(VALID_KEY_NAMES), 10)
        self.assertIn("USER2", VALID_KEY_NAMES)

    def test_followups_cover_stateful_commands(self):
        self.assertEqual(build_followups("*SET:FORMAT RIGHT"), ("*GET:FORMAT",))
        self.assertEqual(build_followups("*SET:DISPLAY OFF"), ("*GET:DISPLAY",))
        self.assertEqual(
            [item.text for item in initial_sync_requests()],
            ["*GET:DISPLAY", "*GET:FORMAT", "*GET:DATE", "*GET:TIME", "*GET:ALARM"],
        )


if __name__ == "__main__":
    unittest.main()
