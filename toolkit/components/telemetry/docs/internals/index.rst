=========
Internals
=========

Here is a quick overview of the most important code parts. They can be found in the telemetry folder.

* TelemetryController: Main telemetry logic, e.g. assembling pings, local storage (when archiving is enabled), preference changes, testing
* Telemetry.cpp contains most of the public interface, implements the IDL
* The different data types for telemetry are handled in TelemetryHistogram, TelemetryScalar, TelemetryEvent.
* TelemetryEnvironment: A helper for gathering environment data, like build version or graphics data
* TelemetryScheduler: Starts regular jobs for collecting and sending data
* TelemetrySend: Sending and caching of pings
* TelemetryStorage: Handles writing pings to disk for TelemetrySend
* TelemetrySession: Collects data for a browsing session, includes many of the most important probes (aka metrics)
* Policy: A layer of indirection added to provide testability. A common pattern in many files
* pings/: Contains definitions and handling for most ping types, like EventPing

More details on different topics can be found in these chapters:

.. toctree::
   :maxdepth: 2
   :titlesonly:
   :glob:

   **
