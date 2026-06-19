# pc_host/widgets/twin_panel.py
from PyQt5.QtWidgets import QGridLayout, QLabel, QVBoxLayout, QWidget
from pc_host.commands import VALID_KEY_NAMES


class TwinPanel(QWidget):
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
        self.key_labels = {}
        for index, name in enumerate(VALID_KEY_NAMES):
            label = QLabel(name)
            self.key_labels[name] = label
            key_grid.addWidget(label, index // 5, index % 5)
        layout.addLayout(key_grid)

    def update_state(self, state) -> None:
        self.mode_value.setText(state.mode_value)
        for index, char in enumerate(state.seg_text[:8]):
            self.digit_labels[index].setText(char)
        led_bits = bin(int(state.led_hex, 16))[2:].zfill(8)
        for index, bit in enumerate(led_bits):
            self.led_labels[index].setText("●" if bit == "1" else "○")
        for name, label in self.key_labels.items():
            label.setText(f"{name} *" if state.last_key_event == name else name)
