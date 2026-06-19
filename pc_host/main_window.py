# pc_host/main_window.py
import time
from typing import Callable, Optional
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QGroupBox, QHBoxLayout, QMainWindow, QVBoxLayout, QWidget
from pc_host.command_scheduler import CommandScheduler
from pc_host.commands import CommandRequest, query_name_for
from pc_host.device_state import DeviceState
from pc_host.protocol import parse_line
from pc_host.serial_manager import SerialManager
from pc_host.widgets.control_panel import ControlPanel
from pc_host.widgets.log_panel import LogPanel
from pc_host.widgets.status_bar import StatusBarWidget


class MainWindow(QMainWindow):
    def __init__(self, serial_manager: Optional[SerialManager] = None, now_ms: Optional[Callable[[], int]] = None) -> None:
        super().__init__()
        self.setWindowTitle("PC Host")
        self.serial_manager = serial_manager or SerialManager()
        self.state = DeviceState()
        self._now_ms = now_ms or (lambda: int(time.monotonic() * 1000))
        self.scheduler = CommandScheduler(self._send_command, self._now_ms)

        root = QWidget()
        self.setCentralWidget(root)
        outer = QVBoxLayout(root)
        self.status_bar = StatusBarWidget()
        self.control_panel = ControlPanel()
        self.twin_placeholder = QGroupBox("右侧数字孪生")
        twin_layout = QVBoxLayout(self.twin_placeholder)
        twin_layout.addWidget(QWidget())
        self.log_panel = LogPanel()

        self.control_panel.command_requested.connect(self.handle_command_request)
        self.control_panel.connect_requested.connect(self.connect_selected_port)
        self.control_panel.disconnect_requested.connect(self.disconnect_serial)
        self.control_panel.refresh_requested.connect(self.refresh_ports)

        top_row = QHBoxLayout()
        top_row.addWidget(self.control_panel, 3)
        top_row.addWidget(self.twin_placeholder, 5)
        outer.addWidget(self.status_bar)
        outer.addLayout(top_row)
        outer.addWidget(self.log_panel, 2)

        self.serial_timer = QTimer(self)
        self.serial_timer.timeout.connect(self.poll_serial)
        self.serial_timer.start(50)
        self.scheduler_timer = QTimer(self)
        self.scheduler_timer.timeout.connect(self.scheduler.tick)
        self.scheduler_timer.start(50)
        self.refresh_ports()
        self.refresh_ui()

    def refresh_ports(self) -> None:
        self.control_panel.refresh_ports(self.serial_manager.list_ports())

    def connect_selected_port(self, port_name: str) -> None:
        if not port_name:
            self.log_panel.append_entry("warn", "no serial port selected")
            return
        self.serial_manager.open(port_name)
        self.state.connected = True
        self.state.ready = False
        self.state.port_name = port_name
        self.scheduler.on_port_opened()
        self.log_panel.append_entry("warn", f"connected {port_name}, waiting ready")
        self.refresh_ui()

    def disconnect_serial(self) -> None:
        self.serial_manager.close()
        self.scheduler.on_port_closed()
        self.state.connected = False
        self.state.ready = False
        self.state.port_name = ""
        self.log_panel.append_entry("warn", "serial disconnected")
        self.refresh_ui()

    def handle_command_request(self, request: CommandRequest) -> None:
        if request.requires_ready and not self.state.ready:
            self.log_panel.append_entry("warn", f"blocked before ready: {request.text}")
            return
        self.state.apply_shadow_from_command(request.text)
        self.scheduler.enqueue(request)
        self.refresh_ui()

    def _send_command(self, text: str) -> None:
        self.serial_manager.send_line(text)
        self.log_panel.append_entry("send", text)

    def poll_serial(self) -> None:
        for line in self.serial_manager.poll_lines():
            self.process_incoming_line(line)

    def process_incoming_line(self, line: str) -> None:
        message = parse_line(line)
        current_request = self.scheduler.current_request_text()
        if message.kind == "pong":
            self.scheduler.handle_reply(message)
            self.state.ready = self.scheduler.ready
            self.state.last_uptime_s = message.uptime_s
            self.state.last_rtt_ms = self.scheduler.last_rtt_ms
            self.log_panel.append_entry("reply", line)
        elif message.kind == "ok":
            query_name = query_name_for(current_request or "")
            if query_name is not None:
                self.state.apply_query_result(query_name, message.payload)
            self.scheduler.handle_reply(message)
            self.log_panel.append_entry("reply", line)
        elif message.kind == "error":
            self.state.last_error = message.payload
            self.scheduler.handle_reply(message)
            self.log_panel.append_entry("error", line)
        elif message.kind == "event":
            self.state.apply_event(message.event_name, message.payload)
            self.log_panel.append_entry("event", line)
        self.refresh_ui()

    def refresh_ui(self) -> None:
        self.control_panel.set_connected(self.state.connected)
        self.control_panel.set_ready_enabled(self.state.ready)
        self.status_bar.update_state(self.state)
