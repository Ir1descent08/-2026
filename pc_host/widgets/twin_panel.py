from PyQt5.QtCore import QSize, Qt, QRectF, pyqtSignal
from PyQt5.QtGui import QColor, QPainter
from PyQt5.QtWidgets import QFrame, QGridLayout, QLabel, QPushButton, QVBoxLayout, QWidget
from pc_host.commands import KEY_LAYOUT_ROWS


class SevenSegmentDigit(QWidget):
    _SEGMENTS = {
        "0": "abcdef",
        "1": "bc",
        "2": "abdeg",
        "3": "abcdg",
        "4": "bcfg",
        "5": "acdfg",
        "6": "acdefg",
        "7": "abc",
        "8": "abcdefg",
        "9": "abcdfg",
        "A": "abcefg",
        "B": "cdefg",
        "C": "adef",
        "D": "bcdeg",
        "E": "adefg",
        "F": "aefg",
        "H": "bcefg",
        "I": "bc",
        "J": "bcde",
        "L": "def",
        "N": "abcef",
        "O": "abcdef",
        "P": "abefg",
        "R": "eg",
        "S": "acdfg",
        "T": "defg",
        "U": "bcdef",
        "W": "bcdef",
        "Y": "bcdfg",
        "-": "g",
        "_": "d",
        " ": "",
    }

    def __init__(self) -> None:
        super().__init__()
        self._char = " "
        self._decimal_point = False
        self.setMinimumSize(28, 56)

    def sizeHint(self) -> QSize:
        return QSize(36, 72)

    def set_character(self, char: str, decimal_point: bool) -> None:
        value = (char[:1] or " ").upper()
        if value not in self._SEGMENTS:
            value = " "
        self._char = value
        self._decimal_point = decimal_point
        self.update()

    def display_text(self) -> str:
        return self._char + ("." if self._decimal_point else "")

    def text(self) -> str:
        return self.display_text()

    def paintEvent(self, _event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.fillRect(self.rect(), QColor("#111111"))

        inner = self.rect().adjusted(4, 4, -4, -4)
        thickness = max(4.0, min(inner.width() / 6.0, inner.height() / 10.0))
        half = inner.height() / 2.0
        mid_top = inner.top() + half - thickness * 0.5
        mid_bottom = inner.top() + half + thickness * 0.5
        active = QColor("#ff453a")
        inactive = QColor("#3a0f0d")

        segments = {
            "a": QRectF(inner.left() + thickness, inner.top(), inner.width() - 2 * thickness, thickness),
            "d": QRectF(inner.left() + thickness, inner.bottom() - thickness, inner.width() - 2 * thickness, thickness),
            "g": QRectF(inner.left() + thickness, inner.top() + half - thickness * 0.5, inner.width() - 2 * thickness, thickness),
            "f": QRectF(inner.left(), inner.top() + thickness, thickness, half - 1.5 * thickness),
            "b": QRectF(inner.right() - thickness, inner.top() + thickness, thickness, half - 1.5 * thickness),
            "e": QRectF(inner.left(), mid_bottom, thickness, inner.bottom() - mid_bottom - thickness),
            "c": QRectF(inner.right() - thickness, mid_bottom, thickness, inner.bottom() - mid_bottom - thickness),
        }
        enabled = set(self._SEGMENTS[self._char])
        painter.setPen(Qt.NoPen)
        for name, rect in segments.items():
            painter.setBrush(active if name in enabled else inactive)
            painter.drawRoundedRect(rect, thickness / 3.0, thickness / 3.0)
        radius = thickness * 0.45
        painter.setBrush(active if self._decimal_point else inactive)
        painter.drawEllipse(QRectF(inner.right() - radius * 1.6, inner.bottom() - radius * 1.6, radius * 1.2, radius * 1.2))


class TwinPanel(QWidget):
    key_requested = pyqtSignal(str)

    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        self.mode_value = QLabel("UNKNOWN")
        self.mode_value.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.mode_value)

        digit_grid = QGridLayout()
        self.digit_widgets = []
        self.digit_labels = self.digit_widgets
        for index in range(8):
            widget = SevenSegmentDigit()
            self.digit_widgets.append(widget)
            digit_grid.addWidget(widget, 0, index)
        layout.addLayout(digit_grid)

        led_grid = QGridLayout()
        self.led_labels = []
        for index in range(8):
            label = QLabel("○")
            label.setAlignment(Qt.AlignCenter)
            self.led_labels.append(label)
            led_grid.addWidget(label, 0, index)
        layout.addLayout(led_grid)

        self.key_layout = QGridLayout()
        self.key_buttons = {}
        separator = QFrame()
        separator.setFrameShape(QFrame.VLine)
        separator.setFrameShadow(QFrame.Sunken)
        self.key_layout.addWidget(separator, 0, 1, 2, 1)
        for row_index, row in enumerate(KEY_LAYOUT_ROWS):
            for column_index, name in enumerate(row):
                button = QPushButton(name)
                self.key_buttons[name] = button
                button.clicked.connect(lambda _checked=False, key=name: self.key_requested.emit(key))
                target_column = column_index + 1 if column_index > 0 else column_index
                self.key_layout.addWidget(button, row_index, target_column)
        layout.addLayout(self.key_layout)

    def update_state(self, state) -> None:
        self.mode_value.setText(state.mode_value)
        dot_mask = int(state.seg_dp_hex, 16)
        chars = list(state.seg_text[:8].ljust(8))
        if state.edit_mode != 0:
            blink_on = ((state.ui_now_ms // 500) % 2) == 0
            if blink_on:
                if state.format_value == "RIGHT":
                    start = 6 - state.edit_field * 2
                else:
                    start = state.edit_field * 2
                if 0 <= start <= 6:
                    chars[start] = " "
                    chars[start + 1] = " "
        for index, char in enumerate(chars):
            self.digit_widgets[index].set_character(" " if char == "_" else char, bool(dot_mask & (1 << index)))
        led_value = int(state.led_hex, 16)
        for index, label in enumerate(self.led_labels):
            label.setText("●" if ((led_value >> index) & 0x01) else "○")
        for name, button in self.key_buttons.items():
            button.setText(f"{name} *" if state.last_key_event == name else name)
            button.setEnabled(state.ready)
