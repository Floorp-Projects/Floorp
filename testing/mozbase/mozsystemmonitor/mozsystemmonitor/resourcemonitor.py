# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import multiprocessing
import sys
import time

# psutil will raise NotImplementedError if the platform is not supported.
try:
    import psutil
except Exception:
    psutil = None

from collections import (
    OrderedDict,
    namedtuple,
)

from contextlib import contextmanager


def _collect(pipe, poll_interval):
    """Collects system metrics.

    This is the main function for the background process. It collects
    data then forwards it on a pipe until told to stop.
    """

    data = []

    # Establish initial values.

    # We should ideally use a monotonic clock. However, Python 2.7 doesn't
    # make a monotonic clock available on all platforms. Python 3.3 does!
    last_time = time.time()
    io_last = psutil.disk_io_counters()
    cpu_last = psutil.cpu_times(True)
    swap_last = psutil.swap_memory()
    psutil.cpu_percent(None, True)

    sin_index = swap_last._fields.index('sin')
    sout_index = swap_last._fields.index('sout')

    sleep_interval = poll_interval

    while not pipe.poll(sleep_interval):
        io = psutil.disk_io_counters()
        cpu_times = psutil.cpu_times(True)
        cpu_percent = psutil.cpu_percent(None, True)
        virt_mem = psutil.virtual_memory()
        swap_mem = psutil.swap_memory()
        measured_end_time = time.time()

        # TODO Does this wrap? At 32 bits? At 64 bits?
        # TODO Consider patching "delta" API to upstream.
        io_diff = [v - io_last[i] for i, v in enumerate(io)]
        io_last = io

        cpu_diff = []
        for core, values in enumerate(cpu_times):
            cpu_diff.append([v - cpu_last[core][i] for i, v in
                enumerate(values)])

        cpu_last = cpu_times

        swap_entry = list(swap_mem)
        swap_entry[sin_index] = swap_mem.sin - swap_last.sin
        swap_entry[sout_index] = swap_mem.sout - swap_last.sout
        swap_last = swap_mem

        data.append((last_time, measured_end_time, io_diff, cpu_diff,
            cpu_percent, list(virt_mem), swap_entry))

        collection_overhead = time.time() - last_time - poll_interval
        last_time = measured_end_time
        sleep_interval = max(0, poll_interval - collection_overhead)

    for entry in data:
        pipe.send(entry)

    pipe.send(('done', None, None, None, None, None, None))
    pipe.close()
    sys.exit(0)


SystemResourceUsage = namedtuple('SystemResourceUsage',
    ['start', 'end', 'cpu_times', 'cpu_percent', 'io', 'virt', 'swap'])


class SystemResourceMonitor(object):
    """Measures system resources.

    Each instance measures system resources from the time it is started
    until it is finished. It does this on a separate process so it doesn't
    impact execution of the main Python process.

    Each instance is a one-shot instance. It cannot be used to record multiple
    durations.

    Aside from basic data gathering, the class supports basic analysis
    capabilities. You can query for data between ranges. You can also tell it
    when certain events occur and later grab data relevant to those events or
    plot those events on a timeline.

    The resource monitor works by periodically polling the state of the
    system. By default, it polls every second. This can be adjusted depending
    on the required granularity of the data and considerations for probe
    overhead. It tries to probe at the interval specified. However, variations
    should be expected. Fast and well-behaving systems should experience
    variations in the 1ms range. Larger variations may exist if the system is
    under heavy load or depending on how accurate socket polling is on your
    system.

    In its current implementation, data is not available until collection has
    stopped. This may change in future iterations.

    Usage
    =====

    monitor = SystemResourceMonitor()
    monitor.start()

    # Record that a single event in time just occurred.
    foo.do_stuff()
    monitor.record_event('foo_did_stuff')

    # Record that we're about to perform a possibly long-running event.
    with monitor.phase('long_job'):
        foo.do_long_running_job()

    # Stop recording. Currently we need to stop before data is available.
    monitor.stop()

    # Obtain the raw data for the entire probed range.
    print('CPU Usage:')
    for core in monitor.aggregate_cpu():
        print(core)

    # We can also request data corresponding to a specific phase.
    for data in monitor.phase_usage('long_job'):
        print(data.cpu_percent)
    """

    # The interprocess communication is complicated enough to warrant
    # explanation. To work around the Python GIL, we launch a separate
    # background process whose only job is to collect metrics. If we performed
    # collection in the main process, the polling interval would be
    # inconsistent if a long-running function were on the stack. Since the
    # child process is independent of the instantiating process, data
    # collection should be evenly spaced.
    #
    # As the child process collects data, it buffers it locally. When
    # collection stops, it flushes all that data to a pipe to be read by
    # the parent process.

    def __init__(self, poll_interval=1.0):
        """Instantiate a system resource monitor instance.

        The instance is configured with a poll interval. This is the interval
        between samples, in float seconds.
        """
        self.start_time = None
        self.end_time = None

        self.events = []
        self.phases = OrderedDict()

        self._active_phases = {}

        self._running = False
        self._stopped = False

        if psutil is None:
            return

        cpu_percent = psutil.cpu_percent(0.0, True)
        cpu_times = psutil.cpu_times(False)
        io = psutil.disk_io_counters()
        virt = psutil.virtual_memory()
        swap = psutil.swap_memory()

        self._cpu_cores = len(cpu_percent)
        self._cpu_times_type = type(cpu_times)
        self._cpu_times_len = len(cpu_times)
        self._io_type = type(io)
        self._io_len = len(io)
        self._virt_type = type(virt)
        self._virt_len = len(virt)
        self._swap_type = type(swap)
        self._swap_len = len(swap)

        self._pipe, child_pipe = multiprocessing.Pipe(True)

        self._process = multiprocessing.Process(None, _collect,
            args=(child_pipe, poll_interval))

    def __del__(self):
        if self._running:
            self._pipe.send(('terminate',))
            self._process.join()

    # Methods to control monitoring.

    def start(self):
        """Start measuring system-wide CPU resource utilization.

        You should only call this once per instance.
        """
        if psutil is None:
            return

        self._running = True
        self._process.start()

    def stop(self):
        """Stop measuring system-wide CPU resource utilization.

        You should call this if and only if you have called start(). You should
        always pair a stop() with a start().

        Currently, data is not available until you call stop().
        """
        if psutil is None:
            self._stopped = True
            return

        assert self._running
        assert not self._stopped

        self._pipe.send(('terminate'))
        self._running = False
        self._stopped = True

        self.measurements = []

        done = False

        while self._pipe.poll(1):
            start_time, end_time, io_diff, cpu_diff, cpu_percent, virt_mem, \
                swap_mem = self._pipe.recv()

            if start_time == 'done':
                done = True
                continue

            io = self._io_type(*io_diff)
            virt = self._virt_type(*virt_mem)
            swap = self._swap_type(*swap_mem)
            cpu_times = [self._cpu_times_type(*v) for v in cpu_diff]

            self.measurements.append(SystemResourceUsage(start_time, end_time,
                cpu_times, cpu_percent, io, virt, swap))

        self._process.join()
        assert done

        if len(self.measurements):
            self.start_time = self.measurements[0].start
            self.end_time = self.measurements[-1].end

    # Methods to record events alongside the monitored data.

    def record_event(self, name):
        """Record an event as occuring now.

        Events are actions that occur at a specific point in time. If you are
        looking for an action that has a duration, see the phase API below.
        """
        self.events.append((time.time(), name))

    @contextmanager
    def phase(self, name):
        """Context manager for recording an active phase."""
        self.begin_phase(name)
        yield
        self.finish_phase(name)

    def begin_phase(self, name):
        """Record the start of a phase.

        Phases are actions that have a duration. Multiple phases can be active
        simultaneously. Phases can be closed in any order.

        Keep in mind that if phases occur in parallel, it will become difficult
        to isolate resource utilization specific to individual phases.
        """
        assert name not in self._active_phases

        self._active_phases[name] = time.time()

    def finish_phase(self, name):
        """Record the end of a phase."""

        assert name in self._active_phases

        phase = (self._active_phases[name], time.time())
        self.phases[name] = phase
        del self._active_phases[name]

        return phase[1] - phase[0]

    # Methods to query data.

    def range_usage(self, start=None, end=None):
        """Obtain the usage data falling within the given time range.

        This is a generator of SystemResourceUsage.

        If no time range bounds are given, all data is returned.
        """
        if not self._stopped or self.start_time is None:
            return

        if start is None:
            start = self.start_time

        if end is None:
            end = self.end_time

        for entry in self.measurements:
            if entry.start < start:
                continue

            if entry.end > end:
                break

            yield entry

    def phase_usage(self, phase):
        """Obtain usage data for a specific phase.

        This is a generator of SystemResourceUsage.
        """
        time_start, time_end = self.phases[phase]

        return self.range_usage(time_start, time_end)

    def between_events_usage(self, start_event, end_event):
        """Obtain usage data between two point events.

        This is a generator of SystemResourceUsage.
        """
        start_time = None
        end_time = None

        for t, name in self.events:
            if name == start_event:
                start_time = t
            elif name == end_event:
                end_time = t

        if start_time is None:
            raise Exception('Could not find start event: %s' % start_event)

        if end_time is None:
            raise Exception('Could not find end event: %s' % end_event)

        return self.range_usage(start_time, end_time)

    def aggregate_cpu_percent(self, start=None, end=None, phase=None,
        per_cpu=True):
        """Obtain the aggregate CPU percent usage for a range.

        Returns a list of floats representing average CPU usage percentage per
        core if per_cpu is True (the default). If per_cpu is False, return a
        single percentage value.

        By default this will return data for the entire instrumented interval.
        If phase is defined, data for a named phase will be returned. If start
        and end are defined, these times will be fed into range_usage().
        """
        cpu = [[] for i in range(0, self._cpu_cores)]

        if phase:
            data = self.phase_usage(phase)
        else:
            data = self.range_usage(start, end)

        for usage in data:
            for i, v in enumerate(usage.cpu_percent):
                cpu[i].append(v)

        samples = len(cpu[0])

        if not samples:
            return None

        if per_cpu:
            return [sum(x) / samples for x in cpu]

        cores = [sum(x) for x in cpu]

        return sum(cores) / len(cpu) / samples

    def aggregate_cpu_times(self, start=None, end=None, phase=None,
        per_cpu=True):
        """Obtain the aggregate CPU times for a range.

        If per_cpu is True (the default), this returns a list of named tuples.
        Each tuple is as if it were returned by psutil.cpu_times(). If per_cpu
        is False, this returns a single named tuple of the aforementioned type.
        """
        empty = [0 for i in range(0, self._cpu_times_len)]
        cpu = [list(empty) for i in range(0, self._cpu_cores)]

        if phase:
            data = self.phase_usage(phase)
        else:
            data = self.range_usage(start, end)

        for usage in data:
            for i, core_values in enumerate(usage.cpu_times):
                for j, v in enumerate(core_values):
                    cpu[i][j] += v

        if per_cpu:
            return [self._cpu_times_type(*v) for v in cpu]

        sums = list(empty)
        for core in cpu:
            for i, v in enumerate(core):
                sums[i] += v

        return self._cpu_times_type(*sums)

    def aggregate_io(self, start=None, end=None, phase=None):
        """Obtain aggregate I/O counters for a range.

        Returns an iostat named tuple from psutil.
        """

        io = [0 for i in range(self._io_len)]

        if phase:
            data = self.phase_usage(phase)
        else:
            data = self.range_usage(start, end)

        for usage in data:
            for i, v in enumerate(usage.io):
                io[i] += v

        return self._io_type(*io)

    def min_memory_available(self, start=None, end=None, phase=None):
        """Return the minimum observed available memory number from a range.

        Returns long bytes of memory available.

        See psutil for notes on how this is calculated.
        """
        if phase:
            data = self.phase_usage(phase)
        else:
            data = self.range_usage(start, end)

        values = []

        for usage in data:
            values.append(usage.virt.available)

        return min(values)

    def max_memory_percent(self, start=None, end=None, phase=None):
        """Returns the maximum percentage of system memory used.

        Returns a float percentage. 1.00 would mean all system memory was in
        use at one point.
        """
        if phase:
            data = self.phase_usage(phase)
        else:
            data = self.range_usage(start, end)

        values = []

        for usage in data:
            values.append(usage.virt.percent)

        return max(values)

    def as_dict(self):
        """Convert the recorded data to a dict, suitable for serialization.

        The returned dict has the following keys:

          version - Integer version number being rendered. Currently 1.
          cpu_times_fields - A list of the names of the CPU times fields.
          io_fields - A list of the names of the I/O fields.
          virt_fields - A list of the names of the virtual memory fields.
          swap_fields - A list of the names of the swap memory fields.
          samples - A list of dicts containing low-level measurements.
          events - A list of lists representing point events. The inner list
            has 2 elements, the float wall time of the event and the string
            event name.
          phases - A list of dicts describing phases. Each phase looks a lot
            like an entry from samples (see below). Some phases may not have
            data recorded against them, so some keys may be None.

        Each entry in the sample list is a dict with the following keys:

          start - Float wall time this measurement began on.
          end - Float wall time this measurement ended on.
          io - List of numerics for I/O values.
          virt - List of numerics for virtual memory values.
          swap - List of numerics for swap memory values.
          cpu_percent - List of floats representing CPU percent on each core.
          cpu_times - List of lists. Main list is each core. Inner lists are
            lists of floats representing CPU times on that core.
          cpu_percent_mean - Float of mean CPU percent across all cores.
          cpu_times_sum - List of floats representing the sum of CPU times
            across all cores.
          cpu_times_total - Float representing the sum of all CPU times across
            all cores. This is useful for calculating the percent in each CPU
            time.
        """

        o = dict(
            version=1,
            cpu_times_fields=list(self._cpu_times_type._fields),
            io_fields=list(self._io_type._fields),
            virt_fields=list(self._virt_type._fields),
            swap_fields=list(self._swap_type._fields),
            samples=[],
            phases=[],
        )

        def populate_derived(e):
            if e['cpu_percent_cores']:
                e['cpu_percent_mean'] = sum(e['cpu_percent_cores']) / \
                    len(e['cpu_percent_cores'])
            else:
                e['cpu_percent_mean'] = None

            if e['cpu_times']:
                e['cpu_times_sum'] = [0.0] * self._cpu_times_len
                for i in range(0, self._cpu_times_len):
                    e['cpu_times_sum'][i] = sum(core[i] for core in e['cpu_times'])

                e['cpu_times_total'] = sum(e['cpu_times_sum'])


        for m in self.measurements:
            e = dict(
                start=m.start,
                end=m.end,
                io=list(m.io),
                virt=list(m.virt),
                swap=list(m.swap),
                cpu_percent_cores=list(m.cpu_percent),
                cpu_times=list(list(cpu) for cpu in m.cpu_times)
            )

            populate_derived(e)
            o['samples'].append(e)

        if o['samples']:
            o['start'] = o['samples'][0]['start']
            o['end'] = o['samples'][-1]['end']
            o['duration'] = o['end'] - o['start']
        else:
            o['start'] = None
            o['end'] = None
            o['duration'] = None

        o['events'] = [list(e) for e in self.events]

        for phase, v in self.phases.items():
            e = dict(
                name=phase,
                start=v[0],
                end=v[1],
                duration=v[1] - v[0],
                cpu_percent_cores=self.aggregate_cpu_percent(phase=phase),
                cpu_times=[list(c) for c in
                    self.aggregate_cpu_times(phase=phase)],
                io=list(self.aggregate_io(phase=phase)),
            )

            populate_derived(e)
            o['phases'].append(e)

        return o
