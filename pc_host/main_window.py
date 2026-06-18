from PyQt5.QtWidgets import QGroupBox, QHBoxLayout, QLabel, QMainWindow, QVBoxLayout, QWidget


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("PC Host")
        root = QWidget()
        self.setCentralWidget(root)

        main_layout = QVBoxLayout(root)
        top_row = QHBoxLayout()
        bottom_box = QGroupBox("收发日志 / 原始命令")
        bottom_layout = QVBoxLayout(bottom_box)
        self.log_placeholder = QLabel("日志区占位")
        bottom_layout.addWidget(self.log_placeholder)

        self.control_placeholder = QGroupBox("左侧控制区")
        control_layout = QVBoxLayout(self.control_placeholder)
        control_layout.addWidget(QLabel("串口 / 时钟 / 显示 / 外设 / 按键 / 扩展"))

        self.twin_placeholder = QGroupBox("右侧数字孪生")
        twin_layout = QVBoxLayout(self.twin_placeholder)
        twin_layout.addWidget(QLabel("7SEG / LED / KEY / USER1 / USER2"))

        top_row.addWidget(self.control_placeholder, 3)
        top_row.addWidget(self.twin_placeholder, 5)
        main_layout.addLayout(top_row)
        main_layout.addWidget(bottom_box, 2)
