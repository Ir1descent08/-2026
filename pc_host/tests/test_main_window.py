import unittest
from PyQt5.QtWidgets import QApplication
from pc_host.main_window import MainWindow


class MainWindowSmokeTests(unittest.TestCase):
    def test_window_has_a2_regions(self):
        app = QApplication.instance() or QApplication([])
        window = MainWindow()
        self.assertEqual(window.windowTitle(), "PC Host")
        self.assertIsNotNone(window.control_placeholder)
        self.assertIsNotNone(window.twin_placeholder)
        self.assertIsNotNone(window.log_placeholder)


if __name__ == "__main__":
    unittest.main()
