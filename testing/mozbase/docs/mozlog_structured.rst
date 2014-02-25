:mod:`mozlog.structured` --- Structured logging for test output
===============================================================

``mozlog.structured`` is a library designed for logging the execution
and results of test harnesses. The canonical output format is JSON,
with one line of JSON per log entry. It is *not* based on the stdlib
logging module, although it shares several concepts with this module.

One notable difference between this module and the standard logging
module is the way that loggers are created. The structured logging
module does not require that loggers with a specific name are
singleton objects accessed through a factory function. Instead the
``StructuredLogger`` constructor may be used directly. However all
loggers with the same name share the same internal state (the "Borg"
pattern). In particular the list of handlers functions is the same for
all loggers with the same name.

Logging is threadsafe, with access to handlers protected by a
``threading.Lock``. However it is `not` process-safe. This means that
applications using multiple processes, e.g. via the
``multiprocessing`` module, should arrange for all logging to happen in
a single process.

Data Format
-----------

Structured loggers produce messages in a simple format designed to be
compatible with the JSON data model. Each message is a single object,
with the type of message indicated by the ``action`` key. It is
intended that the set of ``action`` values be closed; where there are
use cases for additional values they should be integrated into this
module rather than extended in an ad-hoc way. The set of keys present
on on all messages is:

``action``
  The type of the message (string).

``time``
  The timestamp of the message in ms since the epoch (int).

``thread``
  The name of the thread emitting the message (string).

``pid``
  The pid of the process creating the message (int).

``source``
  Name of the logger creating the message (string).

For each ``action`` there are is a further set of specific fields
describing the details of the event that caused the message to be
emitted:

``suite_start``
  Emitted when the testsuite starts running.

  ``tests``
    A list of test_ids (list).

``suite_end``
  Emitted when the testsuite is finished and no more results will be produced.

``test_start``
  Emitted when a test is being started.

  ``test``
    A unique id for the test (string or list of strings).

``test_status``
  Emitted for a test which has subtests to record the result of a
  single subtest

  ``test``
    The same unique id for the test as in the ``test_start`` message.

  ``subtest``
    Name of the subtest (string).

  ``status``
    Result of the test (string enum; ``PASS``, ``FAIL``, ``TIMEOUT``,
    ``NOTRUN``)

  ``expected``
    Expected result of the test. Omitted if the expected result is the
    same as the actual result (string enum, same as ``status``).

``test_end``
  Emitted to give the result of a test with no subtests, or the status
  of the overall file when there are subtests.

  ``test``
    The same unique id for the test as in the ``test_start`` message.

  ``status``
    Either result of the test (if there are no subtests) in which case
    (string enum ``PASS``, ``FAIL``, ``TIMEOUT``, ``CRASH``,
    ``ASSERT``, ``SKIP``) or the status of the overall file where
    there are subtests (string enum ``OK``, ``ERROR``, ``TIMEOUT``,
    ``CRASH``, ``ASSERT``, ``SKIP``).

  ``expected``
    The expected status, or emitted if the expected status matches the
    actual status (string enum, same as ``status``).

``process_output``
  Output from a managed subprocess.

  ``process``
  pid of the subprocess.

  ``command``
  Command used to launch the subprocess.

  ``data``
  Data output by the subprocess.

``log``
  General human-readable logging message, used to debug the harnesses
  themselves rather than to provide input to other tools.

  ``level``
    Level of the log message (string enum ``CRITICAL``, ``ERROR``,
    ``WARNING``, ``INFO``, ``DEBUG``).

  ``message``
    Text of the log message.

StructuredLogger Objects
------------------------

.. automodule:: mozlog.structured.structuredlog

.. autoclass:: StructuredLogger
   :members: add_handler, remove_handler, handlers, suite_start,
             suite_end, test_start, test_status, test_end,
             process_output, critical, error, warning, info, debug

.. autoclass:: StructuredLogFileLike
  :members:

Handlers
--------

A handler is a callable that is called for each log message produced
and is responsible for handling the processing of that
message. The typical example of this is a ``StreamHandler`` which takes
a log message, invokes a formatter which converts the log to a string,
and writes it to a file.

.. automodule:: mozlog.structured.handlers

.. autoclass:: StreamHandler
  :members:

.. autoclass:: LogLevelFilter
  :members:

Formatters
----------

Formatters are callables that take a log message, and return either a
string representation of that message, or ``None`` if that message
should not appear in the output. This allows formatters to both
exclude certain items and create internal buffers of the output so
that, for example, a single string might be returned for a
``test_end`` message indicating the overall result of the test,
including data provided in the ``test_status`` messages.

Formatter modules are written so that they can take raw input on stdin
and write formatted output on stdout. This allows the formatters to be
invoked as part of a command line for post-processing raw log files.

.. automodule:: mozlog.structured.formatters.base

.. autoclass:: BaseFormatter
  :members:

.. automodule:: mozlog.structured.formatters.unittest

.. autoclass:: UnittestFormatter
  :members:

.. automodule:: mozlog.structured.formatters.xunit

.. autoclass:: XUnitFormatter
  :members:

.. automodule:: mozlog.structured.formatters.html

.. autoclass:: HTMLFormatter
  :members:

.. automodule:: mozlog.structured.formatters.machformatter

.. autoclass:: MachFormatter
  :members:

.. autoclass:: MachTerminalFormatter
  :members:

Processing Log Files
--------------------

The ``mozlog.structured.reader`` module provides utilities for working
with structured log files.

.. automodule:: mozlog.structured.reader
  :members:

Integration with argparse
-------------------------

The `mozlog.structured.commandline` module provides integration with
the `argparse` module to provide uniform logging-related command line
arguments to programs using `mozlog.structured`. Each known formatter
gets a command line argument of the form ``--log-{name}``, which takes
the name of a file to log to with that format of `-` to indicate stdout.

.. automodule:: mozlog.structured.commandline
  :members:

Examples
--------

Log to stdout::

    from mozlog.structured import structuredlog
    from mozlog.structured import handlers, formatters
    logger = structuredlog.StructuredLogger("my-test-suite")
    logger.add_handler(handlers.StreamHandler(sys.stdout,
                                              formatters.JSONFormatter()))
    logger.suite_start(["test-id-1"])
    logger.test_start("test-id-1")
    logger.info("This is a message with action='LOG' and level='INFO'")
    logger.test_status("test-id-1", "subtest-1", "PASS")
    logger.test_end("test-id-1", "OK")
    logger.suite_end()


Populate an ``argparse.ArgumentParser`` with logging options, and
create a logger based on the value of those options, defaulting to
JSON output on stdout if nothing else is supplied::

   import argparse
   from mozlog.structured import commandline

   parser = argparse.ArgumentParser()
   # Here one would populate the parser with other options
   commandline.add_logging_group(parser)

   args = parser.parse_args()
   logger = commandline.setup_logging("testsuite-name", args, {"raw": sys.stdout})

Count the number of tests that timed out in a testsuite::

   from mozlog.structured import reader

   class TestCounter(object):
       def count(filename):
           self.count = 0
           with open(filename) as f:
               reader.map_action(reader.read(f),
                                 {"test_end":self.process_test_end})
          return self.count

       def process_test_end(self, data):
           if data["status"] == "TIMEOUT":
               self.count += 1

   print TestCounter().count("my_test_run.log")
