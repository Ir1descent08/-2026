import tempfile
import unittest
from PyQt5.QtWidgets import QApplication
from pc_host.device_state import DeviceState
from pc_host.widgets.log_panel import LogPanel
from pc_host.widgets.status_bar import StatusBarWidget


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


if __name__ == "__main__":
    unittest.main()
