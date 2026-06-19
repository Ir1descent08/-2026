import unittest
from pc_host.serial_manager import SerialManager


class FakePort:
    def __init__(self):
        self.in_waiting = 2
        self._lines = [b"OK\r\n", b"*PONG 7\r\n"]
        self.written = []
        self.closed = False

    def write(self, payload: bytes) -> None:
        self.written.append(payload)

    def flush(self) -> None:
        return None

    def readline(self) -> bytes:
        if not self._lines:
            self.in_waiting = 0
            return b""
        line = self._lines.pop(0)
        self.in_waiting = len(self._lines)
        return line

    def close(self) -> None:
        self.closed = True


class SerialManagerTests(unittest.TestCase):
    def test_send_line_appends_crlf(self):
        fake_port = FakePort()
        manager = SerialManager(serial_factory=lambda *args, **kwargs: fake_port)
        manager.open("COM3")
        manager.send_line("*PING")
        self.assertEqual(fake_port.written[-1], b"*PING\r\n")

    def test_poll_lines_decodes_nonblocking_input(self):
        fake_port = FakePort()
        manager = SerialManager(serial_factory=lambda *args, **kwargs: fake_port)
        manager.open("COM3")
        self.assertEqual(manager.poll_lines(), ["OK", "*PONG 7"])


if __name__ == "__main__":
    unittest.main()
