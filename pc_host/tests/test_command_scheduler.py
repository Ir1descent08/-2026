import unittest
from pc_host.command_scheduler import CommandScheduler
from pc_host.commands import CommandRequest
from pc_host.protocol import parse_line


class FakeClock:
    def __init__(self):
        self.value = 0

    def __call__(self):
        return self.value

    def advance(self, ms: int) -> None:
        self.value += ms


class CommandSchedulerTests(unittest.TestCase):
    def test_boot_guard_waits_then_sends_ping(self):
        sent = []
        clock = FakeClock()
        scheduler = CommandScheduler(sent.append, clock)
        scheduler.on_port_opened()
        scheduler.tick()
        self.assertEqual(sent, [])
        clock.advance(10_000)
        scheduler.tick()
        self.assertEqual(sent, ["*PING"])
        self.assertEqual(scheduler.current_request_text(), "*PING")

    def test_pong_marks_ready_and_queues_initial_gets(self):
        sent = []
        clock = FakeClock()
        scheduler = CommandScheduler(sent.append, clock)
        scheduler.on_port_opened()
        clock.advance(10_000)
        scheduler.tick()
        clock.advance(120)
        scheduler.handle_reply(parse_line("*PONG 9"))
        self.assertEqual(scheduler.last_rtt_ms, 120)
        clock.advance(500)
        scheduler.tick()
        self.assertEqual(sent[1], "*GET:DISPLAY")
        self.assertTrue(scheduler.ready)

    def test_ready_scheduler_sends_periodic_ping(self):
        sent = []
        clock = FakeClock()
        scheduler = CommandScheduler(sent.append, clock)
        scheduler.on_port_opened()
        clock.advance(10_000)
        scheduler.tick()
        clock.advance(120)
        scheduler.handle_reply(parse_line("*PONG 9"))
        scheduler._queue.clear()
        clock.advance(1000)
        scheduler.tick()
        self.assertEqual(sent[-1], "*PING")
        self.assertEqual(scheduler.current_request_text(), "*PING")

    def test_followups_wait_for_ok(self):
        sent = []
        clock = FakeClock()
        scheduler = CommandScheduler(sent.append, clock)
        scheduler.mark_ready()
        scheduler.enqueue(CommandRequest("*SET:FORMAT RIGHT", followups_on_ok=("*GET:FORMAT",)))
        scheduler.tick()
        self.assertEqual(sent, ["*SET:FORMAT RIGHT"])
        self.assertEqual(scheduler.current_request_text(), "*SET:FORMAT RIGHT")
        scheduler.handle_reply(parse_line("OK"))
        clock.advance(500)
        scheduler.tick()
        self.assertEqual(sent[1], "*GET:FORMAT")

    def test_current_request_clears_after_error(self):
        sent = []
        clock = FakeClock()
        scheduler = CommandScheduler(sent.append, clock)
        scheduler.mark_ready()
        scheduler.enqueue(CommandRequest("*SET:DISPLAY OFF"))
        scheduler.tick()
        scheduler.handle_reply(parse_line("ERROR PARAM"))
        self.assertIsNone(scheduler.current_request_text())


if __name__ == "__main__":
    unittest.main()
