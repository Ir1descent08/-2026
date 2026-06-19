import unittest
from PyQt5.QtWidgets import QApplication
from pc_host.main_window import MainWindow
from pc_host.serial_manager import SerialManager


class FakeSerialManager(SerialManager):
    def __init__(self):
        super().__init__(serial_factory=lambda *args, **kwargs: object())
        self.sent = []
        self._open = False
        self._lines = []
        self.port_name = ""

    @property
    def is_open(self):
        return self._open

    def open(self, port_name: str, baudrate: int = 115200) -> None:
        self._open = True
        self.port_name = port_name

    def close(self) -> None:
        self._open = False
        self.port_name = ""

    def send_line(self, text: str) -> None:
        self.sent.append(text)

    def poll_lines(self) -> list[str]:
        lines, self._lines = self._lines, []
        return lines


class MainWindowFlowTests(unittest.TestCase):
    def test_auto_mode_button_enqueues_set_mode_command(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.connect_selected_port("COM3")
        window.scheduler.tick()
        window.process_incoming_line("*PONG 12")  # mark ready
        window.control_panel.auto_mode_button.click()
        # Check the scheduler queue contains a SET:MODE command
        queue_texts = [req.text for req in window.scheduler._queue]
        mode_commands = [c for c in queue_texts if c.startswith("*SET:MODE")]
        self.assertEqual(len(mode_commands), 1)
        self.assertIn(mode_commands[0], ["*SET:MODE DAY", "*SET:MODE NIGHT"])

    def test_pong_marks_ready_and_updates_status(self):
        app = QApplication.instance() or QApplication([])
        serial_manager = FakeSerialManager()
        window = MainWindow(serial_manager=serial_manager, now_ms=lambda: 10_000)
        window.connect_selected_port("COM3")
        window.scheduler.tick()
        window.process_incoming_line("*PONG 12")
        self.assertTrue(window.state.ready)
        self.assertEqual(window.status_bar.ready_value.text(), "READY")


if __name__ == "__main__":
    unittest.main()
