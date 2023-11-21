============================
Adding a new Telemetry probe
============================

In Firefox, the Telemetry system collects various measures of Firefox performance, hardware, usage and customizations and submit it to Mozilla. This article provides an overview of what is needed to add any new Telemetry data collection.

.. important::

    Every new data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection#Requesting_Approval>`__ from a data collection peer. Just set the feedback? flag for one of the data peers. They try to reply within a business day.

What is your goal?
==================

We have various :doc:`data collection tools <../collection/index>` available, each serving different needs. Before diving right into technical details, it is best to take a step back and consider what you need to achieve.

Your goal could be to answer product questions like “how many people use feature X?” or “what is the error rate of service Y?”.
You could also be focused more on answering engineering questions, say “which web features are most used?” or “how is the performance of engine Z?”.

From there, questions you should ask are:

- What is the minimum data that can answer your questions?
- How many people do you need this data from?
- Is data from the pre-release channels sufficient?

This also informs the `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection>`__, which requires a plan for how to use the data. Data collection review is required for all new data collection.

Data collection levels
======================

Most of our data collection falls into one of two levels, *release* and *pre-release*.

**Release data** is recorded by default on all channels, users need to explicitly opt out to disable it. This has `stricter constraints <https://wiki.mozilla.org/Firefox/Data_Collection#Requirements>`_ for what data we can collect. "Most" users submit this data.

**Pre-release data** is not recorded on release, but is collected by default on our pre-release channels (Beta and Nightly), so it can be biased.

These levels cover what is described in the `Firefox privacy notice <https://www.mozilla.org/en-US/privacy/firefox/>`_. For other needs, there might be custom mechanisms that clearly require user opt-in and show what data is collected.

Rich data & aggregate data
==========================

For the recording and transmission of data, we have various data types available. We can divide these data types into two large groups.

**Aggregate data** is aggregated on the client-side and cheap to send, process and analyze. This could e.g. be a simple count of tab opens or a histogram showing how long it takes to switch between tabs. This should be your default choice and is well supported in our analysis tools.

**Rich data** is used when questions can not be answered from aggregate data. When we send more detailed data we can e.g. see when a specific UI interaction happened and in which context.

As a general rule, you can inform the choice of data types from your goals like this:

+------------------------+-----------------+-----------------------+
| Goals                  | Collection type | Implementation        |
+========================+=================+=======================+
| On-going monitoring    | Aggregate data  | Histograms            |
|                        |                 |                       |
| Health tracking        |                 | Scalars               |
|                        |                 |                       |
| KPI impact             |                 | Environment data      |
+------------------------+-----------------+-----------------------+
| Detailed user behavior | Rich data       | Event Telemetry       |
|                        |                 |                       |
| Funnel analysis        |                 | Detailed custom pings |
|                        |                 |                       |
| Diagnostics            |                 | Logs                  |
|                        |                 |                       |
|                        |                 | Crash data            |
+------------------------+-----------------+-----------------------+

Aggregate data
--------------

Most of our data collection happens through :doc:`scalars <../collection/scalars>` and :doc:`histograms <../collection/histograms>`:

- Scalars allow collection of simple values, like counts, booleans and strings.
- Histograms allow collection of multiple different values, but aggregate them into a number of buckets. Each bucket has a value range and a count of how many values we recorded.

Both scalars & histograms allow recording by keys. This allows for more flexible, two-level data collection.

We also collect :doc:`environment data <../data/environment>`. This consists of mostly scalar values that capture the “working environment” a Firefox session lives in, and includes e.g. data on hardware, OS, add-ons and some settings. Any data that is part of the "working environment", or needs to split :doc:`subsessions <../concepts/sessions>`, should go into it.

Rich data
---------

Aggregate data can tell you that something happened, but is usually lacking details about what exactly. When more details are needed, we can collect them using other tools that submit less efficient data. This usually means that we can't enable the data collection for all users, for cost and performance concerns.

There are multiple mechanisms to collect rich data:

**Stack collection** helps with e.g. diagnosing hangs. Stack data is recorded into chrome hangs and threadhang stats. To diagnose where rarely used code is called from, you can use stack capturing.

:doc:`Event Telemetry <../collection/events>` provides a way to record both when and what happened. This enables e.g. funnel analysis for usage.

:doc:`Custom pings <../collection/custom-pings>` are used when other existing data collection does not cover your need. Submitting a custom ping enables you to submit your own JSON package that will be delivered to the Telemetry servers. However, this loses you access to existing tooling and makes it harder to join your data with other sources.

Setup & building
================

Every build of Firefox has Telemetry enabled. Local developer builds with no custom build flags will record all Telemetry data, but not send it out.

When adding any new scalar, histogram or event Firefox needs to be built. Artifact builds are currently not supported, even if code changes are limited to JavaScript.

Usually you don't need to send out data to add new Telemetry. In the rare event you do, you need the following in your *.mozconfig*::

   MOZ_TELEMETRY_REPORTING=1
   MOZILLA_OFFICIAL=1

Testing
=======

Local confirmation
------------------

Your first step should always be to confirm your new data collection locally.

The *about:telemetry* page allows to view any data you submitted to Telemetry in the last 60 days, whether it is in existing pings or in new custom pings. You can choose which pings to display on the top-left.

If you need to confirm when - or if - pings are getting sent, you can run an instance of the `gzipServer <https://github.com/mozilla/gzipServer>`_ locally. It emulates roughly how the official Telemetry servers respond, and saves all received pings to disk for inspection.

Test coverage
-------------

Any data collection that you need to base decisions on needs to have test coverage. Using JS, you can access the recorded values for your data collection. You can use the following functions:

- for scalars, `getSnapshotForScalars() <https://searchfox.org/mozilla-central/rev/f997bd6bbfc4773e774fdb6cd010142370d186f9/toolkit/components/telemetry/core/nsITelemetry.idl#90-102>`_
  or `getSnapshotForKeyedScalars() <https://searchfox.org/mozilla-central/rev/f997bd6bbfc4773e774fdb6cd010142370d186f9/toolkit/components/telemetry/core/nsITelemetry.idl#104-116>`_
- for histograms, `getSnapshotForHistograms() <https://searchfox.org/mozilla-central/rev/f997bd6bbfc4773e774fdb6cd010142370d186f9/toolkit/components/telemetry/core/nsITelemetry.idl#54-74>`_
  or `getSnapshotForKeyedHistograms() <https://searchfox.org/mozilla-central/rev/f997bd6bbfc4773e774fdb6cd010142370d186f9/toolkit/components/telemetry/core/nsITelemetry.idl#76-88>`_

  * Optionally, histogram objects have a `snapshot() <https://searchfox.org/mozilla-central/rev/f997bd6bbfc4773e774fdb6cd010142370d186f9/toolkit/components/telemetry/core/nsITelemetry.idl#285-287,313-315>`_ method.

- for events, `snapshotEvents() <https://searchfox.org/mozilla-central/rev/f997bd6bbfc4773e774fdb6cd010142370d186f9/toolkit/components/telemetry/core/nsITelemetry.idl#542-558>`_

If you need to test that pings were correctly passed to Telemetry, you can use `TelemetryArchiveTesting <https://searchfox.org/mozilla-central/search?q=TelemetryArchiveTesting&redirect=false>`_.

Validation
----------

While it's important to confirm that the data collection works on your machine, the Firefox user population is very diverse. Before basing decisions on any new data, it should be validated. This could take various forms.

For *new data collection* using existing Telemetry data types, the transport mechanism is already tested. It is sufficient to validate the incoming values. This could happen through `Redash <https://docs.telemetry.mozilla.org/tools/stmo.html>`_ or through `custom analysis <https://docs.telemetry.mozilla.org/tools/spark.html>`_.

For *new custom pings*, you'll want to check schema validation results, as well as that the contents look valid.

Getting help
============

You can find all important Telemetry resources listed on `telemetry.mozilla.org <https://telemetry.mozilla.org/>`_.

The Telemetry team is there to help with any problems. You can reach us via:

- Matrix in `#telemetry:mozilla.org <https://chat.mozilla.org/#/room/#telemetry:mozilla.org>`_
- Slack in `#data-help <https://mozilla.slack.com/messages/data-help/>`_
- the `fx-data-dev mailing list <https://mail.mozilla.org/listinfo/fx-data-dev>`_
- flags for `one of the peers <https://wiki.mozilla.org/Modules/Toolkit#Telemetry>`_ on Bugzilla or send us an e-mail
