=========
Telemetry
=========

Telemetry is a feature that allows data collection. This is being used to collect performance metrics and other information about how Firefox performs in the wild.

Client-side, this consists of:

* data collection in `Histograms <https://developer.mozilla.org/en-US/docs/Mozilla/Performance/Adding_a_new_Telemetry_probe>`_ and other data structures
* assembling :doc:`pings` with the general information and the data payload
* sending them to the server and local ping retention

*Note:* the `data collection policy <https://wiki.mozilla.org/Firefox/Data_Collection>`_ documents the process and requirements that are applied here.

.. toctree::
   :maxdepth: 2

   pings
   common-ping
   environment
   main-ping
   preferences
