import csv
import tempfile
import unittest
from unittest.mock import MagicMock, patch
from PyQt5.QtWidgets import QApplication
from pc_host.main_window import MainWindow
from pc_host.serial_manager import PortEntry, SerialManager


class FakeSerialManager(SerialManager):
    def __init__(self):
        super().__init__(serial_factory=lambda *args, **kwargs: object())
        self.sent = []
        self._open = False
        self._lines = []
        self.port_name = ""
        self.available_ports = [PortEntry(label="COM3 - USB Serial Device", device="COM3")]
        self.open_error = None
        self.send_error = None
        self.poll_error = None

    @property
    def is_open(self):
        return self._open

    def open(self, port_name: str, baudrate: int = 115200) -> None:
        if self.open_error is not None:
            raise self.open_error
        self._open = True
        self.port_name = port_name

    def close(self) -> None:
        self._open = False
        self.port_name = ""

    def send_line(self, text: str) -> None:
        if self.send_error is not None:
            raise self.send_error
        self.sent.append(text)

    def poll_lines(self) -> list[str]:
        if self.poll_error is not None:
            raise self.poll_error
        lines, self._lines = self._lines, []
        return lines

    def list_ports(self):
        return list(self.available_ports)


class MainWindowFlowTests(unittest.TestCase):
    def test_auto_mode_button_enables_auto_mode_and_polls_once(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        with patch.object(window, "poll_auto_mode") as poll_auto_mode:
            window.control_panel.auto_mode_button.click()
        self.assertTrue(window.auto_mode_enabled)
        poll_auto_mode.assert_called_once_with()

    def test_pong_marks_ready_and_updates_status(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.connect_selected_port("COM3")
        window.scheduler.tick()
        window.process_incoming_line("*PONG 12")
        self.assertTrue(window.state.ready)
        self.assertEqual(window.status_bar.ready_value.text(), "READY")
        self.assertEqual(window.status_bar.connection_value.text(), "已连接")

    def test_refresh_ports_keeps_selected_port(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.control_panel.refresh_ports([
            PortEntry(label="COM3 - USB Serial Device", device="COM3"),
            PortEntry(label="COM4 - XDS110 UART", device="COM4"),
        ])
        window.control_panel.port_combo.setCurrentIndex(1)
        serial_manager.available_ports = [
            PortEntry(label="COM4 - XDS110 UART", device="COM4"),
            PortEntry(label="COM5 - USB Serial Device", device="COM5"),
        ]
        window.refresh_ports()
        self.assertEqual(window.control_panel.selected_port(), "COM4")

    def test_twin_panel_key_click_enqueues_virtual_key_command(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.state.ready = True
        window.refresh_ui()
        window.twin_panel.key_buttons["USER1"].click()
        self.assertEqual(window.scheduler._queue[-1].text, "*SET:KEY USER1")

    def test_user1_event_triggers_ntp_sync(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        with patch.object(window, "run_ntp_sync") as run_ntp_sync:
            window.process_incoming_line("*EVT:KEY USER1")
        run_ntp_sync.assert_called_once_with()

    def test_event_history_persists_key_mode_and_alarm_events(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        with tempfile.NamedTemporaryFile("w+", delete=False, suffix=".csv") as handle:
            window.history_csv = type(window.history_csv)(handle.name)
        with patch.object(window, "run_ntp_sync"):
            window.process_incoming_line("*EVT:KEY USER1")
        window.process_incoming_line("*EVT:MODE NIGHT")
        window.process_incoming_line("*EVT:ALARM")
        window.process_incoming_line("*EVT:ALARM_OFF")
        with open(window.history_csv, "r", encoding="utf-8") as handle:
            rows = list(csv.reader(handle))
        self.assertEqual([row[1] for row in rows], ["key", "mode", "alarm", "alarm_off"])
        self.assertEqual([row[2] for row in rows], ["USER1", "NIGHT", "", ""])

    def test_weather_fetch_enqueues_weather_command(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.state.ready = True
        with patch.dict("sys.modules", {
            "pc_host.services.weather_service": type("WeatherModule", (), {"fetch_weather": staticmethod(lambda session, location: (31, "Sunny"))})(),
            "pc_host.services.chart_service": type("ChartModule", (), {"append_history_row": staticmethod(lambda path, event_name, value: None)})(),
        }):
            window.run_weather_fetch()
        self.assertEqual(window.scheduler._queue[-1].text, "*SET:WEATHER 31 5")

    def test_weather_fetch_clamps_negative_temperature(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.state.ready = True
        with patch.dict("sys.modules", {
            "pc_host.services.weather_service": type("WeatherModule", (), {"fetch_weather": staticmethod(lambda session, location: (-3, "Snow"))})(),
            "pc_host.services.chart_service": type("ChartModule", (), {"append_history_row": staticmethod(lambda path, event_name, value: None)})(),
        }):
            window.run_weather_fetch()
        self.assertEqual(window.scheduler._queue[-1].text, "*SET:WEATHER 0 0")

    def test_run_auto_mode_does_not_remember_blocked_mode(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window._auto_mode_last = "DAY"
        with patch.dict("sys.modules", {
            "pc_host.services.daynight_service": type("DaynightModule", (), {"compute_mode": staticmethod(lambda observer, when, timezone_name: "NIGHT")})(),
        }):
            mode = window.run_auto_mode(object(), object(), "Asia/Shanghai")
        self.assertIsNone(mode)
        self.assertEqual(window._auto_mode_last, "DAY")

    def test_disconnect_clears_auto_mode_last(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window._auto_mode_last = "NIGHT"
        window.disconnect_serial()
        self.assertEqual(window._auto_mode_last, "")

    def test_connect_selected_port_logs_open_error(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        serial_manager.open_error = RuntimeError("busy")
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.connect_selected_port("COM3")
        self.assertFalse(window.state.connected)
        self.assertIn("open serial failed", window.log_panel.text_edit.toPlainText())

    def test_unknown_line_logs_error(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.process_incoming_line("garbage")
        self.assertIn("unknown line", window.log_panel.text_edit.toPlainText())

    def test_export_log_writes_file(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.log_panel.append_entry("send", "*PING")
        with patch("pc_host.main_window.QFileDialog.getSaveFileName", return_value=("C:/temp/pc_host_log.txt", "Text Files (*.txt)")), \
             patch.object(window.log_panel, "export_to_file") as export_to_file:
            window.export_log()
        export_to_file.assert_called_once_with("C:/temp/pc_host_log.txt")

    def test_export_history_chart_uses_selected_path(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        close_figure = MagicMock()
        fake_matplotlib = type("MatplotlibModule", (), {"pyplot": type("PyplotModule", (), {"close": close_figure})()})()
        with patch("pathlib.Path.exists", return_value=True), \
             patch("pc_host.main_window.QFileDialog.getSaveFileName", return_value=("C:/temp/out.png", "PNG Files (*.png)")), \
             patch("pc_host.services.chart_service.build_history_figure") as build_history_figure, \
             patch.dict("sys.modules", {"matplotlib": fake_matplotlib}):
            figure = MagicMock()
            build_history_figure.return_value = figure
            window.export_history_chart()
        figure.savefig.assert_called_once_with("C:/temp/out.png")
        close_figure.assert_called_once_with(figure)

    def test_poll_serial_logs_error_and_disconnects_on_read_failure(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        serial_manager.poll_error = OSError("device removed")
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.state.connected = True
        window.state.port_name = "COM3"
        window.poll_serial()
        self.assertFalse(window.state.connected)
        self.assertIn("serial read failed", window.log_panel.text_edit.toPlainText())


if __name__ == "__main__":
    unittest.main()
