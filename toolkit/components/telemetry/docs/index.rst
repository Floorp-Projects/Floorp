=========
Telemetry
=========

Telemetry is a feature that allows data collection.
This is being used to collect performance metrics and other information about how Firefox performs in the wild, e.g. update events or session lengths.

There are two main ways of gathering data, Desktop Telemetry - documented here - which is used in Firefox Desktop
and `Glean <https://docs.telemetry.mozilla.org/concepts/glean/glean.html>`__ which is
Mozilla’s newer telemetry framework and used for example in Firefox on Android.
Information which is gathered is called a probe in Desktop Telemetry or a metric in Glean.
The data is being sent in so-called pings. When pings cannot be sent immediately, caching is implemented as well.

This means, client-side the main building blocks are:

* :doc:`data collection <collection/index>`, e.g. in histograms and scalars
* assembling :doc:`concepts/pings` with the general information and the data payload
* sending them to the server and local ping retention

There are also some notable special cases:

1. `Firefox on Glean (FOG) <../glean/index.html>`__ a wrapper around Glean for Firefox Desktop.
2. `GeckoView Streaming Telemetry <../internals/geckoview-streaming.html>`__ - gecko uses Desktop Telemetry for recording data, but on Android (Fenix) Glean is used for sending the data. GeckoView provides the necessary adapter to get the data from Desktop Telemetry to Glean.

*Note:* Mozilla's `data collection policy <https://wiki.mozilla.org/Firefox/Data_Collection>`_ documents the process and requirements that are applied here.

.. toctree::
   :maxdepth: 5
   :titlesonly:

   start/index
   concepts/index
   collection/index
   data/index
   internals/index
   obsolete/index
