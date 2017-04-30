===============
Data collection
===============

There are different APIs and formats to collect data in Firefox, all suiting different use cases.

In general, we aim to submit data in a common format where possible. This has several advantages; from common code and tooling to sharing analysis know-how.

In cases where this isn't possible and more flexibility is needed, we can submit custom pings or consider adding different data formats to existing pings.

*Note:* Every new data collection must go through a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection>`_.

The current data collection possibilities include:

* :doc:`scalars` allow recording of a single value (string, boolean, a number)
* :doc:`histograms` can efficiently record multiple data points
* ``environment`` data records information about the system and settings a session occurs in
* :doc:`events` can record richer data on individual occurences of specific actions
* ``TelemetryLog`` allows collecting ordered event entries up to a limit of 1000 entries (note: this does not have supporting analysis tools)
* :doc:`measuring elapsed time <measuring-time>`
* :doc:`custom pings <custom-pings>`
* :doc:`stack capture <stack-capture>` allow recording application call stacks
* :doc:`Use counters <use-counters>` measure the usage of web platform features
* :doc:`Experiment annotations <experiments>`

.. toctree::
   :maxdepth: 2
   :titlesonly:
   :hidden:
   :glob:

   scalars
   histograms
   events
   measuring-time
   custom-pings
   stack-capture
   experiments
   *

Browser Usage Telemetry
~~~~~~~~~~~~~~~~~~~~~~~
For more information, see :ref:`browserusagetelemetry`.
