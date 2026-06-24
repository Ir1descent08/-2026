# pc_host/widgets/control_panel.py
from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import QComboBox, QFormLayout, QFrame, QGridLayout, QGroupBox, QHBoxLayout, QLineEdit, QPushButton, QVBoxLayout, QWidget
from pc_host.commands import ABBREV_DEMO_COMMAND, CommandRequest, DEMO_PRESETS, KEY_LAYOUT_ROWS, MIXED_CASE_DEMO_COMMAND, build_followups


class ControlPanel(QWidget):
    command_requested = pyqtSignal(object)
    connect_requested = pyqtSignal(str)
    disconnect_requested = pyqtSignal()
    refresh_requested = pyqtSignal()
    ntp_requested = pyqtSignal()
    weather_requested = pyqtSignal()
    auto_mode_requested = pyqtSignal()
    chart_requested = pyqtSignal()

    def __init__(self) -> None:
        super().__init__()
        self._connected = False
        layout = QVBoxLayout(self)

        port_box = QGroupBox("串口管理")
        port_layout = QHBoxLayout(port_box)
        self.port_combo = QComboBox()
        self.refresh_button = QPushButton("刷新")
        self.connect_button = QPushButton("连接")
        self.disconnect_button = QPushButton("断开")
        self.ping_button = QPushButton("Ping")
        self.reset_button = QPushButton("Reset")
        for widget in (self.port_combo, self.refresh_button, self.connect_button, self.disconnect_button, self.ping_button, self.reset_button):
            port_layout.addWidget(widget)
        layout.addWidget(port_box)

        format_box = QGroupBox("显示控制")
        format_layout = QHBoxLayout(format_box)
        self.display_on_button = QPushButton("DISPLAY ON")
        self.display_off_button = QPushButton("DISPLAY OFF")
        self.format_left_button = QPushButton("FORMAT LEFT")
        self.format_right_button = QPushButton("FORMAT RIGHT")
        self.mode_day_button = QPushButton("MODE DAY")
        self.mode_night_button = QPushButton("MODE NIGHT")
        self.msg_input = QLineEdit()
        self.msg_input.setPlaceholderText("消息文本")
        self.msg_send_button = QPushButton("发送 MSG")
        for widget in (
            self.display_on_button,
            self.display_off_button,
            self.format_left_button,
            self.format_right_button,
            self.mode_day_button,
            self.mode_night_button,
            self.msg_input,
            self.msg_send_button,
        ):
            format_layout.addWidget(widget)
        layout.addWidget(format_box)

        io_box = QGroupBox("外设 / 基础时钟")
        io_layout = QFormLayout(io_box)
        self.date_input = QLineEdit("YEAR MONTH DATE 2026 06 18")
        self.time_input = QLineEdit("HOUR MINUTE SECOND 12 34 56")
        self.alarm_input = QLineEdit("HOUR MINUTE SECOND 07 30 00")
        self.led_input = QLineEdit("AA")
        self.beep_input = QLineEdit("500")
        self.set_date_button = QPushButton("SET DATE")
        self.set_time_button = QPushButton("SET TIME")
        self.set_alarm_button = QPushButton("SET ALARM")
        self.off_alarm_button = QPushButton("ALARM OFF")
        self.set_led_button = QPushButton("SET LED")
        self.set_beep_button = QPushButton("SET BEEP")
        io_layout.addRow(self.date_input, self.set_date_button)
        io_layout.addRow(self.time_input, self.set_time_button)
        io_layout.addRow(self.alarm_input, self.set_alarm_button)
        io_layout.addRow(self.off_alarm_button)
        io_layout.addRow(self.led_input, self.set_led_button)
        io_layout.addRow(self.beep_input, self.set_beep_button)
        layout.addWidget(io_box)

        key_box = QGroupBox("虚拟按键")
        key_layout = QGridLayout(key_box)
        separator = QFrame()
        separator.setFrameShape(QFrame.VLine)
        separator.setFrameShadow(QFrame.Sunken)
        key_layout.addWidget(separator, 0, 1, 2, 1)
        self.key_buttons = {}
        for row_index, row in enumerate(KEY_LAYOUT_ROWS):
            for column_index, name in enumerate(row):
                button = QPushButton(name)
                self.key_buttons[name] = button
                target_column = column_index + 1 if column_index > 0 else column_index
                key_layout.addWidget(button, row_index, target_column)
                button.clicked.connect(lambda _checked=False, key=name: self._emit(f"*SET:KEY {key}"))
        layout.addWidget(key_box)

        demo_box = QGroupBox("协议演示")
        demo_layout = QHBoxLayout(demo_box)
        self.demo_combo = QComboBox()
        for label, command in DEMO_PRESETS:
            self.demo_combo.addItem(label, command)
        self.demo_send_button = QPushButton("发送演示命令")
        self.abbrev_demo_button = QPushButton("缩写演示")
        self.mixed_case_demo_button = QPushButton("大小写混合演示")
        demo_layout.addWidget(self.demo_combo)
        demo_layout.addWidget(self.demo_send_button)
        demo_layout.addWidget(self.abbrev_demo_button)
        demo_layout.addWidget(self.mixed_case_demo_button)
        layout.addWidget(demo_box)

        query_box = QGroupBox("状态查询")
        query_layout = QHBoxLayout(query_box)
        self.get_display_button = QPushButton("GET DISPLAY")
        self.get_format_button = QPushButton("GET FORMAT")
        self.get_date_button = QPushButton("GET DATE")
        self.get_time_button = QPushButton("GET TIME")
        self.get_alarm_button = QPushButton("GET ALARM")
        for widget in (
            self.get_display_button,
            self.get_format_button,
            self.get_date_button,
            self.get_time_button,
            self.get_alarm_button,
        ):
            query_layout.addWidget(widget)
        layout.addWidget(query_box)

        extension_box = QGroupBox("扩展功能")
        extension_layout = QHBoxLayout(extension_box)
        self.ntp_button = QPushButton("NTP 对时")
        self.weather_button = QPushButton("获取天气")
        self.auto_mode_button = QPushButton("自动昼夜")
        self.chart_button = QPushButton("导出图表")
        for widget in (self.ntp_button, self.weather_button, self.auto_mode_button, self.chart_button):
            extension_layout.addWidget(widget)
        layout.addWidget(extension_box)

        raw_box = QGroupBox("调试区")
        raw_layout = QHBoxLayout(raw_box)
        self.raw_input = QLineEdit()
        self.raw_send_button = QPushButton("发送原始命令")
        raw_layout.addWidget(self.raw_input)
        raw_layout.addWidget(self.raw_send_button)
        layout.addWidget(raw_box)

        self.ping_button.clicked.connect(lambda: self.command_requested.emit(CommandRequest("*PING", requires_ready=False)))
        self.reset_button.clicked.connect(lambda: self._emit("*RST"))
        self.display_on_button.clicked.connect(lambda: self._emit("*SET:DISPLAY ON"))
        self.display_off_button.clicked.connect(lambda: self._emit("*SET:DISPLAY OFF"))
        self.format_left_button.clicked.connect(lambda: self._emit("*SET:FORMAT LEFT"))
        self.format_right_button.clicked.connect(lambda: self._emit("*SET:FORMAT RIGHT"))
        self.mode_day_button.clicked.connect(lambda: self._emit("*SET:MODE DAY"))
        self.mode_night_button.clicked.connect(lambda: self._emit("*SET:MODE NIGHT"))
        self.msg_send_button.clicked.connect(lambda: self._emit(f"*SET:MSG {self.msg_input.text().strip()}"))
        self.set_date_button.clicked.connect(lambda: self._emit(f"*SET:DATE {self.date_input.text().strip()}"))
        self.set_time_button.clicked.connect(lambda: self._emit(f"*SET:TIME {self.time_input.text().strip()}"))
        self.set_alarm_button.clicked.connect(lambda: self._emit(f"*SET:ALARM {self.alarm_input.text().strip()}"))
        self.off_alarm_button.clicked.connect(lambda: self._emit("*SET:ALARM OFF"))
        self.set_led_button.clicked.connect(lambda: self._emit(f"*SET:LED {self.led_input.text().strip()}"))
        self.set_beep_button.clicked.connect(lambda: self._emit(f"*SET:BEEP {self.beep_input.text().strip()}"))
        self.demo_send_button.clicked.connect(lambda: self._emit(self.demo_combo.currentData()))
        self.abbrev_demo_button.clicked.connect(lambda: self.command_requested.emit(CommandRequest(ABBREV_DEMO_COMMAND)))
        self.mixed_case_demo_button.clicked.connect(lambda: self.command_requested.emit(CommandRequest(MIXED_CASE_DEMO_COMMAND)))
        self.get_display_button.clicked.connect(lambda: self._emit("*GET:DISPLAY"))
        self.get_format_button.clicked.connect(lambda: self._emit("*GET:FORMAT"))
        self.get_date_button.clicked.connect(lambda: self._emit("*GET:DATE"))
        self.get_time_button.clicked.connect(lambda: self._emit("*GET:TIME"))
        self.get_alarm_button.clicked.connect(lambda: self._emit("*GET:ALARM"))
        self.connect_button.clicked.connect(lambda: self.connect_requested.emit(self.selected_port()))
        self.disconnect_button.clicked.connect(self.disconnect_requested.emit)
        self.refresh_button.clicked.connect(self.refresh_requested.emit)
        self.ntp_button.clicked.connect(self.ntp_requested.emit)
        self.weather_button.clicked.connect(self.weather_requested.emit)
        self.auto_mode_button.clicked.connect(self.auto_mode_requested.emit)
        self.chart_button.clicked.connect(self.chart_requested.emit)
        self.raw_send_button.clicked.connect(lambda: self.command_requested.emit(CommandRequest(self.raw_input.text().strip(), requires_ready=False)))

    def _emit(self, command: str) -> None:
        self.command_requested.emit(CommandRequest(command, followups_on_ok=build_followups(command)))

    def refresh_ports(self, port_names) -> None:
        if self.port_combo.view().isVisible():
            return
        current = self.selected_port()
        entries = []
        for item in port_names:
            if isinstance(item, tuple):
                label, device = item
            elif hasattr(item, "label") and hasattr(item, "device"):
                label, device = item.label, item.device
            else:
                label = str(item)
                device = str(item)
            entries.append((label, device))

        current_entries = [
            (self.port_combo.itemText(index), self.port_combo.itemData(index) or self.port_combo.itemText(index))
            for index in range(self.port_combo.count())
        ]
        if entries == current_entries:
            self.connect_button.setEnabled((not self._connected) and (self.port_combo.count() > 0))
            return

        self.port_combo.clear()
        for label, device in entries:
            self.port_combo.addItem(label, device)
        if current:
            index = self.port_combo.findData(current)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)
        self.connect_button.setEnabled((not self._connected) and (self.port_combo.count() > 0))

    def selected_port(self) -> str:
        return self.port_combo.currentData() or self.port_combo.currentText()

    def set_connected(self, connected: bool) -> None:
        self._connected = connected
        self.connect_button.setEnabled((not connected) and (self.port_combo.count() > 0))
        self.disconnect_button.setEnabled(connected)

    def set_ready_enabled(self, ready: bool) -> None:
        for button in (
            self.display_on_button,
            self.display_off_button,
            self.format_left_button,
            self.format_right_button,
            self.mode_day_button,
            self.mode_night_button,
            self.msg_send_button,
            self.set_date_button,
            self.set_time_button,
            self.set_alarm_button,
            self.off_alarm_button,
            self.set_led_button,
            self.set_beep_button,
            self.reset_button,
            self.demo_send_button,
            self.abbrev_demo_button,
            self.mixed_case_demo_button,
            self.get_display_button,
            self.get_format_button,
            self.get_date_button,
            self.get_time_button,
            self.get_alarm_button,
        ):
            button.setEnabled(ready)
        for button in self.key_buttons.values():
            button.setEnabled(ready)
