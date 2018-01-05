.. _telemetry:

=========
Telemetry
=========

Telemetry is a feature that allows data collection. This is being used to collect performance metrics and other information about how Firefox performs in the wild.

Client-side, this consists of:

* :doc:`data collection <collection/index>`, e.g. in histograms and scalars
* assembling :doc:`concepts/pings` with the general information and the data payload
* sending them to the server and local ping retention

*Note:* the `data collection policy <https://wiki.mozilla.org/Firefox/Data_Collection>`_ documents the process and requirements that are applied here.

.. toctree::
   :maxdepth: 5
   :titlesonly:

   start/index
   concepts/index
   collection/index
   data/index
   internals/index
   fhr/index
