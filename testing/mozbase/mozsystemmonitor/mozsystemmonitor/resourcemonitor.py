# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import multiprocessing
import sys
import time
import warnings
from collections import OrderedDict, namedtuple
from contextlib import contextmanager


class PsutilStub(object):
    def __init__(self):
        self.sswap = namedtuple(
            "sswap", ["total", "used", "free", "percent", "sin", "sout"]
        )
        self.sdiskio = namedtuple(
            "sdiskio",
            [
                "read_count",
                "write_count",
                "read_bytes",
                "write_bytes",
                "read_time",
                "write_time",
            ],
        )
        self.pcputimes = namedtuple("pcputimes", ["user", "system"])
        self.svmem = namedtuple(
            "svmem",
            [
                "total",
                "available",
                "percent",
                "used",
                "free",
                "active",
                "inactive",
                "buffers",
                "cached",
            ],
        )

    def cpu_count(self, logical=True):
        return 0

    def cpu_percent(self, a, b):
        return [0]

    def cpu_times(self, percpu):
        if percpu:
            return [self.pcputimes(0, 0)]
        else:
            return self.pcputimes(0, 0)

    def disk_io_counters(self):
        return self.sdiskio(0, 0, 0, 0, 0, 0)

    def swap_memory(self):
        return self.sswap(0, 0, 0, 0, 0, 0)

    def virtual_memory(self):
        return self.svmem(0, 0, 0, 0, 0, 0, 0, 0, 0)


# psutil will raise NotImplementedError if the platform is not supported.
try:
    import psutil

    have_psutil = True
except Exception:
    try:
        # The PsutilStub should get us time intervals, at least
        psutil = PsutilStub()
    except Exception:
        psutil = None

    have_psutil = False


def get_disk_io_counters():
    try:
        io_counters = psutil.disk_io_counters()

        if io_counters is None:
            return PsutilStub().disk_io_counters()
    except RuntimeError:
        io_counters = PsutilStub().disk_io_counters()

    return io_counters


def _poll(pipe, poll_interval=0.1):
    """Wrap multiprocessing.Pipe.poll to hide POLLERR and POLLIN
    exceptions.

    multiprocessing.Pipe is not actually a pipe on at least Linux.
    That has an effect on the expected outcome of reading from it when
    the other end of the pipe dies, leading to possibly hanging on revc()
    below.
    """
    try:
        return pipe.poll(poll_interval)
    except Exception:
        # Poll might throw an exception even though there's still
        # data to read. That happens when the underlying system call
        # returns both POLLERR and POLLIN, but python doesn't tell us
        # about it. So assume there is something to read, and we'll
        # get an exception when trying to read the data.
        return True


def _collect(pipe, poll_interval):
    """Collects system metrics.

    This is the main function for the background process. It collects
    data then forwards it on a pipe until told to stop.
    """

    data = []

    try:
        # Establish initial values.

        last_time = time.monotonic()
        io_last = get_disk_io_counters()
        cpu_last = psutil.cpu_times(True)
        swap_last = psutil.swap_memory()
        psutil.cpu_percent(None, True)

        sin_index = swap_last._fields.index("sin")
        sout_index = swap_last._fields.index("sout")

        sleep_interval = poll_interval

        while not _poll(pipe, poll_interval=sleep_interval):
            io = get_disk_io_counters()
            cpu_times = psutil.cpu_times(True)
            cpu_percent = psutil.cpu_percent(None, True)
            virt_mem = psutil.virtual_memory()
            swap_mem = psutil.swap_memory()
            measured_end_time = time.monotonic()

            # TODO Does this wrap? At 32 bits? At 64 bits?
            # TODO Consider patching "delta" API to upstream.
            io_diff = [v - io_last[i] for i, v in enumerate(io)]
            io_last = io

            cpu_diff = []
            for core, values in enumerate(cpu_times):
                cpu_diff.append([v - cpu_last[core][i] for i, v in enumerate(values)])

            cpu_last = cpu_times

            swap_entry = list(swap_mem)
            swap_entry[sin_index] = swap_mem.sin - swap_last.sin
            swap_entry[sout_index] = swap_mem.sout - swap_last.sout
            swap_last = swap_mem

            data.append(
                (
                    last_time,
                    measured_end_time,
                    io_diff,
                    cpu_diff,
                    cpu_percent,
                    list(virt_mem),
                    swap_entry,
                )
            )

            collection_overhead = time.monotonic() - last_time - sleep_interval
            last_time = measured_end_time
            sleep_interval = max(poll_interval / 2, poll_interval - collection_overhead)

    except Exception as e:
        warnings.warn("_collect failed: %s" % e)

    finally:
        for entry in data:
            pipe.send(entry)

        pipe.send(("done", None, None, None, None, None, None))
        pipe.close()

    sys.exit(0)


SystemResourceUsage = namedtuple(
    "SystemResourceUsage",
    ["start", "end", "cpu_times", "cpu_percent", "io", "virt", "swap"],
)


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

    instance = None

    def __init__(self, poll_interval=1.0, metadata={}):
        """Instantiate a system resource monitor instance.

        The instance is configured with a poll interval. This is the interval
        between samples, in float seconds.
        """
        self.start_time = None
        self.end_time = None

        self.events = []
        self.markers = []
        self.phases = OrderedDict()

        self._active_phases = {}
        self._active_markers = {}

        self._running = False
        self._stopped = False
        self._process = None

        if psutil is None:
            return

        # This try..except should not be needed! However, some tools (like
        # |mach build|) attempt to load psutil before properly creating a
        # virtualenv by building psutil. As a result, python/psutil may be in
        # sys.path and its .py files may pick up the psutil C extension from
        # the system install. If the versions don't match, we typically see
        # failures invoking one of these functions.
        try:
            cpu_percent = psutil.cpu_percent(0.0, True)
            cpu_times = psutil.cpu_times(False)
            io = get_disk_io_counters()
            virt = psutil.virtual_memory()
            swap = psutil.swap_memory()
        except Exception as e:
            warnings.warn("psutil failed to run: %s" % e)
            return

        self._cpu_cores = len(cpu_percent)
        self._cpu_times_type = type(cpu_times)
        self._cpu_times_len = len(cpu_times)
        self._io_type = type(io)
        self._io_len = len(io)
        self._virt_type = type(virt)
        self._virt_len = len(virt)
        self._swap_type = type(swap)
        self._swap_len = len(swap)
        self.start_timestamp = time.time()

        self._pipe, child_pipe = multiprocessing.Pipe(True)

        self._process = multiprocessing.Process(
            target=_collect, args=(child_pipe, poll_interval)
        )
        self.poll_interval = poll_interval
        self.metadata = metadata

    def __del__(self):
        if self._running:
            self._pipe.send(("terminate",))
            self._process.join()

    # Methods to control monitoring.

    def start(self):
        """Start measuring system-wide CPU resource utilization.

        You should only call this once per instance.
        """
        if not self._process:
            return

        self._process.start()
        self._running = True
        self.start_time = time.monotonic()
        SystemResourceMonitor.instance = self

    def stop(self):
        """Stop measuring system-wide CPU resource utilization.

        You should call this if and only if you have called start(). You should
        always pair a stop() with a start().

        Currently, data is not available until you call stop().
        """
        if not self._process:
            self._stopped = True
            return

        self.stop_time = time.monotonic()
        assert not self._stopped

        try:
            self._pipe.send(("terminate",))
        except Exception:
            pass
        self._stopped = True

        self.measurements = []

        # The child process will send each data sample over the pipe
        # as a separate data structure. When it has finished sending
        # samples, it sends a special "done" message to indicate it
        # is finished.

        while _poll(self._pipe, poll_interval=0.1):
            try:
                (
                    start_time,
                    end_time,
                    io_diff,
                    cpu_diff,
                    cpu_percent,
                    virt_mem,
                    swap_mem,
                ) = self._pipe.recv()
            except Exception as e:
                warnings.warn("failed to receive data: %s" % e)
                # Assume we can't recover
                break

            # There should be nothing after the "done" message so
            # terminate.
            if start_time == "done":
                break

            try:
                io = self._io_type(*io_diff)
                virt = self._virt_type(*virt_mem)
                swap = self._swap_type(*swap_mem)
                cpu_times = [self._cpu_times_type(*v) for v in cpu_diff]

                self.measurements.append(
                    SystemResourceUsage(
                        start_time, end_time, cpu_times, cpu_percent, io, virt, swap
                    )
                )
            except Exception:
                # We also can't recover, but output the data that caused the exception
                warnings.warn(
                    "failed to read the received data: %s"
                    % str(
                        (
                            start_time,
                            end_time,
                            io_diff,
                            cpu_diff,
                            cpu_percent,
                            virt_mem,
                            swap_mem,
                        )
                    )
                )

                break

        # We establish a timeout so we don't hang forever if the child
        # process has crashed.
        if self._running:
            self._process.join(10)
            if self._process.is_alive():
                self._process.terminate()
                self._process.join(10)

        self._running = False
        SystemResourceUsage.instance = None
        self.end_time = time.monotonic()

    # Methods to record events alongside the monitored data.

    @staticmethod
    def record_event(name):
        """Record an event as occuring now.

        Events are actions that occur at a specific point in time. If you are
        looking for an action that has a duration, see the phase API below.
        """
        if SystemResourceMonitor.instance:
            SystemResourceMonitor.instance.events.append((time.monotonic(), name))

    @staticmethod
    def record_marker(name, start, end, text):
        """Record a marker with a duration and an optional text

        Markers are typically used to record when a single command happened.
        For actions with a longer duration that justifies tracking resource use
        see the phase API below.
        """
        if SystemResourceMonitor.instance:
            SystemResourceMonitor.instance.markers.append((name, start, end, text))

    @staticmethod
    def begin_marker(name, text, disambiguator=None):
        if SystemResourceMonitor.instance:
            id = name + ":" + text
            if disambiguator:
                id += ":" + disambiguator
            SystemResourceMonitor.instance._active_markers[id] = time.monotonic()

    @staticmethod
    def end_marker(name, text, disambiguator=None):
        if not SystemResourceMonitor.instance:
            return
        end = time.monotonic()
        id = name + ":" + text
        if disambiguator:
            id += ":" + disambiguator
        if not id in SystemResourceMonitor.instance._active_markers:
            return
        start = SystemResourceMonitor.instance._active_markers.pop(id)
        SystemResourceMonitor.instance.record_marker(name, start, end, text)

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

        self._active_phases[name] = time.monotonic()

    def finish_phase(self, name):
        """Record the end of a phase."""

        assert name in self._active_phases

        phase = (self._active_phases[name], time.monotonic())
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
            raise Exception("Could not find start event: %s" % start_event)

        if end_time is None:
            raise Exception("Could not find end event: %s" % end_event)

        return self.range_usage(start_time, end_time)

    def aggregate_cpu_percent(self, start=None, end=None, phase=None, per_cpu=True):
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
            return 0

        if per_cpu:
            # pylint --py3k W1619
            return [sum(x) / samples for x in cpu]

        cores = [sum(x) for x in cpu]

        # pylint --py3k W1619
        return sum(cores) / len(cpu) / samples

    def aggregate_cpu_times(self, start=None, end=None, phase=None, per_cpu=True):
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

          version - Integer version number being rendered. Currently 2.
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
          overall - A dict representing overall resource usage. This resembles
            a sample entry.
          system - Contains additional information about the system including
            number of processors and amount of memory.

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
            version=2,
            cpu_times_fields=list(self._cpu_times_type._fields),
            io_fields=list(self._io_type._fields),
            virt_fields=list(self._virt_type._fields),
            swap_fields=list(self._swap_type._fields),
            samples=[],
            phases=[],
            system={},
        )

        def populate_derived(e):
            if e["cpu_percent_cores"]:
                # pylint --py3k W1619
                e["cpu_percent_mean"] = sum(e["cpu_percent_cores"]) / len(
                    e["cpu_percent_cores"]
                )
            else:
                e["cpu_percent_mean"] = None

            if e["cpu_times"]:
                e["cpu_times_sum"] = [0.0] * self._cpu_times_len
                for i in range(0, self._cpu_times_len):
                    e["cpu_times_sum"][i] = sum(core[i] for core in e["cpu_times"])

                e["cpu_times_total"] = sum(e["cpu_times_sum"])

        def phase_entry(name, start, end):
            e = dict(
                name=name,
                start=start,
                end=end,
                duration=end - start,
                cpu_percent_cores=self.aggregate_cpu_percent(phase=name),
                cpu_times=[list(c) for c in self.aggregate_cpu_times(phase=name)],
                io=list(self.aggregate_io(phase=name)),
            )
            populate_derived(e)
            return e

        for m in self.measurements:
            e = dict(
                start=m.start,
                end=m.end,
                io=list(m.io),
                virt=list(m.virt),
                swap=list(m.swap),
                cpu_percent_cores=list(m.cpu_percent),
                cpu_times=list(list(cpu) for cpu in m.cpu_times),
            )

            populate_derived(e)
            o["samples"].append(e)

        if o["samples"]:
            o["start"] = o["samples"][0]["start"]
            o["end"] = o["samples"][-1]["end"]
            o["duration"] = o["end"] - o["start"]
            o["overall"] = phase_entry(None, o["start"], o["end"])
        else:
            o["start"] = None
            o["end"] = None
            o["duration"] = None
            o["overall"] = None

        o["events"] = [list(ev) for ev in self.events]

        for phase, v in self.phases.items():
            o["phases"].append(phase_entry(phase, v[0], v[1]))

        if have_psutil:
            o["system"].update(
                dict(
                    cpu_logical_count=psutil.cpu_count(logical=True),
                    cpu_physical_count=psutil.cpu_count(logical=False),
                    swap_total=psutil.swap_memory()[0],
                    vmem_total=psutil.virtual_memory()[0],
                )
            )

        return o

    def as_profile(self):
        profile_time = time.monotonic()
        start_time = self.start_time
        profile = {
            "meta": {
                "processType": 0,
                "product": "mach",
                "stackwalk": 0,
                "version": 27,
                "preprocessedProfileVersion": 47,
                "symbolicationNotSupported": True,
                "interval": self.poll_interval * 1000,
                "startTime": self.start_timestamp * 1000,
                "profilingStartTime": 0,
                "logicalCPUs": psutil.cpu_count(logical=True),
                "physicalCPUs": psutil.cpu_count(logical=False),
                "mainMemory": psutil.virtual_memory()[0],
                "markerSchema": [
                    {
                        "name": "Phase",
                        "tooltipLabel": "{marker.data.phase}",
                        "tableLabel": "{marker.name} — {marker.data.phase} — CPU time: {marker.data.cpuTime} ({marker.data.cpuPercent})",
                        "chartLabel": "{marker.data.phase}",
                        "display": ["marker-chart", "marker-table"],
                        "data": [
                            {
                                "key": "cpuTime",
                                "label": "CPU Time",
                                "format": "duration",
                            },
                            {
                                "key": "cpuPercent",
                                "label": "CPU Percent",
                                "format": "string",
                            },
                        ],
                    },
                    {
                        "name": "Text",
                        "tooltipLabel": "{marker.name}",
                        "tableLabel": "{marker.name} — {marker.data.text}",
                        "chartLabel": "{marker.data.text}",
                        "display": ["marker-chart", "marker-table"],
                        "data": [
                            {
                                "key": "text",
                                "label": "Description",
                                "format": "string",
                                "searchable": True,
                            }
                        ],
                    },
                    {
                        "name": "Mem",
                        "tooltipLabel": "{marker.name}",
                        "display": [],
                        "data": [
                            {"key": "used", "label": "Memory Used", "format": "bytes"},
                            {
                                "key": "cached",
                                "label": "Memory cached",
                                "format": "bytes",
                            },
                            {
                                "key": "buffers",
                                "label": "Memory buffers",
                                "format": "bytes",
                            },
                        ],
                        "graphs": [
                            {"key": "used", "color": "orange", "type": "line-filled"}
                        ],
                    },
                    {
                        "name": "IO",
                        "tooltipLabel": "{marker.name}",
                        "display": [],
                        "data": [
                            {
                                "key": "write_bytes",
                                "label": "Written",
                                "format": "bytes",
                            },
                            {
                                "key": "write_count",
                                "label": "Write count",
                                "format": "integer",
                            },
                            {"key": "read_bytes", "label": "Read", "format": "bytes"},
                            {
                                "key": "read_count",
                                "label": "Read count",
                                "format": "integer",
                            },
                        ],
                        "graphs": [
                            {"key": "read_bytes", "color": "green", "type": "bar"},
                            {"key": "write_bytes", "color": "red", "type": "bar"},
                        ],
                    },
                ],
                "usesOnlyOneStackType": True,
            },
            "libs": [],
            "threads": [
                {
                    "processType": "default",
                    "processName": "mach",
                    "processStartupTime": 0,
                    "processShutdownTime": None,
                    "registerTime": 0,
                    "unregisterTime": None,
                    "pausedRanges": [],
                    "name": "",
                    "isMainThread": False,
                    "pid": "0",
                    "tid": 0,
                    "samples": {
                        "weightType": "samples",
                        "weight": None,
                        "stack": [],
                        "time": [],
                        "length": 0,
                    },
                    "stringArray": ["(root)"],
                    "markers": {
                        "data": [],
                        "name": [],
                        "startTime": [],
                        "endTime": [],
                        "phase": [],
                        "category": [],
                        "length": 0,
                    },
                    "stackTable": {
                        "frame": [0],
                        "prefix": [None],
                        "category": [0],
                        "subcategory": [0],
                        "length": 1,
                    },
                    "frameTable": {
                        "address": [-1],
                        "inlineDepth": [0],
                        "category": [None],
                        "subcategory": [0],
                        "func": [0],
                        "nativeSymbol": [None],
                        "innerWindowID": [0],
                        "implementation": [None],
                        "line": [None],
                        "column": [None],
                        "length": 1,
                    },
                    "funcTable": {
                        "isJS": [False],
                        "relevantForJS": [False],
                        "name": [0],
                        "resource": [-1],
                        "fileName": [None],
                        "lineNumber": [None],
                        "columnNumber": [None],
                        "length": 1,
                    },
                    "resourceTable": {
                        "lib": [],
                        "name": [],
                        "host": [],
                        "type": [],
                        "length": 0,
                    },
                    "nativeSymbols": {
                        "libIndex": [],
                        "address": [],
                        "name": [],
                        "functionSize": [],
                        "length": 0,
                    },
                }
            ],
            "counters": [],
        }

        firstThread = profile["threads"][0]
        markers = firstThread["markers"]
        for key in self.metadata:
            profile["meta"][key] = self.metadata[key]

        def get_string_index(string):
            stringArray = firstThread["stringArray"]
            try:
                return stringArray.index(string)
            except ValueError:
                stringArray.append(string)
                return len(stringArray) - 1

        def add_marker(name_index, start, end, data, precision=None):
            # The precision argument allows setting how many digits after the
            # decimal point are desired.
            # For resource use samples where we sample with a timer, an integer
            # number of ms is good enough.
            # For short duration markers, the profiler front-end may show up to
            # 3 digits after the decimal point (ie. µs precision).
            markers["startTime"].append(round((start - start_time) * 1000, precision))
            if end is None:
                markers["endTime"].append(None)
                # 0 = Instant marker
                markers["phase"].append(0)
            else:
                markers["endTime"].append(round((end - start_time) * 1000, precision))
                # 1 = marker with start and end times, 2 = start but no end.
                markers["phase"].append(1)
            markers["category"].append(0)
            markers["name"].append(name_index)
            markers["data"].append(data)
            markers["length"] = markers["length"] + 1

        def format_percent(value):
            return str(round(value, 1)) + "%"

        samples = firstThread["samples"]
        samples["stack"].append(0)
        samples["time"].append(0)

        cpu_string_index = get_string_index("CPU Use")
        memory_string_index = get_string_index("Memory")
        io_string_index = get_string_index("IO")
        valid_cpu_fields = set()
        for m in self.measurements:
            # Ignore samples that are much too short.
            if m.end - m.start < self.poll_interval / 10:
                continue

            # Sample times
            samples["stack"].append(0)
            samples["time"].append(round((m.end - start_time) * 1000))

            # CPU
            markerData = {
                "type": "CPU",
                "cpuPercent": format_percent(
                    sum(list(m.cpu_percent)) / len(m.cpu_percent)
                ),
            }
            total = 0
            for field in ["nice", "user", "system", "iowait", "softirq"]:
                if hasattr(m.cpu_times[0], field):
                    total += sum(getattr(core, field) for core in m.cpu_times) / (
                        m.end - m.start
                    )
                    if total > 0:
                        valid_cpu_fields.add(field)
                    markerData[field] = round(total, 3)
            for field in ["nice", "user", "system", "iowait", "idle"]:
                if hasattr(m.cpu_times[0], field):
                    markerData[field + "_pct"] = format_percent(
                        100
                        * sum(getattr(core, field) for core in m.cpu_times)
                        / (m.end - m.start)
                        / len(m.cpu_times)
                    )
            add_marker(cpu_string_index, m.start, m.end, markerData)

            # Memory
            markerData = {"type": "Mem", "used": m.virt.used}
            if hasattr(m.virt, "cached"):
                markerData["cached"] = m.virt.cached
            if hasattr(m.virt, "buffers"):
                markerData["buffers"] = m.virt.buffers
            add_marker(memory_string_index, m.start, m.end, markerData)

            # IO
            markerData = {
                "type": "IO",
                "read_count": m.io.read_count,
                "read_bytes": m.io.read_bytes,
                "write_count": m.io.write_count,
                "write_bytes": m.io.write_bytes,
            }
            add_marker(io_string_index, m.start, m.end, markerData)
        samples["length"] = len(samples["stack"])

        # The marker schema for CPU markers should only contain graph
        # definitions for fields we actually have, or the profiler front-end
        # will detect missing data and skip drawing the track entirely.
        cpuSchema = {
            "name": "CPU",
            "tooltipLabel": "{marker.name}",
            "display": [],
            "data": [{"key": "cpuPercent", "label": "CPU Percent", "format": "string"}],
            "graphs": [],
        }
        cpuData = cpuSchema["data"]
        for field, label in {
            "user": "User %",
            "iowait": "IO Wait %",
            "system": "System %",
            "nice": "Nice %",
            "idle": "Idle %",
        }.items():
            if field in valid_cpu_fields or field == "idle":
                cpuData.append(
                    {"key": field + "_pct", "label": label, "format": "string"}
                )
        cpuGraphs = cpuSchema["graphs"]
        for field, color in {
            "softirq": "orange",
            "iowait": "red",
            "system": "grey",
            "user": "yellow",
            "nice": "blue",
        }.items():
            if field in valid_cpu_fields:
                cpuGraphs.append({"key": field, "color": color, "type": "bar"})
        profile["meta"]["markerSchema"].insert(0, cpuSchema)

        # Create markers for phases
        phase_string_index = get_string_index("Phase")
        for phase, v in self.phases.items():
            markerData = {"type": "Phase", "phase": phase}

            cpu_percent_cores = self.aggregate_cpu_percent(phase=phase)
            if cpu_percent_cores:
                markerData["cpuPercent"] = format_percent(
                    sum(cpu_percent_cores) / len(cpu_percent_cores)
                )

            cpu_times = [list(c) for c in self.aggregate_cpu_times(phase=phase)]
            cpu_times_sum = [0.0] * self._cpu_times_len
            for i in range(0, self._cpu_times_len):
                cpu_times_sum[i] = sum(core[i] for core in cpu_times)
            total_cpu_time_ms = sum(cpu_times_sum) * 1000
            if total_cpu_time_ms > 0:
                markerData["cpuTime"] = total_cpu_time_ms

            add_marker(phase_string_index, v[0], v[1], markerData, 3)

        # Add generic markers
        for name, start, end, text in self.markers:
            markerData = {"type": "Text"}
            if text:
                markerData["text"] = text
            add_marker(get_string_index(name), start, end, markerData, 3)
        if self.events:
            event_string_index = get_string_index("Event")
            for event_time, text in self.events:
                if text:
                    add_marker(
                        event_string_index,
                        event_time,
                        None,
                        {"type": "Text", "text": text},
                        3,
                    )

        # We may have spent some time generating this profile, and there might
        # also have been some time elapsed between stopping the resource
        # monitor, and the profile being created. These are hidden costs that
        # we should account for as best as possible, and the best we can do
        # is to make the profile contain information about this cost somehow.
        # We extend the profile end time up to now rather than self.end_time,
        # and add a phase covering that period of time.
        now = time.monotonic()
        profile["meta"]["profilingEndTime"] = round(
            (now - self.start_time) * 1000 + 0.0005, 3
        )
        markerData = {
            "type": "Phase",
            "phase": "teardown",
        }
        add_marker(phase_string_index, self.stop_time, now, markerData, 3)
        teardown_string_index = get_string_index("resourcemonitor")
        markerData = {
            "type": "Text",
            "text": "stop",
        }
        add_marker(teardown_string_index, self.stop_time, self.end_time, markerData, 3)
        markerData = {
            "type": "Text",
            "text": "as_profile",
        }
        add_marker(teardown_string_index, profile_time, now, markerData, 3)

        # Unfortunately, whatever the caller does with the profile (e.g. json)
        # or after that (hopefully, exit) is not going to be counted, but we
        # assume it's fast enough.
        return profile
