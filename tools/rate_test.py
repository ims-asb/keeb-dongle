#!/usr/bin/env python3
"""
rate_test.py - count HID input reports per second from the dongle's
N96_TEST_MODE firmware (VID 0xCAFE, PID 0x0097 by default).

What it measures: how many 16-byte reports per second reach this process
through hidapi, and whether the 32-bit sequence counter in bytes 0..3 has
gaps. In test mode the firmware queues a new report whenever the USB
endpoint is free, so the rate is bounded by the endpoint's polling
interval (bInterval=1: 125 us at High Speed, 1 ms at Full Speed) AND by the
host's controller, OS and hidapi. A result below 8000/s does not by itself
say which of those is the limit; a result near 1000/s suggests Full Speed
enumeration (check with lsusb -t / USBTreeView, see docs/TESTING.md).

This measures the USB leg only. It says nothing about RF latency and is not
an end-to-end number.

Requires: pip install hidapi   (only for talking to real hardware; the
counting logic has no dependencies and is unit-tested in
tests/host/test_rate_tool.py)
"""
import argparse
import struct
import sys
import time

REPORT_LEN = 16
DEFAULT_VID = 0xCAFE
DEFAULT_PID = 0x0097


class RateStats:
    """Feed it (report bytes, timestamp) pairs; it tracks rate and sequence gaps."""

    def __init__(self, interval=1.0):
        self.interval = interval
        self.total = 0
        self.missed = 0          # sequence numbers skipped (reports the host never saw)
        self.repeats = 0         # same or older sequence number seen again
        self.malformed = 0       # wrong length
        self.last_seq = None
        self.t_first = None
        self.t_last = None
        self._win_start = None
        self._win_count = 0
        self._win_missed = 0
        self.window_rates = []   # completed per-interval rates (reports/s)

    def feed(self, data, now):
        """Returns a completed-interval line to print, or None."""
        data = bytes(data)
        # Some hidapi backends prepend a report-ID byte (0) for ID-less devices.
        if len(data) == REPORT_LEN + 1 and data[0] == 0:
            data = data[1:]
        if len(data) != REPORT_LEN:
            self.malformed += 1
            return None
        (seq,) = struct.unpack_from("<I", data, 0)
        if self.t_first is None:
            self.t_first = now
            self._win_start = now
        self.t_last = now
        self.total += 1
        self._win_count += 1
        if self.last_seq is not None:
            delta = (seq - self.last_seq) & 0xFFFFFFFF
            if delta == 0 or delta > 0x7FFFFFFF:
                self.repeats += 1
            elif delta > 1:
                self.missed += delta - 1
                self._win_missed += delta - 1
        self.last_seq = seq
        if now - self._win_start >= self.interval:
            elapsed = now - self._win_start
            rate = self._win_count / elapsed
            self.window_rates.append(rate)
            line = "%8.1f reports/s   missed=%d (this interval) total=%d" % (
                rate, self._win_missed, self.total)
            self._win_start = now
            self._win_count = 0
            self._win_missed = 0
            return line
        return None

    def summary(self):
        if self.total < 2 or self.t_last <= self.t_first:
            return "not enough reports received to compute a rate (got %d)" % self.total
        avg = (self.total - 1) / (self.t_last - self.t_first)
        lines = [
            "reports received : %d" % self.total,
            "average rate     : %.1f reports/s (first to last report)" % avg,
        ]
        if self.window_rates:
            lines.append("per-interval     : min %.1f  max %.1f reports/s" % (
                min(self.window_rates), max(self.window_rates)))
        lines += [
            "sequence gaps    : %d reports missed" % self.missed,
            "repeats/reorder  : %d" % self.repeats,
            "malformed        : %d" % self.malformed,
        ]
        return "\n".join(lines)


def run(device, duration, interval, clock=time.perf_counter, out=print):
    """Read reports from `device` (anything with read(size, timeout_ms)) for `duration` s."""
    stats = RateStats(interval)
    t_end = clock() + duration
    while clock() < t_end:
        data = device.read(64, 100)
        now = clock()
        if data:
            line = stats.feed(data, now)
            if line:
                out(line)
    return stats


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--vid", type=lambda s: int(s, 0), default=DEFAULT_VID)
    ap.add_argument("--pid", type=lambda s: int(s, 0), default=DEFAULT_PID)
    ap.add_argument("--duration", type=float, default=10.0, help="seconds (default 10)")
    ap.add_argument("--interval", type=float, default=1.0, help="print interval seconds")
    ap.add_argument("--list", action="store_true", help="list HID devices and exit")
    args = ap.parse_args(argv)

    try:
        import hid  # hidapi
    except ImportError:
        print("hidapi not installed: pip install hidapi", file=sys.stderr)
        return 2

    if args.list:
        for d in hid.enumerate():
            print("%04x:%04x  %s / %s  usage_page=0x%04x" % (
                d["vendor_id"], d["product_id"], d.get("manufacturer_string"),
                d.get("product_string"), d.get("usage_page", 0)))
        return 0

    dev = hid.device()
    try:
        dev.open(args.vid, args.pid)
    except OSError as e:
        print("cannot open %04x:%04x (%s). On Linux you may need a udev rule or sudo; "
              "make sure the RATE TEST firmware is flashed (PID 0x0097)." % (args.vid, args.pid, e),
              file=sys.stderr)
        return 1
    print("opened: %s / %s" % (dev.get_manufacturer_string(), dev.get_product_string()))
    try:
        stats = run(dev, args.duration, args.interval)
    finally:
        dev.close()
    print(stats.summary())
    return 0


if __name__ == "__main__":
    sys.exit(main())
