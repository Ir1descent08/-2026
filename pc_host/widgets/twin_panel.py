# pc_host/widgets/twin_panel.py
from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import QGridLayout, QLabel, QPushButton, QVBoxLayout, QWidget
from pc_host.commands import VALID_KEY_NAMES


class TwinPanel(QWidget):
    key_requested = pyqtSignal(str)

    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        self.mode_value = QLabel("UNKNOWN")
        layout.addWidget(self.mode_value)

        digit_grid = QGridLayout()
        self.digit_labels = []
        for index in range(8):
            label = QLabel(" ")
            self.digit_labels.append(label)
            digit_grid.addWidget(label, 0, index)
        layout.addLayout(digit_grid)

        led_grid = QGridLayout()
        self.led_labels = []
        for index in range(8):
            label = QLabel("○")
            self.led_labels.append(label)
            led_grid.addWidget(label, 0, index)
        layout.addLayout(led_grid)

        key_grid = QGridLayout()
        self.key_buttons = {}
        for index, name in enumerate(VALID_KEY_NAMES):
            button = QPushButton(name)
            self.key_buttons[name] = button
            button.clicked.connect(lambda _checked=False, key=name: self.key_requested.emit(key))
            key_grid.addWidget(button, index // 5, index % 5)
        layout.addLayout(key_grid)

    def update_state(self, state) -> None:
        self.mode_value.setText(state.mode_value)
        dot_mask = int(state.seg_dp_hex, 16)
        for index, char in enumerate(state.seg_text[:8]):
            text = char if char != "_" else " "
            if dot_mask & (1 << index):
                text = f"{text}." if text.strip() else "."
            self.digit_labels[index].setText(text)
        led_bits = bin(int(state.led_hex, 16))[2:].zfill(8)
        for index, bit in enumerate(led_bits):
            self.led_labels[index].setText("●" if bit == "1" else "○")
        for name, button in self.key_buttons.items():
            button.setText(f"{name} *" if state.last_key_event == name else name)
            button.setEnabled(state.ready)
