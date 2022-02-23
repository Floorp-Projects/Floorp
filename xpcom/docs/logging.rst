Gecko Logging
=============

A minimal C++ logging framework is provided for use in core Gecko code. It is enabled for all builds and is thread-safe.

Declaring a Log Module
----------------------

``LazyLogModule`` defers the creation the backing ``LogModule`` in a thread-safe manner and is the preferred method to declare a log module. Multiple ``LazyLogModules`` with the same name can be declared, all will share the same backing ``LogModule``. This makes it much simpler to share a log module across multiple translation units. ``LazyLogLodule`` provides a conversion operator to ``LogModule*`` and is suitable for passing into the logging macros detailed below.

Note: Log module names can only contain specific characters. The first character must be a lowercase or uppercase ASCII char, underscore, dash, or dot. Subsequent characters may be any of those, or an ASCII digit.

.. code-block:: c++

  #include "mozilla/Logging.h"

  static mozilla::LazyLogModule sFooLog("foo");


Logging interface
-----------------

A basic interface is provided in the form of 2 macros and an enum class.

+----------------------------------------+----------------------------------------------------------------------------+
| MOZ_LOG(module, level, message)        | Outputs the given message if the module has the given log level enabled:   |
|                                        |                                                                            |
|                                        | *   module: The log module to use.                                         |
|                                        | *   level: The log level of the message.                                   |
|                                        | *   message: A printf-style message to output. Must be enclosed in         |
|                                        |     parentheses.                                                           |
+----------------------------------------+----------------------------------------------------------------------------+
| MOZ_LOG_TEST(module, level)            | Checks if the module has the given level enabled:                          |
|                                        |                                                                            |
|                                        | *    module: The log module to use.                                        |
|                                        | *    level: The output level of the message.                               |
+----------------------------------------+----------------------------------------------------------------------------+


+-----------+---------------+-----------------------------------------------------------------------------------------+
| Log Level | Numeric Value | Purpose                                                                                 |
+===========+===============+=========================================================================================+
| Disabled  |      0        | Indicates logging is disabled. This should not be used directly in code.                |
+-----------+---------------+-----------------------------------------------------------------------------------------+
| Error     |      1        | An error occurred, generally something you would consider asserting in a debug build.   |
+-----------+---------------+-----------------------------------------------------------------------------------------+
| Warning   |      2        | A warning often indicates an unexpected state.                                          |
+-----------+---------------+-----------------------------------------------------------------------------------------+
| Info      |      3        | An informational message, often indicates the current program state.                    |
+-----------+---------------+-----------------------------------------------------------------------------------------+
| Debug     |      4        | A debug message, useful for debugging but too verbose to be turned on normally.         |
+-----------+---------------+-----------------------------------------------------------------------------------------+
| Verbose   |      5        | A message that will be printed a lot, useful for debugging program flow and will        |
|           |               | probably impact performance.                                                            |
+-----------+---------------+-----------------------------------------------------------------------------------------+

Example Usage
-------------

.. code-block:: c++

  #include "mozilla/Logging.h"

  using mozilla::LogLevel;

  static mozilla::LazyLogModule sLogger("example_logger");

  static void DoStuff()
  {
    MOZ_LOG(sLogger, LogLevel::Info, ("Doing stuff."));

    int i = 0;
    int start = Time::NowMS();
    MOZ_LOG(sLogger, LogLevel::Debug, ("Starting loop."));
    while (i++ &lt; 10) {
      MOZ_LOG(sLogger, LogLevel::Verbose, ("i = %d", i));
    }

    // Only calculate the elapsed time if the Warning level is enabled.
    if (MOZ_LOG_TEST(sLogger, LogLevel::Warning)) {
      int elapsed = Time::NowMS() - start;
      if (elapsed &gt; 1000) {
        MOZ_LOG(sLogger, LogLevel::Warning, ("Loop took %dms!", elapsed));
      }
    }

    if (i != 10) {
      MOZ_LOG(sLogger, LogLevel::Error, ("i should be 10!"));
    }
  }

Enabling Logging
----------------

The log level for a module is controlled by setting an environment variable before launching the application. It can also be adjusted by setting prefs.  By default all logging output is disabled.

::

  set MOZ_LOG="example_logger:3"

In the Windows Command Prompt (`cmd.exe`), don't use quotes:

::

  set MOZ_LOG=example_logger:3

If you want this on GeckoView example, use the following adb command to launch process:

::

  adb shell am start -n org.mozilla.geckoview_example/.GeckoViewActivity --es env0 "MOZ_LOG=example_logger:3"

There are special module names to change logging behavior. You can specify one or more special module names without logging level.

+-------------------------+-------------------------------------------------------------------------------------------+
| Special module name     | Action                                                                                    |
+=========================+===========================================================================================+
| append                  | Append new logs to existing log file.                                                     |
+-------------------------+-------------------------------------------------------------------------------------------+
| sync                    | Print each log synchronously, this is useful to check behavior in real time or get logs   |
|                         | immediately before crash.                                                                 |
+-------------------------+-------------------------------------------------------------------------------------------+
| raw                     | Print exactly what has been specified in the format string, without the                   |
|                         | process/thread/timestamp, etc. prefix.                                                    |
+-------------------------+-------------------------------------------------------------------------------------------+
| timestamp               | Insert timestamp at start of each log line.                                               |
+-------------------------+-------------------------------------------------------------------------------------------+
| rotate: **N**           | | This limits the produced log files' size.  Only most recent **N megabytes** of log data |
|                         | | is saved.  We rotate four log files with .0, .1, .2, .3 extensions.  Note: this option  |
|                         | | disables 'append' and forces 'timestamp'.                                               |
+-------------------------+-------------------------------------------------------------------------------------------+

For example, if you want to specify `sync`, `timestamp` and `rotate`:

::

  set MOZ_LOG="example_logger:3,timestamp,sync,rotate:10"

To adjust the logging after Firefox has started, you can set prefs under the `logging.` prefix. For example, setting `logging.foo` to `3` will set the log module `foo` to start logging at level 3. The special boolean prefs `logging.config.sync` and `logging.config.add_timestamp` can be used to control the `sync` and `timestamp` properties described above.

.. warning::
    A sandboxed content process cannot write to stderr or any file.  The easiest way to log these processes is to disable the content sandbox by setting the preference `security.sandbox.content.level` to `0`.  On Windows, you can still see child process messages by using DOS (not the `MOZ_LOG_FILE` variable defined below) to redirect output to a file.  For example: `MOZ_LOG="CameraChild:5" mach run >& my_log_file.txt` will include debug messages from the camera's child actor that lives in a (sandboxed) content process.

Redirecting logging output to a file
------------------------------------

Logging output  can be redirected to a file by passing its path via an environment variable.

.. note::
  By default logging output goes to `stderr`.

::

  set MOZ_LOG_FILE="log.txt"

The `rotate` and `append` options described above only apply when logging to a file.

The special pref `logging.config.LOG_FILE` can be set at runtime to change the log file being output to.

Logging Rust
------------

We're gradually adding more Rust code to Gecko, and Rust crates typically use a different approach to logging. Many Rust libraries use the `log <https://docs.rs/log>`_ crate to log messages, which works together with `env_logger <https://docs.rs/env_logger>`_ at the application level to control what's actually printed via `RUST_LOG`.

You can set an overall logging level, though it could be quite verbose:

::

  set RUST_LOG="debug"

You can also target individual modules by path:

::

  set RUST_LOG="style::style_resolver=debug"

.. note::
  For Linux/MacOS users, you need to use `export` rather than `set`.

.. note::
  Sometimes it can be useful to only log child processes and ignore the parent process. In Firefox 57 and later, you can use `RUST_LOG_CHILD` instead of `RUST_LOG` to specify log settings that will only apply to child processes.

The `log` crate lists the available `log levels <https://docs.rs/log/0.3.8/log/enum.LogLevel.html>`_:

+-----------+---------------------------------------------------------------------------------------------------------+
| Log Level | Purpose                                                                                                 |
+===========+=========================================================================================================+
| error     | Designates very serious errors.                                                                         |
+-----------+---------------------------------------------------------------------------------------------------------+
| warn      | Designates hazardous situations.                                                                        |
+-----------+---------------------------------------------------------------------------------------------------------+
| info      | Designates useful information.                                                                          |
+-----------+---------------------------------------------------------------------------------------------------------+
| debug     | Designates lower priority information.                                                                  |
+-----------+---------------------------------------------------------------------------------------------------------+
| trace     | Designates very low priority, often extremely verbose, information.                                     |
+-----------+---------------------------------------------------------------------------------------------------------+

It is common for debug and trace to be disabled at compile time in release builds, so you may need a debug build if you want logs from those levels.

Check the `env_logger <https://docs.rs/env_logger>`_ docs for more details on logging options.

