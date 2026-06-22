# pc_host/main_window.py
import time
from pathlib import Path
from typing import Callable, Optional
import requests
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QFileDialog, QHBoxLayout, QMainWindow, QVBoxLayout, QWidget
from pc_host.command_scheduler import CommandScheduler
from pc_host.commands import CommandRequest, query_name_for
from pc_host.device_state import DeviceState
from pc_host.protocol import parse_line
from pc_host.serial_manager import SerialManager
from pc_host.widgets.control_panel import ControlPanel
from pc_host.widgets.log_panel import LogPanel
from pc_host.widgets.status_bar import StatusBarWidget
from pc_host.widgets.twin_panel import TwinPanel


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
        self.twin_panel = TwinPanel()
        self.log_panel = LogPanel()

        self.history_csv = Path(__file__).parent / "pc_host_history.csv"
        self.log_txt = Path(__file__).parent / "pc_host_log.txt"
        self.auto_mode_enabled = False
        self._auto_mode_last = ""

        self.control_panel.command_requested.connect(self.handle_command_request)
        self.control_panel.connect_requested.connect(self.connect_selected_port)
        self.control_panel.disconnect_requested.connect(self.disconnect_serial)
        self.control_panel.refresh_requested.connect(self.refresh_ports)
        self.control_panel.ntp_requested.connect(self.run_ntp_sync)
        self.control_panel.auto_mode_requested.connect(self._on_auto_mode_requested)
        self.control_panel.weather_requested.connect(self.run_weather_fetch)
        self.control_panel.chart_requested.connect(self.export_history_chart)
        self.log_panel.export_requested.connect(self.export_log)
        self.twin_panel.key_requested.connect(lambda key: self.handle_command_request(CommandRequest(f"*SET:KEY {key}")))

        top_row = QHBoxLayout()
        top_row.addWidget(self.control_panel, 3)
        top_row.addWidget(self.twin_panel, 5)
        outer.addWidget(self.status_bar)
        outer.addLayout(top_row)
        outer.addWidget(self.log_panel, 2)

        self.serial_timer = QTimer(self)
        self.serial_timer.timeout.connect(self.poll_serial)
        self.serial_timer.start(50)
        self.scheduler_timer = QTimer(self)
        self.scheduler_timer.timeout.connect(self.scheduler.tick)
        self.scheduler_timer.start(50)
        self.port_refresh_timer = QTimer(self)
        self.port_refresh_timer.timeout.connect(self.refresh_ports)
        self.port_refresh_timer.start(1000)
        self.auto_mode_timer = QTimer(self)
        self.auto_mode_timer.timeout.connect(self.poll_auto_mode)
        self.auto_mode_timer.start(60_000)
        self.refresh_ports()
        self.refresh_ui()

    def refresh_ports(self) -> None:
        self.control_panel.refresh_ports(self.serial_manager.list_ports())

    def connect_selected_port(self, port_name: str) -> None:
        if not port_name:
            self.log_panel.append_entry("warn", "no serial port selected")
            return
        try:
            self.serial_manager.open(port_name)
        except Exception as exc:
            self.log_panel.append_entry("error", f"open serial failed: {exc}")
            self.state.connected = False
            self.state.ready = False
            self.state.port_name = ""
            self.refresh_ui()
            return
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
        self.state.last_rtt_ms = 0
        self._auto_mode_last = ""
        self.log_panel.append_entry("warn", "serial disconnected")
        self.refresh_ui()

    def handle_command_request(self, request: CommandRequest) -> bool:
        if request.requires_ready and not self.state.ready:
            self.log_panel.append_entry("warn", f"blocked before ready: {request.text}")
            return False
        self.state.apply_shadow_from_command(request.text)
        self.scheduler.enqueue(request)
        self.refresh_ui()
        return True

    def _send_command(self, text: str) -> None:
        try:
            self.serial_manager.send_line(text)
        except Exception as exc:
            self.log_panel.append_entry("error", f"send failed: {exc}")
            self.disconnect_serial()
            return
        self.log_panel.append_entry("send", text)

    def poll_serial(self) -> None:
        try:
            lines = self.serial_manager.poll_lines()
        except Exception as exc:
            self.log_panel.append_entry("error", f"serial read failed: {exc}")
            self.disconnect_serial()
            return
        for line in lines:
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
            if (message.event_name == "KEY") and (message.payload == "USER1"):
                self.run_ntp_sync()
        else:
            self.log_panel.append_entry("error", f"unknown line: {line}")
        self.refresh_ui()

    def refresh_ui(self) -> None:
        self.control_panel.set_connected(self.state.connected)
        self.control_panel.set_ready_enabled(self.state.ready)
        self.status_bar.update_state(self.state)
        self.twin_panel.update_state(self.state)

    def run_ntp_sync(self) -> None:
        from pc_host.services.chart_service import append_history_row
        from pc_host.services.ntp_service import build_sync_requests, fetch_ntp_datetime

        try:
            moment = fetch_ntp_datetime()
        except Exception as exc:
            self.log_panel.append_entry("error", f"ntp sync failed: {exc}")
            return
        append_history_row(str(self.history_csv), "ntp_sync", moment.isoformat(timespec="seconds"))
        for request in build_sync_requests(moment):
            self.handle_command_request(request)

    def run_auto_mode(self, observer, when, timezone_name: str) -> Optional[str]:
        from pc_host.services.daynight_service import compute_mode

        mode = compute_mode(observer, when, timezone_name)
        if mode == self._auto_mode_last:
            return mode
        if self.handle_command_request(CommandRequest(f"*SET:MODE {mode}")):
            return mode
        return None

    def _on_auto_mode_requested(self) -> None:
        self.auto_mode_enabled = True
        self.poll_auto_mode()

    def run_weather_fetch(self, location: str = "Shanghai") -> None:
        from pc_host.services.chart_service import append_history_row
        from pc_host.services.weather_service import fetch_weather

        try:
            temp_c, condition = fetch_weather(requests.Session(), location)
        except Exception as exc:
            self.log_panel.append_entry("error", f"weather fetch failed: {exc}")
            return
        flags = 0
        condition_upper = condition.upper()
        if any(token in condition_upper for token in ("SUN", "CLEAR")):
            flags |= 0x01
        if any(token in condition_upper for token in ("RAIN", "SHOWER", "DRIZZLE", "THUNDER")):
            flags |= 0x02
        if temp_c >= 30:
            flags |= 0x04
        encoded_temp = max(0, min(99, temp_c))
        append_history_row(str(self.history_csv), "weather", f"{temp_c}:{condition}")
        if self.handle_command_request(CommandRequest(f"*SET:WEATHER {encoded_temp} {flags}")):
            self.log_panel.append_entry("reply", f"weather {temp_c}C {condition}")
        else:
            self.log_panel.append_entry("warn", f"weather fetched {temp_c}C {condition}, device not ready")

    def poll_auto_mode(self) -> None:
        if not self.auto_mode_enabled:
            return
        try:
            from datetime import datetime, timezone
            from astral import Observer
        except Exception as exc:
            self.log_panel.append_entry("error", f"auto mode unavailable: {exc}")
            self.auto_mode_enabled = False
            return
        observer = Observer(latitude=31.2304, longitude=121.4737)
        when = datetime.now(timezone.utc)
        mode = self.run_auto_mode(observer, when, "Asia/Shanghai")
        if mode is not None:
            self._auto_mode_last = mode

    def export_log(self) -> None:
        path, _selected = QFileDialog.getSaveFileName(self, "导出日志", str(self.log_txt), "Text Files (*.txt);;All Files (*)")
        if not path:
            return
        self.log_panel.export_to_file(path)
        self.log_panel.append_entry("reply", f"saved {Path(path).name}")

    def export_history_chart(self):
        from pc_host.services.chart_service import build_history_figure

        if not self.history_csv.exists():
            self.log_panel.append_entry("error", "no history CSV to export")
            return
        path, _selected = QFileDialog.getSaveFileName(self, "导出图表", str(Path.cwd() / "pc_host_history.png"), "PNG Files (*.png);;All Files (*)")
        if not path:
            return
        figure = build_history_figure(str(self.history_csv))
        try:
            figure.savefig(path)
        finally:
            try:
                from matplotlib import pyplot as plt
                plt.close(figure)
            except Exception:
                pass
        self.log_panel.append_entry("reply", f"saved {Path(path).name}")
