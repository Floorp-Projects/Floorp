GeckoView Streaming API
=======================

As an alternative to `GeckoView <../internals/geckoview>` mode,
Telemetry can instead route Histogram accumulations out of Gecko to a Telemetry Delegate.

To do this, ``toolkit.telemetry.geckoview.streaming`` must be set to true,
and Gecko must have been built with ``MOZ_WIDGET_ANDROID`` defined.

Details
=======

Accumulations from Histograms that have a ``products`` list that includes ``geckoview_streaming`` will be redirected to a small batching service in ``toolkit/components/telemetry/geckoview/streaming``.
The batching service (essentially just a table of histogram names to lists of samples) will hold on to these lists of accumulations paired to the histogram names for a length of time (``toolkit.telemetry.geckoview.batchDurationMS`` (default 5000)) after which the next accumulation will trigger the whole batch (all lists) to be passed over to the ``StreamingTelemetryDelegate``.
