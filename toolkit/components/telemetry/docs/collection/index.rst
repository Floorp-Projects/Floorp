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
* :doc:`events` can record richer data on individual occurrences of specific actions
* :doc:`Measuring elapsed time <measuring-time>`
* :doc:`Custom pings <custom-pings>`
* :doc:`Stack capture <stack-capture>` allow recording application call stacks
* :doc:`Use counters <use-counters>` measure the usage of web platform features
* :doc:`Experiment annotations <experiments>`
* :doc:`Remote content uptake <uptake>`
* :doc:`Hybrid Content Telemetry <hybrid-content>` allows recording telemetry from semi-privileged hosted content
* :doc:`WebExtension API <webextension-api>` can be used in privileged webextensions
* :doc:`Origin Telemetry <origin>` Experimental prototype. For use by Content Blocking only for now.

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
   uptake
   hybrid-content
   *

Browser Usage Telemetry
~~~~~~~~~~~~~~~~~~~~~~~
For more information, see :ref:`browserusagetelemetry`.

Version History
~~~~~~~~~~~~~~~

- Firefox 61: Stopped reporting Telemetry Log items (`bug 1443614 <https://bugzilla.mozilla.org/show_bug.cgi?id=1443614>`_).
