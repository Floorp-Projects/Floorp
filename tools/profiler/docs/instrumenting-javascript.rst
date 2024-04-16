Instrumenting JavaScript
========================

There are multiple ways to use the profiler with JavaScript. There is the "JavaScript"
profiler feature (via about:profiling), which enables stack walking for JavaScript code.
This is most likely turned on already for every profiler preset.

In addition, markers can be created to specifically marker an instant in time, or a
duration. This can be helpful to make sense of a particular piece of the front-end,
or record events that normally wouldn't show up in samples.

.. note::
    This guide explains JavaScript markers in depth. To learn more about how to add a
    marker in C++, Rust or JVM please take a look at their documentation
    in :doc:`markers-guide`, :doc:`instrumenting-rust` or :doc:`instrumenting-android`
    respectively.

Markers in Browser Chrome
*************************

If you have access to ChromeUtils, then adding a marker is relatively easily.

.. code-block:: javascript

  // Add an instant marker, representing a single point in time
  ChromeUtils.addProfilerMarker("MarkerName");

  // Add a duration marker, representing a span of time.
  const startTime = Cu.now();
  doWork();
  ChromeUtils.addProfilerMarker("MarkerName", startTime);

  // Add a duration marker, representing a span of time, with some additional tex
  const startTime = Cu.now();
  doWork();
  ChromeUtils.addProfilerMarker("MarkerName", startTime, "Details about this event");

  // Add an instant marker, with some additional tex
  const startTime = Cu.now();
  doWork();
  ChromeUtils.addProfilerMarker("MarkerName", undefined, "Details about this event");

Markers in Content Code
***********************

If instrumenting content code, then the `UserTiming`_ API is the best bet.
:code:`performance.mark` will create an instant marker, and a :code:`performance.measure`
will create a duration marker. These markers will show up under UserTiming in
the profiler UI.

.. code-block:: javascript

  // Create an instant marker.
  performance.mark("InstantMarkerName");

  doWork();

  // Measuring with the performance API will also create duration markers.
  performance.measure("DurationMarkerName", "InstantMarkerName");

.. _UserTiming: https://developer.mozilla.org/en-US/docs/Web/API/User_Timing_API
