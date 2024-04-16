Instrumenting Android
========================

There are multiple ways to use the profiler with Android. There is the "Java"
profiler feature (via about:profiling), which enables profiling for JVM code.
This is most likely turned on already for the preset if you are profiling an
Android device.

In addition to sampling, markers can be created to specifically mark an instant
in time, or a duration. This can be helpful to make sense of a particular piece
of the front-end, or record events that normally wouldn't show up in samples.

.. note::
    This guide explains Android markers in depth. To learn more about how to add a
    marker in C++, JavaScript or Rust, please take a look at their documentation
    in :doc:`markers-guide`, :doc:`instrumenting-javascript` or
    :doc:`instrumenting-rust` respectively.

Markers in the GeckoView Java codebase
**************************************

If you are in the GeckoView codebase, then you should have access to ``GeckoRuntime``.
``GeckoRuntime`` has a ``getProfilerController`` method to get the ``ProfilerController``.
See the `ProfilerController Java file`_ (`javadoc`_) to find which methods you can use to
instrument your source code.

Here's an example:

.. code-block:: java

    // Simple marker
    sGeckoRuntime.getProfilerController().addMarker("Marker Name");

    // Simple marker with additional information
    sGeckoRuntime.getProfilerController().addMarker("Marker Name", "info");

    // Duration marker
    Double startTime = sGeckoRuntime.getProfilerController().getProfilerTime();
    // ...some code you want to measure...
    sGeckoRuntime.getProfilerController().addMarker("Marker Name", startTime);

    // Duration marker with additional information
    sGeckoRuntime.getProfilerController().addMarker("Marker Name", startTime, "info");

There are various overloads of ``addMarker`` you can choose depending on your need.

If you need to compute some information before adding it to a marker, it's
recommended to wrap that code with a `isProfilerActive` if check to make sure
that it's only executed while the profiler is active. Here's an example:

.. code-block:: java

  ProfilerController profilerController = runtime.getProfilerController();
  if (profilerController.isProfilerActive()) {
    // Compute the information you want to include.
    String info = ...
    sGeckoRuntime.getProfilerController().addMarker("Marker Name", info);
  }

Markers in the Fenix codebase
*****************************

If you are in the Fenix codebase, then you should have access to the Android
components. The Android components includes a `Profiler interface here`_, with
its corresponding `implementation here`_. You should be able to do everything
you can do with the ``ProfilerController``. Here's an example on how to call them:

.. code-block:: kotlin

    // Simple marker
    components.core.engine.profiler?.addMarker("Marker Name");

    // Simple marker with additional information
    components.core.engine.profiler?.addMarker("Marker Name", "info");

    // Duration marker
    val startTime = components.core.engine.profiler?.getProfilerTime()
    // ...some code you want to measure...
    components.core.engine.profiler?.addMarker("Marker Name", startTime, "additional info")

    // Duration marker with additional information
    components.core.engine.profiler?.addMarker("Marker Name", startTime, "info");

Similarly, there are various overloads of ``addMarker`` you can choose depending on your needs.

Like for the GeckoView example above, if you need to compute some information
before adding it to a marker, it's recommended to wrap that code with a
`isProfilerActive` if check to make sure that it's only executed while the
profiler is active. Here's an example:

.. code-block:: kotlin

  if (components.core.engine.profiler?.isProfilerActive() == true) {
    // Compute the information you want to include.
    var info = ...
    components.core.engine.profiler?.addMarker("Marker Name", info)
  }

.. _ProfilerController Java file: https://searchfox.org/mozilla-central/source/mobile/android/geckoview/src/main/java/org/mozilla/geckoview/ProfilerController.java
.. _javadoc: https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/ProfilerController.html
.. _Profiler interface here: https://searchfox.org/mozilla-central/source/mobile/android/android-components/components/concept/base/src/main/java/mozilla/components/concept/base/profiler/Profiler.kt
.. _implementation here: https://searchfox.org/mozilla-central/source/mobile/android/android-components/components/browser/engine-gecko/src/main/java/mozilla/components/browser/engine/gecko/profiler/Profiler.kt
