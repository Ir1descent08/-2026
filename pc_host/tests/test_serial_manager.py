import unittest
from unittest.mock import patch
from pc_host.serial_manager import PortEntry, SerialManager


class FakePort:
    def __init__(self, chunks=None):
        self._chunks = list(chunks or [b"OK\r\n*PONG 7\r\n"])
        self.in_waiting = len(self._chunks[0]) if self._chunks else 0
        self.written = []
        self.closed = False

    def write(self, payload: bytes) -> None:
        self.written.append(payload)

    def flush(self) -> None:
        return None

    def read(self, size: int) -> bytes:
        if not self._chunks:
            self.in_waiting = 0
            return b""
        chunk = self._chunks.pop(0)
        self.in_waiting = len(self._chunks[0]) if self._chunks else 0
        return chunk

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

    def test_poll_lines_reassembles_partial_chunks(self):
        fake_port = FakePort([b"OK\r", b"\n*PO", b"NG 7\r\n"])
        manager = SerialManager(serial_factory=lambda *args, **kwargs: fake_port)
        manager.open("COM3")
        self.assertEqual(manager.poll_lines(), ["OK", "*PONG 7"])

    def test_list_ports_formats_and_sorts_windows_com_entries(self):
        manager = SerialManager()

        class PortInfo:
            def __init__(self, device, description):
                self.device = device
                self.description = description

        with patch("serial.tools.list_ports.comports", return_value=[
            PortInfo("COM10", "USB Serial Device"),
            PortInfo("COM2", "XDS110 Class Application/User UART"),
        ]):
            entries = manager.list_ports()
        self.assertEqual(entries, [
            PortEntry(label="COM2 - XDS110 Class Application/User UART", device="COM2"),
            PortEntry(label="COM10 - USB Serial Device", device="COM10"),
        ])

    def test_poll_lines_clears_buffer_when_read_fails(self):
        class BrokenPort(FakePort):
            def read(self, size: int) -> bytes:
                raise OSError("device removed")

        fake_port = BrokenPort([b"partial"])
        manager = SerialManager(serial_factory=lambda *args, **kwargs: fake_port)
        manager.open("COM3")
        manager._rx_buffer.extend(b"stale")
        with self.assertRaises(OSError):
            manager.poll_lines()
        self.assertEqual(manager._rx_buffer, bytearray())


if __name__ == "__main__":
    unittest.main()
