import sys
import tempfile
import types
import unittest
from pathlib import Path

serial_module = types.ModuleType("serial")
serial_module.Serial = object
serial_tools_module = types.ModuleType("serial.tools")
serial_list_ports_module = types.ModuleType("serial.tools.list_ports")
serial_list_ports_module.comports = lambda: []
serial_tools_module.list_ports = serial_list_ports_module
serial_module.tools = serial_tools_module
sys.modules.setdefault("serial", serial_module)
sys.modules.setdefault("serial.tools", serial_tools_module)
sys.modules.setdefault("serial.tools.list_ports", serial_list_ports_module)

from test_runner import check_response, score_log


class ScoreRuleTests(unittest.TestCase):
    def test_check_response_format_rules(self):
        self.assertTrue(check_response(["OK 12.30.45"], "ok_left12")[0])
        self.assertTrue(check_response(["OK 54.03.21"], "ok_right12")[0])
        self.assertFalse(check_response(["OK 12-30-45"], "ok_left12")[0])
        self.assertFalse(check_response(["OK 54-03-21"], "ok_right12")[0])

    def test_score_log_format_category_full_score(self):
        content = "\n".join(
            [
                "[12:00:00.000] >> L057 *SET:FORMAT LEFT",
                "[12:00:00.010] << OK",
                "[12:00:00.020] >> L058 *GET:TIME",
                "[12:00:00.030] << OK 12.30.45",
                "[12:00:00.040] >> L059 *SET:FORMAT RIGHT",
                "[12:00:00.050] << OK",
                "[12:00:00.060] >> L060 *GET:TIME",
                "[12:00:00.070] << OK 54.03.21",
                "[12:00:00.080] >> L061 *GET:DATE",
                "[12:00:00.090] << OK 52.21.60",
                "[12:00:00.100] >> L062 *SET:FORMAT LEFT",
                "[12:00:00.110] << OK",
                "[12:00:00.120] >> L063 *GET:TIME",
                "[12:00:00.130] << OK 12.30.45",
            ]
        )
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "format_log.txt"
            log_path.write_text(content + "\n", encoding="utf-8")
            report, _, _ = score_log(str(log_path))
        self.assertIn("【FORMAT RIGHT逆序+小数点】  7/7 通过  → 2.0/2.0 分", report)

    def test_score_log_case_and_space_categories_full_score(self):
        content = "\n".join(
            [
                "[12:00:00.000] >> L026 *set:time hour min sec 10 00 00",
                "[12:00:00.010] << OK",
                "[12:00:00.020] >> L027 *Set:Time Hour Min Sec 11 00 00",
                "[12:00:00.030] << OK",
                "[12:00:00.040] >> L028 *SET:time HOUR min SEC 09 00 00",
                "[12:00:00.050] << OK",
                "[12:00:00.060] >> L029 *get:TIME",
                "[12:00:00.070] << OK 09.00.00",
                "[12:00:00.080] >> L030 *ping",
                "[12:00:00.090] << *PONG 12",
                "[12:00:00.100] >> L031 *rst",
                "[12:00:00.110] << OK",
                "[12:00:00.120] >> L033 *SET:DATE  YEAR  MONTH  DATE  2026  06  15",
                "[12:00:00.130] << OK",
                "[12:00:00.140] >> L034 *SET:TIME   HOUR   MIN   SEC   12   00   00",
                "[12:00:00.150] << OK",
                "[12:00:00.160] >> L035 *GET:  TIME",
                "[12:00:00.170] << OK 12.00.00",
                "[12:00:00.180] >> L036 *PING",
                "[12:00:00.190] << *PONG 13",
            ]
        )
        with tempfile.TemporaryDirectory() as tmpdir:
            log_path = Path(tmpdir) / "case_space_log.txt"
            log_path.write_text(content + "\n", encoding="utf-8")
            report, _, _ = score_log(str(log_path))
        self.assertIn("【大小写不敏感】  6/6 通过  → 2.0/2.0 分", report)
        self.assertIn("【空格容错】  4/4 通过  → 2.0/2.0 分", report)


if __name__ == "__main__":
    unittest.main()
