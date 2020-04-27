=======================================================
How to report Gecko Telemetry in engine-gecko via Glean
=======================================================

In `Gecko <https://developer.mozilla.org/en-US/docs/Mozilla/Gecko>`__, the `Telemetry <../index.html>`__ system collects various measures of Gecko performance, hardware, usage and customizations.
When the Gecko engine is embedded in Android products through any of the `engine-gecko-* <https://github.com/mozilla-mobile/android-components/tree/master/components/browser>`__ components of `Android Components <https://mozac.org/>`__ (there is one component for each Gecko channel),
and the product is also using the `Glean SDK <https://docs.telemetry.mozilla.org/concepts/glean/glean.html>`__ for data collection, then Gecko metrics can be reported in `Glean pings <https://mozilla.github.io/glean/book/user/pings/index.html>`__.
This article provides an overview of what is needed to report any existing or new Telemetry data collection in Gecko to Glean.

.. important::

    Every new or changed data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection>`__ from a Data Steward.

Overview
========
Histograms are reported out of Gecko with a mechanism called `streaming Telemetry <../internals/geckoview-streaming.html>`__.
This mechanism intercepts Gecko calls to tagged histograms and batches and bubbles them up through the `the GeckoView RuntimeTelemetry delegate <https://mozilla.github.io/geckoview/javadoc/mozilla-central/index.html>`__.
The ``engine-gecko-*`` components provide implementations of the delegate which dispatches Gecko metrics to the Glean SDK.

Reporting an existing histogram
===============================
Exfiltrating existing histograms is a relatively straightforward process made up of a few small steps.

Tag histograms in ``Histograms.json``
-------------------------------------
Accumulations to non-tagged histograms are ignored if streaming Telemetry is enabled.
To tag a histogram you must add the `geckoview_streaming` product to the `products list <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/collection/histograms.html#products>`__  in the `Histograms.json file <https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/telemetry/Histograms.json>`__ .

Add Glean metrics to ``metrics.yaml``
-------------------------------------
The Glean SDK provides a number of `higher level metric types <https://mozilla.github.io/glean/book/user/metrics/index.html>`__ to map Gecko histogram metrics to.
However, Gecko histograms lack the metadata to infer the Glean SDK destination type manually.
For this reason, engineers must pick the most appropriate Gecko SDK type themselves.

Read more about how to add Glean SDK metrics to the `metrics.yaml file <https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/telemetry/geckoview/streaming/metrics.yaml>`__ in the `Glean SDK documentation <https://mozilla.github.io/glean/book/user/adding-new-metrics.html>`__.

.. important::

    Every new or changed data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection>`__ from a Data Steward.

Example: reporting ``CHECKERBOARD_DURATION``
--------------------------------------------
The first step is to add the relevant tag (i.e. ``geckoview_streaming``) to the histogram's ``products`` key in the ``Histograms.json`` file.

.. code-block:: json

  {
    "CHECKERBOARD_DURATION": {
      "record_in_processes": ["main", "content", "gpu"],
      "products": ["firefox", "fennec", "geckoview", "geckoview_streaming"],
      "alert_emails": ["gfx-telemetry-alerts@mozilla.com", "kgupta@mozilla.com"],
      "bug_numbers": [1238040, 1539309],
      "releaseChannelCollection": "opt-out",
      "expires_in_version": "73",
      "kind": "exponential",
      "high": 100000,
      "n_buckets": 50,
      "description": "Duration of a checkerboard event in milliseconds"
    },
  }

.. note::

    Histograms with ``"releaseChannelCollection": "opt-in"``, or without a ``releaseChannelCollection`` specified in its definition are only collected on Gecko built for ``"nightly"`` and ``"beta"`` channels.

Since this is a timing distribution, with a milliseconds time unit, it can be added as follows to the ``metrics.yaml`` file:

.. code-block:: yaml

  gfx.content.checkerboard:
    duration:
      type: timing_distribution
      time_unit: millisecond
      gecko_datapoint: CHECKERBOARD_DURATION
      description: |
        Duration of a checkerboard event.
      bugs:
        - 1238040
        - 1539309
      data_reviews:
        - https://example.com/data-review-url-example
      notification_emails:
        - gfx-telemetry-alerts@mozilla.com
        - kgupta@mozilla.com
      expires: 2019-12-09 # Gecko 73

Please note that the ``gecko_datapoint`` property will need to point to the name of the histogram exactly as written in the ``Histograms.json`` file. It is also important to note that ``time_unit`` needs to match the unit of the values that are recorded.

Example: recording without losing process information
-----------------------------------------------------
If a histogram is being recorded in multiple processes, care must be taken to guarantee that data always comes from the same process throughout the lifetime of a Gecko instance,
otherwise all the data will be added to the same Glean SDK metric.
If process exclusivity cannot be guaranteed, then a histogram (and the respective Glean SDK metric) must be created for each relevant process.
Consider the ``IPC_MESSAGE_SIZE2`` histogram:

.. code-block:: json

  {
    "IPC_MESSAGE_SIZE2": {
      "record_in_processes": ["main", "content", "gpu"],
      "products": ["firefox", "fennec", "geckoview"],
      "alert_emails": ["hchang@mozilla.com"],
      "bug_numbers": [1353159],
      "expires_in_version": "60",
      "kind": "exponential",
      "high": 8000000,
      "n_buckets": 50,
      "keyed": false,
      "description": "Measures the size of all IPC messages sent that are >= 4096 bytes."
    },
  }

Data for this histogram could come, at the same time, from the ``"main"``, ``"content"`` and ``"gpu"`` processes, since it is measuring IPC itself.
By adding the ``geckoview_streaming`` product, data coming from all the processes would flow in the same Glean SDK metric and would loose the information about the process it came from.
This problem can be solved by creating three histograms, one for each originating process.
Here is, for example, the histogram for the GPU process:

.. code-block:: json

  {
    "IPC_MESSAGE_SIZE2_GPU": {
      "record_in_processes": ["gpu"],
      "products": ["geckoview_streaming"],
      "alert_emails": ["hchang@mozilla.com"],
      "bug_numbers": [1353159],
      "expires_in_version": "60",
      "kind": "exponential",
      "high": 8000000,
      "n_buckets": 50,
      "description": "Measures the size of all IPC messages sent that are >= 4096 bytes."
    },
  }

And the related Glean SDK metric


.. code-block:: yaml

  ipc.message:
    gpu_size:
      type: memory_distribution
      memory_unit: byte
      gecko_datapoint: IPC_MESSAGE_SIZE2_GPU
      description: |
        Measures the size of the IPC messages from/to the GPU process that are >= 4096 bytes.
      bugs:
        - 1353159
      data_reviews:
        - https://example.com/data-review-url-example
      notification_emails:
        - hchang@mozilla.com
      expires: 2019-12-09 # Gecko 73

The ``ipc.message.gpu_size`` metric in the Glean SDK will now contain all the data coming exclusively from the GPU process.
Similar definitions can be used for the other processes.

Reporting a scalar
==================
Exfiltrating existing boolean, string or uint scalars, or adding new ones, is a relatively straightforward process made up of a few small steps.

Tag scalars in ``Scalars.yaml``
----------------------------------
Accumulations to non-tagged scalars are ignored if streaming Telemetry is enabled.
To tag a scalar you must add the `geckoview_streaming` product to the `products list <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/collection/scalars.html#required-fields>`__  in the `Scalars.yaml file <https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/telemetry/Scalars.yaml>`__ .

Add Glean metrics to ``metrics.yaml``
-------------------------------------
The Glean SDK provides the `Quantity <https://mozilla.github.io/glean/book/user/metrics/quantity.html>`__, `Boolean <https://mozilla.github.io/glean/book/user/metrics/boolean.html>`__ and `String <https://mozilla.github.io/glean/book/user/metrics/string.html>`__ metric types to map Gecko scalars to.
However, Gecko scalars lack the metadata to infer the Glean SDK destination type manually.
For this reason, engineers must pick the most appropriate Gecko SDK type themselves.

Read more about how to add Glean SDK metrics to the `metrics.yaml file <https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/telemetry/geckoview/streaming/metrics.yaml>`__ in the `Glean SDK documentation <https://mozilla.github.io/glean/book/user/adding-new-metrics.html>`__.

.. important::

    Every new or changed data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection>`__ from a Data Steward.

Example: reporting the display width from Gecko
-----------------------------------------------
The first step is to add the relevant Gecko scalar with its streaming telemetry tag (i.e. ``geckoview_streaming``) in the ``Scalars.yaml`` file.

.. code-block:: yaml

  gfx.info:
    display_width:
      bug_numbers:
        - 1514840
      description: >
        The width of the main display as detected by Gecko.
      kind: uint
      expires: never
      notification_emails:
        - gfx-telemetry-alerts@mozilla.com
        - rhunt@mozilla.com
      products:
        - 'firefox'
        - 'fennec'
        - 'geckoview'
        - 'geckoview_streaming'
      record_in_processes:
        - 'main'

.. note::

    Scalars with ``"release_channel_collection": "opt-in"``, or without a ``release_channel_collection`` specified in its definition are only collected on Gecko built for ``"nightly"`` and ``"beta"`` channels.

Since this is a uint scalar, it can be added as follows to the ``metrics.yaml`` file:

.. code-block:: yaml

  gfx.display:
    width:
      type: quantity
      description: The width of the display, in pixels.
      unit: pixels
      gecko_datapoint: gfx.info.display_width
      description: |
        Duration of a checkerboard event.
      bugs:
        - 1514840
      data_reviews:
        - https://example.com/data-review-url-example
      notification_emails:
        - gfx-telemetry-alerts@mozilla.com
        - rhunt@mozilla.com
      expires: never

Please note that the ``gecko_datapoint`` property will need to point to the name of the scalar exactly as written in the ``Scalars.yaml`` file.

How to access the data?
=======================
Once a new build of Gecko will be provided through `Maven <https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview>`__, the Android Components team will automatically pick it up.
Because the Gecko train model has three channels, there are three ``engine-gecko-*`` components, one per Gecko channel: `"engine-gecko-nigthly" <https://github.com/mozilla-mobile/android-components/tree/master/components/browser/engine-gecko-nightly>`__, `"engine-gecko-beta" <https://github.com/mozilla-mobile/android-components/tree/master/components/browser/engine-gecko-beta>`__ and `engine-gecko <https://github.com/mozilla-mobile/android-components/tree/master/components/browser/engine-gecko>`__.

The availability of the metric in the specific product's dataset depends on which channel the application is using.
For example, if Fenix Release depends on the ``engine-gecko (release)`` channel, then the registry file additions need to be available on the Release channel for Gecko in order for them to be exposed in Fenix.

Unless `Glean custom pings <https://mozilla.github.io/glean/book/user/pings/custom.html>`__ are used, all the metrics are reported through the `Glean metrics ping <https://mozilla.github.io/glean/book/user/pings/metrics.html>`__.

Testing your metrics
====================
At this time, the procedure for testing that metrics are correctly exfiltrated from GeckoView to Glean SDK-enabled products is a bit involved.

1. After adding your metric as described in the previous section, substitute the locally built GeckoView in your local copy of `Android Components <https://github.com/mozilla-mobile/android-components/>`__ as described in the `GeckoView docs <https://mozilla.github.io/geckoview/contributor/geckoview-quick-start#dependency-substiting-your-local-geckoview-into-a-mozilla-project>`__.
2. In Android Components, follow the `instructions to enable upload <https://github.com/mozilla-mobile/android-components/tree/master/samples/browser#glean-sdk-support>`__ in the `samples-browser` application.
3. Build Android Components and the `samples-browser` application.
4. Use the Glean SDK `debugging features <https://mozilla.github.io/glean/book/user/debugging/index.html>`__ to either dump the `metrics` ping or send it to the `Glean Debug View <https://docs.telemetry.mozilla.org/concepts/glean/debug_ping_view.html>`__.

.. note::

    It is important to substitute GeckoView in Android Components, even if it's possible to substitute it directly in the final product. This is because the bulk of the processing happens in Android Components, in the `engine-gecko-*` components wrapping GeckoView.

Unsupported features
====================
This is the list of the currently unsupported features:

* `keyed scalars <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/collection/scalars.html#keyed-scalars>`__ are not supported and there are no future plans for supporting them;
* uint scalar operations other than `set <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/collection/scalars.html#id2>`__ are not supported and there are no future plans for supporting them.
* `events <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/collection/events.html>`__ are not supported and there are no future plans for supporting them.
