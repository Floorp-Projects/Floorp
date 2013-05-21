# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from abc import (
    ABCMeta,
    abstractmethod,
)

import os
import sys
import time

from mach.mixin.logging import LoggingMixin

from ..frontend.data import (
    ReaderSummary,
    SandboxDerived,
)
from .configenvironment import ConfigEnvironment


class BackendConsumeSummary(object):
    """Holds state about what a backend did.

    This is used primarily to print a summary of what the backend did
    so people know what's going on.
    """
    def __init__(self):
        # How many moz.build files were read. This includes included files.
        self.mozbuild_count = 0

        # The number of derived objects from the read moz.build files.
        self.object_count = 0

        # The total wall time this backend spent consuming objects. If
        # the iterable passed into consume() is a generator, this includes the
        # time spent to read moz.build files.
        self.wall_time = 0.0

        # CPU time spent by during the interval captured by wall_time.
        self.cpu_time = 0.0

        # The total wall time spent executing moz.build files. This is just
        # the read and execute time. It does not cover consume time.
        self.mozbuild_execution_time = 0.0

        # The total wall time spent in the backend. This counts the time the
        # backend writes out files, etc.
        self.backend_execution_time = 0.0

        # How much wall time the system spent doing other things. This is
        # wall_time - mozbuild_execution_time - backend_execution_time.
        self.other_time = 0.0

    @property
    def reader_summary(self):
        return 'Finished reading {:d} moz.build files into {:d} descriptors in {:.2f}s'.format(
            self.mozbuild_count, self.object_count,
            self.mozbuild_execution_time)

    @property
    def backend_summary(self):
        return 'Backend executed in {:.2f}s'.format(self.backend_execution_time)

    def backend_detailed_summary(self):
        """Backend summary to be supplied by BuildBackend implementations."""
        return None

    @property
    def total_summary(self):
        efficiency_value = self.cpu_time / self.wall_time if self.wall_time else 100
        return 'Total wall time: {:.2f}s; CPU time: {:.2f}s; Efficiency: {:.0%}'.format(
            self.wall_time, self.cpu_time, efficiency_value)

    def summaries(self):
        yield self.reader_summary
        yield self.backend_summary

        detailed = self.backend_detailed_summary()
        if detailed:
            yield detailed

        yield self.total_summary


class BuildBackend(LoggingMixin):
    """Abstract base class for build backends.

    A build backend is merely a consumer of the build configuration (the output
    of the frontend processing). It does something with said data. What exactly
    is the discretion of the specific implementation.
    """

    __metaclass__ = ABCMeta

    def __init__(self, environment):
        assert isinstance(environment, ConfigEnvironment)

        self.populate_logger()

        self.environment = environment
        self.summary = BackendConsumeSummary()

        # Files whose modification should cause a new read and backend
        # generation.
        self.backend_input_files = set()

        # Pull in Python files for this package as dependencies so backend
        # regeneration occurs if any of the code affecting it changes.
        for name, module in sys.modules.items():
            if not module or not name.startswith('mozbuild'):
                continue

            p = module.__file__

            # We need to look at the actual source files as opposed to derived
            # because there may be nothing loading these modules at build time.
            # Assuming each .pyc comes from a .py file in the same directory is
            # not a safe assumption. Hence the assert to catch future changes
            # in behavior. A better solution likely involves loading all
            # mozbuild modules at the top of the build to force .pyc
            # generation.
            if p.endswith('.pyc'):
                p = p[0:-1]

            assert os.path.exists(p)

            self.backend_input_files.add((os.path.abspath(p)))

        self._environments = {}
        self._environments[environment.topobjdir] = environment

        self._init()

    def _init():
        """Hook point for child classes to perform actions during __init__.

        This exists so child classes don't need to implement __init__.
        """

    def get_environment(self, obj):
        """Obtain the ConfigEnvironment for a specific object.

        This is used to support external source directories which operate in
        their own topobjdir and have their own ConfigEnvironment.

        This is somewhat hacky and should be considered for rewrite if external
        project integration is rewritten.
        """
        environment = self._environments.get(obj.topobjdir, None)
        if not environment:
            config_status = os.path.join(obj.topobjdir, 'config.status')

            environment = ConfigEnvironment.from_config_status(config_status)
            self._environments[obj.topobjdir] = environment

        return environment

    def consume(self, objs):
        """Consume a stream of TreeMetadata instances.

        This is the main method of the interface. This is what takes the
        frontend output and does something with it.

        Child classes are not expected to implement this method. Instead, the
        base class consumes objects and calls methods (possibly) implemented by
        child classes.
        """
        cpu_start = time.clock()
        time_start = time.time()
        backend_time = 0.0

        for obj in objs:
            self.summary.object_count += 1
            obj_start = time.time()
            self.consume_object(obj)
            backend_time += time.time() - obj_start

            if isinstance(obj, SandboxDerived):
                self.backend_input_files |= obj.sandbox_all_paths

            if isinstance(obj, ReaderSummary):
                self.summary.mozbuild_count = obj.total_file_count
                self.summary.mozbuild_execution_time = obj.total_execution_time

        # Write out a file indicating when this backend was last generated.
        age_file = os.path.join(self.environment.topobjdir,
            'backend.%s.built' % self.__class__.__name__)
        with open(age_file, 'a'):
            os.utime(age_file, None)

        finished_start = time.time()
        self.consume_finished()
        backend_time += time.time() - finished_start

        self.summary.cpu_time = time.clock() - cpu_start
        self.summary.wall_time = time.time() - time_start
        self.summary.backend_execution_time = backend_time
        self.summary.other_time = self.summary.wall_time - \
            self.summary.mozbuild_execution_time - \
            self.summary.backend_execution_time

        return self.summary

    @abstractmethod
    def consume_object(self, obj):
        """Consumes an individual TreeMetadata instance.

        This is the main method used by child classes to react to build
        metadata.
        """

    def consume_finished(self):
        """Called when consume() has completed handling all objects."""

