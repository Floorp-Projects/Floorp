# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import multiprocessing
import time
import unittest

import mozunit

try:
    import psutil
except ImportError:
    psutil = None

from mozsystemmonitor.resourcemonitor import (
    SystemResourceMonitor,
    SystemResourceUsage,
)


@unittest.skipIf(psutil is None, 'Resource monitor requires psutil.')
class TestResourceMonitor(unittest.TestCase):

    def test_basic(self):
        monitor = SystemResourceMonitor(poll_interval=0.5)

        monitor.start()
        time.sleep(3)

        monitor.stop()

        data = list(monitor.range_usage())
        self.assertGreater(len(data), 3)

        self.assertIsInstance(data[0], SystemResourceUsage)

    def test_empty(self):
        monitor = SystemResourceMonitor(poll_interval=2.0)
        monitor.start()
        monitor.stop()

        data = list(monitor.range_usage())
        self.assertEqual(len(data), 0)

    def test_phases(self):
        monitor = SystemResourceMonitor(poll_interval=0.25)

        monitor.start()
        time.sleep(1)

        with monitor.phase('phase1'):
            time.sleep(1)

            with monitor.phase('phase2'):
                time.sleep(1)

        monitor.stop()

        self.assertEqual(len(monitor.phases), 2)
        self.assertEqual(['phase2', 'phase1'], monitor.phases.keys())

        all = list(monitor.range_usage())
        data1 = list(monitor.phase_usage('phase1'))
        data2 = list(monitor.phase_usage('phase2'))

        self.assertGreater(len(all), len(data1))
        self.assertGreater(len(data1), len(data2))

        # This could fail if time.time() takes more than 0.1s. It really
        # shouldn't.
        self.assertAlmostEqual(data1[-1].end, data2[-1].end, delta=0.25)

    def test_no_data(self):
        monitor = SystemResourceMonitor()

        data = list(monitor.range_usage())
        self.assertEqual(len(data), 0)

    def test_events(self):
        monitor = SystemResourceMonitor(poll_interval=0.25)

        monitor.start()
        time.sleep(0.5)

        t0 = time.time()
        monitor.record_event('t0')
        time.sleep(0.5)

        monitor.record_event('t1')
        time.sleep(0.5)
        monitor.stop()

        events = monitor.events
        self.assertEqual(len(events), 2)

        event = events[0]

        self.assertEqual(event[1], 't0')
        self.assertAlmostEqual(event[0], t0, delta=0.25)

        data = list(monitor.between_events_usage('t0', 't1'))
        self.assertGreater(len(data), 0)

    def test_aggregate_cpu(self):
        monitor = SystemResourceMonitor(poll_interval=0.25)

        monitor.start()
        time.sleep(1)
        monitor.stop()

        values = monitor.aggregate_cpu_percent()
        self.assertIsInstance(values, list)
        self.assertEqual(len(values), multiprocessing.cpu_count())
        for v in values:
            self.assertIsInstance(v, float)

        value = monitor.aggregate_cpu_percent(per_cpu=False)
        self.assertIsInstance(value, float)

        values = monitor.aggregate_cpu_times()
        self.assertIsInstance(values, list)
        self.assertGreater(len(values), 0)
        self.assertTrue(hasattr(values[0], 'user'))

        t = type(values[0])

        value = monitor.aggregate_cpu_times(per_cpu=False)
        self.assertIsInstance(value, t)

    def test_aggregate_io(self):
        monitor = SystemResourceMonitor(poll_interval=0.25)

        # There's really no easy way to ensure I/O occurs. For all we know
        # reads and writes will all be serviced by the page cache.
        monitor.start()
        time.sleep(1.0)
        monitor.stop()

        values = monitor.aggregate_io()
        self.assertTrue(hasattr(values, 'read_count'))

    def test_memory(self):
        monitor = SystemResourceMonitor(poll_interval=0.25)

        monitor.start()
        time.sleep(1.0)
        monitor.stop()

        v = monitor.min_memory_available()
        self.assertIsInstance(v, long)

        v = monitor.max_memory_percent()
        self.assertIsInstance(v, float)

    def test_as_dict(self):
        monitor = SystemResourceMonitor(poll_interval=0.25)

        monitor.start()
        time.sleep(0.1)
        monitor.begin_phase('phase1')
        monitor.record_event('foo')
        time.sleep(0.1)
        monitor.begin_phase('phase2')
        monitor.record_event('bar')
        time.sleep(0.2)
        monitor.finish_phase('phase1')
        time.sleep(0.2)
        monitor.finish_phase('phase2')
        time.sleep(0.4)
        monitor.stop()

        d = monitor.as_dict()

        self.assertEqual(d['version'], 2)
        self.assertEqual(len(d['events']), 2)
        self.assertEqual(len(d['phases']), 2)
        self.assertIn('system', d)
        self.assertIsInstance(d['system'], dict)
        self.assertIsInstance(d['overall'], dict)
        self.assertIn('duration', d['overall'])
        self.assertIn('cpu_times', d['overall'])


if __name__ == '__main__':
    mozunit.main()
