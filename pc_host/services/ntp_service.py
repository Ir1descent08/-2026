# pc_host/services/ntp_service.py
from datetime import datetime, timedelta, timezone
import ntplib
from pc_host.commands import CommandRequest, build_followups


CHINA_TIMEZONE = timezone(timedelta(hours=8))


def fetch_ntp_datetime(client=None, host: str = "pool.ntp.org") -> datetime:
    active_client = client or ntplib.NTPClient()
    response = active_client.request(host, version=3)
    return datetime.fromtimestamp(response.tx_time, tz=timezone.utc).astimezone(CHINA_TIMEZONE)


def build_sync_requests(moment: datetime) -> list[CommandRequest]:
    return [
        CommandRequest(f"*SET:DATE YEAR MONTH DATE {moment.year:04d} {moment.month:02d} {moment.day:02d}", followups_on_ok=build_followups("*SET:DATE")),
        CommandRequest(f"*SET:TIME HOUR MINUTE SECOND {moment.hour:02d} {moment.minute:02d} {moment.second:02d}", followups_on_ok=build_followups("*SET:TIME")),
    ]
