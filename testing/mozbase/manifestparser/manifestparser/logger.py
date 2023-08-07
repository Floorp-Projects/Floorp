# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os


class Logger(object):
    """
    ManifestParser needs to ensure a singleton for mozlog as documented here:

    https://firefox-source-docs.mozilla.org/mozbase/mozlog.html#mozlog-structured-logging-for-test-output

    Logging is threadsafe, with access to handlers protected by a
    threading.Lock. However it is not process-safe. This means that
    applications using multiple processes, e.g. via the multiprocessing
    module, should arrange for all logging to happen in a single process.

    The test:
    `testing/mochitest/tests/python/test_mochitest_integration.py::test_output_testfile_in_dupe_manifests`
    creates two ManifestParser instances and runs them at the same
    tripping over the condition (above) resulting in this exception:

    [task 2023-08-02T17:16:41.636Z]   File "/builds/worker/checkouts/gecko/testing/mozbase/mozlog/mozlog/handlers/base.py", line 113, in __call__
    [task 2023-08-02T17:16:41.636Z]     self.stream.write(formatted)
    [task 2023-08-02T17:16:41.636Z] ValueError: I/O operation on closed file
    """

    logger = None
    CI = False  # True if we are running in CI

    def __init__(self):
        "Lazily will create an instance of mozlog"
        pass

    def _initialize(self):
        "Creates an instance of mozlog, if needed"
        if "TASK_ID" in os.environ:
            Logger.CI = True  # We are running in CI
        else:
            Logger.CI = False
        if Logger.logger is None:
            component = "manifestparser"
            import mozlog

            Logger.logger = mozlog.get_default_logger(component)
            if Logger.logger is None:
                Logger.logger = mozlog.unstructured.getLogger(component)

    def critical(self, *args, **kwargs):
        self._initialize()
        Logger.logger.critical(*args, **kwargs)

    def debug(self, *args, **kwargs):
        self._initialize()
        Logger.logger.debug(*args, **kwargs)

    def debug_ci(self, *args, **kwargs):
        "Log to INFO level in CI else DEBUG level"
        self._initialize()
        if Logger.CI:
            Logger.logger.info(*args, **kwargs)
        else:
            Logger.logger.debug(*args, **kwargs)

    def error(self, *args, **kwargs):
        self._initialize()
        Logger.logger.error(*args, **kwargs)

    def info(self, *args, **kwargs):
        self._initialize()
        Logger.logger.info(*args, **kwargs)

    def warning(self, *args, **kwargs):
        self._initialize()
        Logger.logger.warning(*args, **kwargs)
