GeckoView Streaming API
=======================

As an alternative to the normal mode where Firefox Desktop records and sends data,
Telemetry can instead route Histogram samples and Scalar values out of Gecko to a Telemetry Delegate.

To do this, ``toolkit.telemetry.geckoview.streaming`` must be set to true,
and Gecko must have been built with ``MOZ_WIDGET_ANDROID`` defined.

See :doc:`this guide <../start/report-gecko-telemetry-in-glean>`
for how to collect data in this mode.

Details
=======

Samples accumulated on Histograms and values set
(``ScalarAdd`` and ``ScalarSetMaximum`` operations are not supported)
on Scalars that have ``products`` lists that include ``geckoview_streaming``
will be redirected to a small batching service in
``toolkit/components/telemetry/geckoview/streaming``.
The batching service
(essentially just tables of histogram/scalar names to lists of samples/values)
will hold on to these lists of samples/values paired to the histogram/scalar names for a length of time
(``toolkit.telemetry.geckoview.batchDurationMS`` (default 5000))
after which the next accumulation or ``ScalarSet`` will trigger the whole batch
(all lists) to be passed over to the ``StreamingTelemetryDelegate``.
