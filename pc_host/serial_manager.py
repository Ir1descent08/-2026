# pc_host/serial_manager.py
from dataclasses import dataclass
import serial
import serial.tools.list_ports


@dataclass(frozen=True)
class PortEntry:
    label: str
    device: str


class SerialManager:
    def __init__(self, serial_factory=None) -> None:
        self._serial_factory = serial_factory or serial.Serial
        self._port = None
        self.port_name = ""
        self._rx_buffer = bytearray()

    @property
    def is_open(self) -> bool:
        return self._port is not None

    def list_ports(self) -> list[PortEntry]:
        def sort_key(item):
            device = getattr(item, "device", "") or ""
            if device.upper().startswith("COM") and device[3:].isdigit():
                return (0, int(device[3:]))
            return (1, device.upper())

        entries = []
        for item in sorted(serial.tools.list_ports.comports(), key=sort_key):
            device = item.device
            description = (getattr(item, "description", "") or "").strip()
            label = device if (not description or description == device) else f"{device} - {description}"
            entries.append(PortEntry(label=label, device=device))
        return entries

    def open(self, port_name: str, baudrate: int = 115200) -> None:
        self.close()
        self._port = self._serial_factory(port_name, baudrate=baudrate, timeout=0)
        self.port_name = port_name
        self._rx_buffer = bytearray()

    def close(self) -> None:
        if self._port is not None:
            self._port.close()
        self._port = None
        self.port_name = ""
        self._rx_buffer = bytearray()

    def send_line(self, text: str) -> None:
        if self._port is None:
            raise RuntimeError("serial port is not open")
        self._port.write((text + "\r\n").encode("ascii"))
        self._port.flush()

    def poll_lines(self) -> list[str]:
        if self._port is None:
            return []
        try:
            waiting = getattr(self._port, "in_waiting", 0)
            while waiting:
                chunk = self._port.read(waiting)
                if not chunk:
                    break
                self._rx_buffer.extend(chunk)
                waiting = getattr(self._port, "in_waiting", 0)
        except (serial.SerialException, OSError):
            self._rx_buffer = bytearray()
            raise

        lines = []
        while b"\n" in self._rx_buffer:
            raw_line, _sep, remainder = self._rx_buffer.partition(b"\n")
            self._rx_buffer = bytearray(remainder)
            line = raw_line.rstrip(b"\r").decode("ascii", errors="replace").strip()
            if line:
                lines.append(line)
        return lines
