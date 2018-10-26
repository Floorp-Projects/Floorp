======================
Measuring elapsed time
======================

To make it easier to measure how long operations take, we have helpers for both JavaScript and C++.
These helpers record the elapsed time into histograms, so you have to create suitable :doc:`histograms` for them first.

From JavaScript
===============
JavaScript can measure elapsed time using TelemetryStopwatch.

``TelemetryStopwatch`` is a helper that simplifies recording elapsed time (in milliseconds) into histograms (plain or keyed).

API:

.. code-block:: js

    TelemetryStopwatch = {
      // Start, check if running, cancel & finish recording elapsed time into a
      // histogram.
      // |aObject| is optional. If specified, the timer is associated with this
      // object, so multiple time measurements can be done concurrently.
      start(histogramId, aObject);
      running(histogramId, aObject);
      cancel(histogramId, aObject);
      finish(histogramId, aObject);
      // Start, check if running, cancel & finish recording elapsed time into a
      // keyed histogram.
      // |key| specifies the key to record into.
      // |aObject| is optional and used as above.
      startKeyed(histogramId, key, aObject);
      runningKeyed(histogramId, key, aObject);
      cancelKeyed(histogramId, key, aObject);
      finishKeyed(histogramId, key, aObject);
    };

Example:

.. code-block:: js

    TelemetryStopwatch.start("SAMPLE_FILE_LOAD_TIME_MS");
    // ... start loading file.
    if (failedToOpenFile) {
      // Cancel this if the operation failed early etc.
      TelemetryStopwatch.cancel("SAMPLE_FILE_LOAD_TIME_MS");
      return;
    }
    // ... do more work.
    TelemetryStopwatch.finish("SAMPLE_FILE_LOAD_TIME_MS");

    // Another loading attempt? Start stopwatch again if
    // not already running.
    if (!TelemetryStopwatch.running("SAMPLE_FILE_LOAD_TIME_MS")) {
      TelemetryStopwatch.start("SAMPLE_FILE_LOAD_TIME_MS");
    }

    // Periodically, it's necessary to attempt to finish a
    // TelemetryStopwatch that's already been canceled or
    // finished. Normally, that throws a warning to the
    // console. If the TelemetryStopwatch being possibly
    // canceled or finished is expected behaviour, the
    // warning can be suppressed by passing the optional
    // aCanceledOkay argument.

    // ... suppress warning on a previously finished
    // TelemetryStopwatch
    TelemetryStopwatch.finish("SAMPLE_FILE_LOAD_TIME_MS", null,
                              true /* aCanceledOkay */);

From C++
========

API:

.. code-block:: cpp

    // This helper class is the preferred way to record elapsed time.
    template<HistogramID id>
    class AutoTimer {
      // Record into a plain histogram.
      explicit AutoTimer(TimeStamp aStart = TimeStamp::Now());
      // Record into a keyed histogram, with key |aKey|.
      explicit AutoTimer(const nsCString& aKey,
                         TimeStamp aStart = TimeStamp::Now());
    };

    // If the Histogram id is not known at compile time:
    class RuntimeAutoTimer {
      // Record into a plain histogram.
      explicit RuntimeAutoTimer(Telemetry::HistogramID aId,
                            TimeStamp aStart = TimeStamp::Now());
      // Record into a keyed histogram, with key |aKey|.
      explicit RuntimeAutoTimer(Telemetry::HistogramID aId,
                            const nsCString& aKey,
                            TimeStamp aStart = TimeStamp::Now());
    };

    void AccumulateTimeDelta(HistogramID id, TimeStamp start, TimeStamp end = TimeStamp::Now());
    void AccumulateTimeDelta(HistogramID id, const nsCString& key, TimeStamp start, TimeStamp end = TimeStamp::Now());

Example:

.. code-block:: cpp

    {
      Telemetry::AutoTimer<Telemetry::FIND_PLUGINS> telemetry;
      // ... scan disk for plugins.
    }
    // When leaving the scope, AutoTimers destructor will record the time that passed.

    // If the histogram id is not known at compile time.
    {
      Telemetry::RuntimeAutoTimer telemetry(Telemetry::FIND_PLUGINS);
      // ... scan disk for plugins.
    }
    // When leaving the scope, AutoTimers destructor will record the time that passed.
