"""Tests for the counting logic in tools/rate_test.py (no hardware, no hidapi needed)."""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import rate_test  # noqa: E402


def report(seq):
    return struct.pack("<I", seq) + bytes((seq + i) & 0xFF for i in range(4, 16))


class FakeClock:
    def __init__(self):
        self.t = 0.0

    def __call__(self):
        return self.t


class FakeDevice:
    """Delivers `rate` reports per second of fake time, optionally skipping sequence numbers."""

    def __init__(self, clock, rate, skip=()):
        self.clock, self.rate, self.skip, self.seq = clock, rate, set(skip), 0

    def read(self, size, timeout_ms):
        self.clock.t += 1.0 / self.rate
        self.seq += 1
        while self.seq in self.skip:
            self.seq += 1
        return list(report(self.seq))


class RateStatsTests(unittest.TestCase):
    def test_steady_rate(self):
        clk = FakeClock()
        dev = FakeDevice(clk, 8000)
        lines = []
        stats = rate_test.run(dev, 3.0, 1.0, clock=clk, out=lines.append)
        self.assertGreaterEqual(len(lines), 2)
        for r in stats.window_rates:
            self.assertAlmostEqual(r, 8000, delta=8000 * 0.01)
        self.assertEqual(stats.missed, 0)
        self.assertEqual(stats.repeats, 0)
        self.assertIn("average rate", stats.summary())

    def test_full_speed_like_rate(self):
        clk = FakeClock()
        stats = rate_test.run(FakeDevice(clk, 1000), 2.0, 1.0, clock=clk, out=lambda s: None)
        self.assertAlmostEqual(stats.window_rates[0], 1000, delta=10)

    def test_gap_counting(self):
        s = rate_test.RateStats()
        for i, seq in enumerate([1, 2, 3, 7, 8, 12]):
            s.feed(report(seq), i * 0.001)
        self.assertEqual(s.missed, (7 - 3 - 1) + (12 - 8 - 1))

    def test_gap_via_device(self):
        clk = FakeClock()
        stats = rate_test.run(FakeDevice(clk, 1000, skip=[10, 11, 50]), 0.2, 1.0, clock=clk, out=lambda s: None)
        self.assertEqual(stats.missed, 3)

    def test_wraparound(self):
        s = rate_test.RateStats()
        s.feed(report(0xFFFFFFFE), 0.0)
        s.feed(report(0xFFFFFFFF), 0.001)
        s.feed(report(0x00000000), 0.002)
        s.feed(report(0x00000002), 0.003)
        self.assertEqual(s.missed, 1)
        self.assertEqual(s.repeats, 0)

    def test_repeat_and_reorder(self):
        s = rate_test.RateStats()
        for seq in [5, 6, 6, 5, 7]:
            s.feed(report(seq), 0.0)
        self.assertEqual(s.repeats, 2)

    def test_report_id_prefix_stripped(self):
        s = rate_test.RateStats()
        s.feed(b"\x00" + report(1), 0.0)
        s.feed(b"\x00" + report(2), 0.001)
        self.assertEqual(s.malformed, 0)
        self.assertEqual(s.missed, 0)

    def test_malformed(self):
        s = rate_test.RateStats()
        s.feed(b"\x01\x02\x03", 0.0)
        s.feed(report(1) + b"\xff\xff", 0.0)
        self.assertEqual(s.malformed, 2)
        self.assertEqual(s.total, 0)
        self.assertIn("not enough", s.summary())

    def test_matches_firmware_report_layout(self):
        # Same construction as n96_test_report() in dongle/src/report.c
        # (also exercised in test_report.c): bytes 0..3 LE seq, byte i = seq + i.
        r = report(0x01020304)
        self.assertEqual(r[:4], bytes([4, 3, 2, 1]))
        self.assertEqual(r[4], (0x01020304 + 4) & 0xFF)
        self.assertEqual(len(r), rate_test.REPORT_LEN)


if __name__ == "__main__":
    unittest.main()
