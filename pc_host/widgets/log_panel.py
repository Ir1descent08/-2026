# pc_host/widgets/log_panel.py
from datetime import datetime
from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import QHBoxLayout, QPushButton, QTextEdit, QVBoxLayout, QWidget


class LogPanel(QWidget):
    export_requested = pyqtSignal()

    COLORS = {
        "send": "#1f6feb",
        "reply": "#238636",
        "event": "#a371f7",
        "error": "#d1242f",
        "warn": "#9a6700",
    }

    def __init__(self) -> None:
        super().__init__()
        self.text_edit = QTextEdit()
        self.text_edit.setReadOnly(True)
        self.clear_button = QPushButton("清空日志")
        self.export_button = QPushButton("导出日志")
        self.clear_button.clicked.connect(self.clear_entries)
        self.export_button.clicked.connect(self.export_requested.emit)
        layout = QVBoxLayout(self)
        layout.addWidget(self.text_edit)
        button_row = QHBoxLayout()
        button_row.addWidget(self.clear_button)
        button_row.addWidget(self.export_button)
        layout.addLayout(button_row)

    def append_entry(self, kind: str, text: str) -> None:
        stamp = datetime.now().strftime("%H:%M:%S")
        color = self.COLORS.get(kind, "#57606a")
        self.text_edit.append(f'<span style="color:{color}">[{stamp}] {text}</span>')

    def clear_entries(self) -> None:
        self.text_edit.clear()

    def export_to_file(self, path: str) -> None:
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(self.text_edit.toPlainText())
