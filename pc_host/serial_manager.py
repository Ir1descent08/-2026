# pc_host/serial_manager.py
import serial
import serial.tools.list_ports


class SerialManager:
    def __init__(self, serial_factory=None) -> None:
        self._serial_factory = serial_factory or serial.Serial
        self._port = None
        self.port_name = ""

    @property
    def is_open(self) -> bool:
        return self._port is not None

    def list_ports(self) -> list[str]:
        return [item.device for item in serial.tools.list_ports.comports()]

    def open(self, port_name: str, baudrate: int = 115200) -> None:
        self.close()
        self._port = self._serial_factory(port_name, baudrate=baudrate, timeout=0)
        self.port_name = port_name

    def close(self) -> None:
        if self._port is not None:
            self._port.close()
        self._port = None
        self.port_name = ""

    def send_line(self, text: str) -> None:
        if self._port is None:
            raise RuntimeError("serial port is not open")
        self._port.write((text + "\r\n").encode("ascii"))
        self._port.flush()

    def poll_lines(self) -> list[str]:
        if self._port is None:
            return []
        lines = []
        while getattr(self._port, "in_waiting", 0):
            raw = self._port.readline()
            if not raw:
                break
            lines.append(raw.decode("ascii", errors="replace").strip())
        return [line for line in lines if line]
