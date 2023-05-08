=======================================================
How to report Gecko Telemetry in engine-gecko via Glean
=======================================================

In Gecko, the `Telemetry <../index.html>`__ system collects various measures of Gecko performance, hardware, usage and customizations.
When the Gecko engine is embedded in Android products that use
`the Glean SDK <https://docs.telemetry.mozilla.org/concepts/glean/glean.html>`__ for data collection, then Gecko metrics can be reported in `Glean pings <https://mozilla.github.io/glean/book/user/pings/index.html>`__.
This article provides an overview of what is needed to report any existing or new Telemetry data collection in Glean.

.. important::

    Every new or changed data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Data_Collection>`__ from a Data Steward.

Using Glean
===========
In short, the way to collect data in all Mozilla products using Gecko is to use Glean.
The Glean SDK is present in Gecko and all of Firefox Desktop via
`Firefox on Glean (FOG) <../../glean/index.html>`__.
If you instrument your Gecko data collection using Glean metrics,
they will be reported by all products that use both Gecko and Glean so long as it is defined in a
``metrics.yaml`` file in the ``gecko_metrics`` list of ``toolkit/components/glean/metrics_index.py``.

See `these docs <../../glean/user/new_definitions_file.html>`__ for details.
Additional relevant concepts are covered in
`this guide for migrating Firefox Telemetry collections to Glean <../../glean/user/migration.html>`__.

To make sure your data is also reported in Telemetry when your code is executed in Firefox Desktop,
you must be sure to mirror the collection in Telemetry.
If possible, you should use the
`Glean Interface For Firefox Telemetry (GIFFT) <../../glean/user/gifft.html>`__
and let FOG mirror the call for you.
If that doesn't look like it'll work out for you,
you can instead add your Glean API calls just next to your Telemetry API calls.

As always, if you have any questions, you can find helpful people in
`the #telemetry channel on Mozilla's Matrix <https://chat.mozilla.org/#/room/#telemetry:mozilla.org>`__.
