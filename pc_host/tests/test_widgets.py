import tempfile
import unittest
from PyQt5.QtWidgets import QApplication
from pc_host.device_state import DeviceState
from pc_host.widgets.control_panel import ControlPanel
from pc_host.widgets.log_panel import LogPanel
from pc_host.widgets.status_bar import StatusBarWidget
from pc_host.widgets.twin_panel import TwinPanel


class ControlPanelTests(unittest.TestCase):
    def test_ping_format_and_mode_buttons_emit_command_requests(self):
        app = QApplication.instance() or QApplication([])
        panel = ControlPanel()
        sent = []
        panel.command_requested.connect(sent.append)
        panel.ping_button.click()
        panel.format_right_button.click()
        panel.mode_day_button.click()
        panel.mode_night_button.click()
        self.assertEqual(sent[0].text, "*PING")
        self.assertFalse(sent[0].requires_ready)
        self.assertEqual(sent[1].text, "*SET:FORMAT RIGHT")
        self.assertEqual(sent[1].followups_on_ok, ("*GET:FORMAT",))
        self.assertEqual(sent[2].text, "*SET:MODE DAY")
        self.assertEqual(sent[3].text, "*SET:MODE NIGHT")

    def test_refresh_ports_keeps_current_selection(self):
        app = QApplication.instance() or QApplication([])
        panel = ControlPanel()
        panel.refresh_ports([("COM3 - USB Serial Device", "COM3"), ("COM4 - XDS110 UART", "COM4")])
        panel.port_combo.setCurrentIndex(1)
        panel.refresh_ports([("COM4 - XDS110 UART", "COM4"), ("COM5 - USB Serial Device", "COM5")])
        self.assertEqual(panel.selected_port(), "COM4")
        self.assertEqual(panel.port_combo.currentText(), "COM4 - XDS110 UART")

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

    def test_demo_and_query_buttons_emit_expected_commands(self):
        app = QApplication.instance() or QApplication([])
        panel = ControlPanel()
        sent = []
        panel.command_requested.connect(sent.append)
        panel.abbrev_demo_button.click()
        panel.mixed_case_demo_button.click()
        panel.get_display_button.click()
        panel.get_format_button.click()
        panel.get_date_button.click()
        panel.get_time_button.click()
        panel.get_alarm_button.click()
        self.assertEqual(
            [request.text for request in sent],
            [
                "*GET:DISP",
                "*gEt:FoRmAt",
                "*GET:DISPLAY",
                "*GET:FORMAT",
                "*GET:DATE",
                "*GET:TIME",
                "*GET:ALARM",
            ],
        )

    def test_virtual_key_layout_matches_board_order(self):
        app = QApplication.instance() or QApplication([])
        panel = ControlPanel()
        self.assertEqual(panel.key_buttons["USER2"].text(), "USER2")
        self.assertEqual(panel.layout().itemAt(3).widget().layout().itemAtPosition(0, 0).widget().text(), "USER2")
        self.assertEqual(panel.layout().itemAt(3).widget().layout().itemAtPosition(0, 5).widget().text(), "DISP")
        self.assertEqual(panel.layout().itemAt(3).widget().layout().itemAtPosition(1, 0).widget().text(), "USER1")
        self.assertEqual(panel.layout().itemAt(3).widget().layout().itemAtPosition(1, 5).widget().text(), "SAVE")
        self.assertIsNotNone(panel.layout().itemAt(3).widget().layout().itemAtPosition(0, 1).widget())


class WidgetTests(unittest.TestCase):
    def test_status_bar_renders_state_text(self):
        app = QApplication.instance() or QApplication([])
        state = DeviceState(connected=True, ready=True, port_name="COM3", format_value="LEFT", mode_value="DAY", alarm_value="OFF", display_enabled="ON", last_rtt_ms=23)
        widget = StatusBarWidget()
        widget.update_state(state)
        self.assertEqual(widget.port_value.text(), "COM3")
        self.assertEqual(widget.connection_value.text(), "已连接")
        self.assertEqual(widget.ready_value.text(), "READY")
        self.assertEqual(widget.format_value.text(), "LEFT")
        self.assertEqual(widget.display_value.text(), "ON")

    def test_log_panel_appends_and_exports(self):
        app = QApplication.instance() or QApplication([])
        panel = LogPanel()
        panel.append_entry("send", "*PING")
        panel.append_entry("reply", "*PONG 7")
        exported = []
        panel.export_requested.connect(lambda: exported.append(True))
        panel.export_button.click()
        self.assertEqual(exported, [True])
        plain_text = panel.text_edit.toPlainText()
        self.assertIn("[TX] *PING", plain_text)
        self.assertIn("[RX] *PONG 7", plain_text)
        with tempfile.NamedTemporaryFile("r+", delete=False) as handle:
            panel.export_to_file(handle.name)
            handle.seek(0)
            self.assertIn("[TX] *PING", handle.read())


class TwinPanelTests(unittest.TestCase):
    def test_twin_panel_renders_digits_leds_decimal_points_and_last_key(self):
        app = QApplication.instance() or QApplication([])
        state = DeviceState(seg_text="12345678", seg_dp_hex="04", led_hex="01", mode_value="DAY", last_key_event="USER1", ready=True)
        panel = TwinPanel()
        panel.update_state(state)
        self.assertEqual(panel.digit_labels[0].text(), "1")
        self.assertEqual(panel.digit_labels[2].text(), "3.")
        self.assertEqual(panel.mode_value.text(), "DAY")
        self.assertEqual(panel.led_labels[0].text(), "●")
        self.assertEqual(panel.led_labels[7].text(), "○")
        self.assertEqual(panel.key_buttons["USER1"].text(), "USER1 *")
        self.assertTrue(panel.key_buttons["USER1"].isEnabled())

    def test_twin_panel_supports_w_and_edit_blink(self):
        app = QApplication.instance() or QApplication([])
        state = DeviceState(seg_text="WTH 2500", seg_dp_hex="00", led_hex="00", mode_value="DAY", format_value="LEFT", edit_mode=1, edit_field=0, ui_now_ms=0, ready=True)
        panel = TwinPanel()
        panel.update_state(state)
        self.assertEqual(panel.digit_labels[2].text(), "H")
        self.assertEqual(panel.digit_labels[0].text(), " ")
        self.assertEqual(panel.digit_labels[1].text(), " ")

    def test_twin_panel_uses_reported_led_state_without_local_override(self):
        app = QApplication.instance() or QApplication([])
        state = DeviceState(led_hex="08", mode_value="NIGHT", ui_now_ms=0, ready=True)
        panel = TwinPanel()
        panel.update_state(state)
        self.assertEqual(panel.led_labels[3].text(), "●")
        self.assertEqual(panel.led_labels[0].text(), "○")
        state.ui_now_ms = 700
        panel.update_state(state)
        self.assertEqual(panel.led_labels[3].text(), "●")
        self.assertEqual(panel.led_labels[0].text(), "○")

    def test_twin_panel_key_layout_matches_board_order(self):
        app = QApplication.instance() or QApplication([])
        panel = TwinPanel()
        self.assertEqual(panel.key_layout.itemAtPosition(0, 0).widget().text(), "USER2")
        self.assertEqual(panel.key_layout.itemAtPosition(0, 5).widget().text(), "DISP")
        self.assertEqual(panel.key_layout.itemAtPosition(1, 0).widget().text(), "USER1")
        self.assertEqual(panel.key_layout.itemAtPosition(1, 5).widget().text(), "SAVE")
        self.assertIsNotNone(panel.key_layout.itemAtPosition(0, 1).widget())

    def test_twin_panel_key_click_emits_request(self):
        app = QApplication.instance() or QApplication([])
        panel = TwinPanel()
        requested = []
        panel.key_requested.connect(requested.append)
        panel.key_buttons["USER2"].click()
        self.assertEqual(requested, ["USER2"])


if __name__ == "__main__":
    unittest.main()
