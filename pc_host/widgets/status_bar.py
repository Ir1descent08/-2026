# pc_host/widgets/status_bar.py
from PyQt5.QtWidgets import QGridLayout, QLabel, QWidget


class StatusBarWidget(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QGridLayout(self)
        layout.addWidget(QLabel("串口"), 0, 0)
        layout.addWidget(QLabel("连接"), 0, 2)
        layout.addWidget(QLabel("Ready"), 0, 4)
        layout.addWidget(QLabel("延迟"), 0, 6)
        layout.addWidget(QLabel("FORMAT"), 1, 0)
        layout.addWidget(QLabel("MODE"), 1, 2)
        layout.addWidget(QLabel("ALARM"), 1, 4)
        layout.addWidget(QLabel("DISPLAY"), 1, 6)
        self.port_value = QLabel("-")
        self.connection_value = QLabel("未连接")
        self.ready_value = QLabel("WAIT")
        self.rtt_value = QLabel("0 ms")
        self.format_value = QLabel("UNKNOWN")
        self.mode_value = QLabel("UNKNOWN")
        self.alarm_value = QLabel("UNKNOWN")
        self.display_value = QLabel("UNKNOWN")
        layout.addWidget(self.port_value, 0, 1)
        layout.addWidget(self.connection_value, 0, 3)
        layout.addWidget(self.ready_value, 0, 5)
        layout.addWidget(self.rtt_value, 0, 7)
        layout.addWidget(self.format_value, 1, 1)
        layout.addWidget(self.mode_value, 1, 3)
        layout.addWidget(self.alarm_value, 1, 5)
        layout.addWidget(self.display_value, 1, 7)

    def update_state(self, state) -> None:
        self.port_value.setText(state.port_name or "-")
        self.connection_value.setText("已连接" if state.connected else "未连接")
        self.ready_value.setText("READY" if state.ready else "WAIT")
        self.rtt_value.setText(f"{state.last_rtt_ms} ms")
        self.format_value.setText(state.format_value)
        self.mode_value.setText(state.mode_value)
        self.alarm_value.setText(state.alarm_value)
        self.display_value.setText(state.display_enabled)
