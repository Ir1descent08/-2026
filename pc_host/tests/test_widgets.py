import tempfile
import unittest
from PyQt5.QtWidgets import QApplication
from pc_host.device_state import DeviceState
from pc_host.widgets.control_panel import ControlPanel
from pc_host.widgets.log_panel import LogPanel
from pc_host.widgets.status_bar import StatusBarWidget
from pc_host.widgets.twin_panel import TwinPanel


class ControlPanelTests(unittest.TestCase):
    def test_ping_and_format_buttons_emit_command_requests(self):
        app = QApplication.instance() or QApplication([])
        panel = ControlPanel()
        sent = []
        panel.command_requested.connect(sent.append)
        panel.ping_button.click()
        panel.format_right_button.click()
        self.assertEqual(sent[0].text, "*PING")
        self.assertFalse(sent[0].requires_ready)
        self.assertEqual(sent[1].text, "*SET:FORMAT RIGHT")
        self.assertEqual(sent[1].followups_on_ok, ("*GET:FORMAT",))

    def test_virtual_key_and_raw_command_emit_expected_text(self):
        app = QApplication.instance() or QApplication([])
        panel = ControlPanel()
        sent = []
        panel.command_requested.connect(sent.append)
        panel.key_buttons["USER1"].click()
        panel.raw_input.setText("*PING")
        panel.raw_send_button.click()
        self.assertEqual(sent[0].text, "*SET:KEY USER1")
        self.assertEqual(sent[1].text, "*PING")


class WidgetTests(unittest.TestCase):
    def test_status_bar_renders_state_text(self):
        app = QApplication.instance() or QApplication([])
        state = DeviceState(connected=True, ready=True, port_name="COM3", format_value="LEFT", mode_value="DAY", alarm_value="OFF", last_rtt_ms=23)
        widget = StatusBarWidget()
        widget.update_state(state)
        self.assertEqual(widget.port_value.text(), "COM3")
        self.assertEqual(widget.ready_value.text(), "READY")
        self.assertEqual(widget.format_value.text(), "LEFT")

    def test_log_panel_appends_and_exports(self):
        app = QApplication.instance() or QApplication([])
        panel = LogPanel()
        panel.append_entry("send", "*PING")
        panel.append_entry("reply", "*PONG 7")
        with tempfile.NamedTemporaryFile("r+", delete=False) as handle:
            panel.export_to_file(handle.name)
            handle.seek(0)
            self.assertIn("*PING", handle.read())


class TwinPanelTests(unittest.TestCase):
    def test_twin_panel_renders_digits_leds_and_last_key(self):
        app = QApplication.instance() or QApplication([])
        state = DeviceState(seg_text="12345678", seg_dp_hex="04", led_hex="AA", mode_value="DAY", last_key_event="USER1")
        panel = TwinPanel()
        panel.update_state(state)
        self.assertEqual(panel.digit_labels[0].text(), "1")
        self.assertEqual(panel.mode_value.text(), "DAY")
        self.assertEqual(panel.key_labels["USER1"].text(), "USER1 *")


if __name__ == "__main__":
    unittest.main()
