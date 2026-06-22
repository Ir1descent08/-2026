# pc_host/command_scheduler.py
from collections import deque
from typing import Callable, Optional
from pc_host.commands import BOOT_GUARD_MS, COMMAND_INTERVAL_MS, CommandRequest, initial_sync_requests


HEARTBEAT_INTERVAL_MS = 1000


class CommandScheduler:
    def __init__(self, send_callable: Callable[[str], None], now_ms: Callable[[], int]) -> None:
        self._send_callable = send_callable
        self._now_ms = now_ms
        self._queue: deque[CommandRequest] = deque()
        self._current: Optional[CommandRequest] = None
        self._last_send_ms = -COMMAND_INTERVAL_MS
        self._boot_deadline_ms = 0
        self._boot_ping_sent = False
        self._last_ping_ms = 0
        self._next_ping_ms = 0
        self.ready = False
        self.waiting_ready = False
        self.last_rtt_ms = 0

    def current_request_text(self) -> Optional[str]:
        return None if self._current is None else self._current.text

    def on_port_opened(self) -> None:
        self._queue.clear()
        self._current = None
        self.ready = False
        self.waiting_ready = True
        self._boot_ping_sent = False
        self._boot_deadline_ms = self._now_ms() + BOOT_GUARD_MS
        self._last_send_ms = self._now_ms() - COMMAND_INTERVAL_MS
        self._next_ping_ms = self._boot_deadline_ms
        self.last_rtt_ms = 0

    def on_port_closed(self) -> None:
        self._queue.clear()
        self._current = None
        self.ready = False
        self.waiting_ready = False
        self._boot_ping_sent = False
        self._next_ping_ms = 0
        self.last_rtt_ms = 0

    def mark_ready(self) -> None:
        if self.ready:
            return
        was_waiting = self.waiting_ready
        self.ready = True
        self.waiting_ready = False
        if was_waiting:
            self._next_ping_ms = self._now_ms() + HEARTBEAT_INTERVAL_MS
            for request in initial_sync_requests():
                self.enqueue(request)

    def enqueue(self, request: CommandRequest) -> None:
        self._queue.append(request)

    def handle_reply(self, message) -> None:
        if message.kind == "pong":
            self.last_rtt_ms = self._now_ms() - self._last_ping_ms
            self._current = None
            self.mark_ready()
            self._next_ping_ms = self._now_ms() + HEARTBEAT_INTERVAL_MS
            return
        if message.kind == "ok" and self._current is not None:
            for followup in self._current.followups_on_ok:
                self.enqueue(CommandRequest(followup))
            self._current = None
            return
        if message.kind == "error":
            self._current = None

    def tick(self) -> None:
        now = self._now_ms()
        if self.waiting_ready and not self._boot_ping_sent and now >= self._boot_deadline_ms and now - self._last_send_ms >= COMMAND_INTERVAL_MS:
            self._send_callable("*PING")
            self._current = CommandRequest("*PING", requires_ready=False)
            self._last_ping_ms = now
            self._last_send_ms = now
            self._boot_ping_sent = True
            return
        if self.ready and self._current is None and not self._queue and now >= self._next_ping_ms and now - self._last_send_ms >= COMMAND_INTERVAL_MS:
            self._send_callable("*PING")
            self._current = CommandRequest("*PING", requires_ready=False)
            self._last_ping_ms = now
            self._last_send_ms = now
            self._next_ping_ms = now + HEARTBEAT_INTERVAL_MS
            return
        if not self._queue:
            return
        if now - self._last_send_ms < COMMAND_INTERVAL_MS:
            return
        request = self._queue[0]
        if request.requires_ready and not self.ready:
            return
        self._queue.popleft()
        self._send_callable(request.text)
        self._last_send_ms = now
        self._current = request
