import csv
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from astral import Observer
from pc_host.services.chart_service import append_history_row, build_history_figure
from pc_host.services.daynight_service import compute_mode
from pc_host.services.ntp_service import build_sync_requests, fetch_ntp_datetime
from pc_host.services.weather_service import fetch_weather


class FakeNtpResponse:
    tx_time = 1718700000


class FakeNtpClient:
    def request(self, host, version=3):
        return FakeNtpResponse()


class ServiceTests(unittest.TestCase):
    def test_fetch_ntp_datetime_uses_client_response(self):
        moment = fetch_ntp_datetime(FakeNtpClient())
        self.assertEqual(moment.utcoffset(), timezone(timedelta(hours=8)).utcoffset(None))
        self.assertEqual(moment.year, 2024)
        self.assertEqual(moment.month, 6)
        self.assertEqual(moment.day, 18)
        self.assertEqual(moment.hour, 16)
        self.assertEqual(moment.minute, 40)
        self.assertEqual(moment.second, 0)

    def test_build_sync_requests_returns_date_then_time(self):
        requests = build_sync_requests(datetime(2026, 6, 18, 12, 34, 56, tzinfo=timezone.utc))
        self.assertEqual(requests[0].text, "*SET:DATE YEAR MONTH DATE 2026 06 18")
        self.assertEqual(requests[1].text, "*SET:TIME HOUR MINUTE SECOND 12 34 56")

    def test_compute_mode_returns_day_when_between_sunrise_and_sunset(self):
        observer = Observer(latitude=31.2304, longitude=121.4737)
        # 04:00 UTC = 12:00 noon Shanghai time (clearly daytime)
        mode = compute_mode(observer, datetime(2026, 6, 18, 4, 0, 0, tzinfo=timezone.utc), "Asia/Shanghai")
        self.assertEqual(mode, "DAY")


class FakeWeatherResponse:
    def json(self):
        return {
            "current_condition": [
                {"temp_C": "28", "weatherDesc": [{"value": "Sunny"}]}
            ]
        }

    def raise_for_status(self):
        return None


class FakeSession:
    def get(self, url, params=None, timeout=5):
        return FakeWeatherResponse()


class WeatherAndChartTests(unittest.TestCase):
    def test_fetch_weather_returns_temperature_and_condition(self):
        temp_c, condition = fetch_weather(FakeSession(), "Shanghai")
        self.assertEqual(temp_c, 28)
        self.assertEqual(condition, "Sunny")

    def test_chart_service_writes_rows_and_builds_a_figure(self):
        with tempfile.NamedTemporaryFile("w+", delete=False) as handle:
            append_history_row(handle.name, "ntp_sync", "OK")
            append_history_row(handle.name, "mode", "DAY")
            figure = build_history_figure(handle.name)
            self.assertEqual(len(figure.axes), 1)

    def test_chart_service_skips_malformed_rows(self):
        with tempfile.NamedTemporaryFile("w+", delete=False, suffix=".csv") as handle:
            handle.write("2026-06-18T12:00:00,ntp_sync,OK\n")
            handle.write("malformed_row_with_only_one_column\n")
            handle.write("2026-06-18T12:01:00,mode,DAY\n")
            handle.write("too,many,columns,here,extra\n")
            handle.write("\n")
            handle.flush()
            figure = build_history_figure(handle.name)
            self.assertEqual(len(figure.axes), 1)
