.. _origintelemetry:

================
Origin Telemetry
================

*Origin Telemetry* is an experimental Firefox Telemetry mechanism that allows us to privately report origin-specific information in aggregate.
In short, it allows us to get exact counts of how *many* Firefox clients do certain things on specific origins without us being able to know *which* clients were doing which things on which origins.

As an example, Content Blocking would like to know which trackers Firefox blocked most frequently.
Origin Telemetry allows us to count how many times a given tracker is blocked without being able to find out which clients were visiting pages that had those trackers on them.

.. important::

    This mechanism is experimental and is a prototype.
    Please do not try to use this without explicit permission from the Firefox Telemetry Team, as it's really only been designed to work for Content Blocking right now.

Adding or removing Origins or Metrics is not supported in artifact builds and build faster workflows. A non-artifact Firefox build is necessary to change these lists.

This mechanism is enabled on Firefox Nightly only at present.

.. important::

    Every new data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection#Requesting_Approval>`_ from a data collection peer.

Privacy
=======

To achieve the necessary goal of getting accurate counts without being able to learn which clients contributed to the counts we use a mechanism called `Prio (pdf) <https://www.usenix.org/system/files/conference/nsdi17/nsdi17-corrigan-gibbs.pdf>`_.

Prio uses cryptographic techniques to encrypt information and a proof that the information is correct, only sending the encrypted information on to be aggregated.
Only after aggregation do we learn the information we want (aggregated counts), and at no point do we learn the information we don't want (which clients contributed to the counts).

.. _origin.usage:

Using Origin Telemetry
======================

To record that something happened on a given origin, three things must happen:

1. The origin must be one of the fixed, known list of origins. ("Where" something happened)
2. The metric must be one of the fixed, known list of metrics. ("What" happened)
3. A call must be made to the Origin Telemetry API. (To let Origin Telemetry know "that" happened "there")

At present the lists of origins and metrics are hardcoded in C++.
Please consult the Firefox Telemetry Team before changing these lists.

Origins can be arbitrary byte sequences of any length.
Do not add duplicate origins to the list.

If an attempt is made to record to an unknown origin, a meta-origin ``__UNKNOWN__`` captures that it happened.
Unlike other origins where multiple recordings are considered additive ``__UNKNOWN__`` only accumulates a single value.
This is to avoid inflating the ping size in case the caller submits a lot of unknown origins for a given unit (e.g. pageload).

Metrics should be of the form ``categoryname.metric_name``.
Both ``categoryname`` and ``metric_name`` should not exceed 40 bytes (UTF-8 encoded) in length and should only contain alphanumeric character and infix underscores.

.. _origin.API:

API
===

Origin Telemetry supplies APIs for recording information into and snapshotting information out of storage.

Recording
---------

``Telemetry::RecordOrigin(aOriginMetricID, aOrigin);``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This C++ API records that a metric was true for a given origin.
For instance, maybe the user visited a page in which content from ``example.net`` was blocked.
That call might look like ``Telemetry::RecordOrigin(OriginMetricID::ContentBlocking_Blocked, NS_LITERAL_CSTRING("example.net"))``.

Snapshotting
------------

``let snapshot = await Telemetry.getEncodedOriginSnapshot(aClear);``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This JS API provides a snapshot of the prio-encoded payload and is intended to only be used to assemble the :doc:`"prio" ping's <../data/prio-ping>` payload.
It returns a Promise which resolves to an object of the form:

.. code-block:: js

  {
    a: <base64-encoded, prio-encoded data>,
    b: <base64-encoded, prio-encoded data>,
  }

``let snapshot = Telemetry.getOriginSnapshot(aClear);``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This JS API provides a snapshot of the unencrypted storage of unsent Origin Telemetry, optionally clearing that storage.
It returns a structure of the form:

.. code-block:: js

  {
    "categoryname.metric_name": {
      "origin1": count1,
      "origin2": count2,
      ...
    },
    ...
  }

.. important::

    This API is only intended to be used by ``about:telemetry`` and tests.

.. _origin.example:

Example
=======

Firefox Content Blocking blocks web content from certain origins present on a list.
Users can exempt certain origins from being blocked.
To improve Content Blocking's effectiveness we need to know these two "whats" of information about that list of "wheres".

This means we need two metrics ``contentblocking.blocked`` and ``contentblocking.exempt`` (the "whats"), and a list of origins (the "wheres").

Say "example.net" was blocked and "example.com" was exempted from blocking.
Content Blocking calls ``Telemetry::RecordOrigin(OriginMetricID::ContentBlocking_Blocked, NS_LITERAL_CSTRING("example.net"))`` and ``Telemetry::RecordOrigin(OriginMetricID::ContentBlocking_Exempt, NS_LITERAL_CSTRING("example.com"))``.

At this time a call to ``Telemetry.getOriginSnapshot()`` would return:

.. code-block:: js

  {
    "contentblocking.blocked": {"example.net": 1},
    "contentblocking.exempt": {"example.com": 1},
  }

Later, Origin Telemetry will get the encoded snapshot (clearing the storage) and assemble it with other information into a :doc:`"prio" ping <../data/prio-ping>` which will then be submitted.

.. _origin.encoding:

Encoding
========

.. note::

    This section is provided to help you understand the client implementation's architecture.
    If how we arranged our code doesn't matter to you, feel free to ignore.

There are three levels of encoding in Origin Telemetry: App Encoding, Prio Encoding, and Base64 Encoding.

*App Encoding* is the process by which we turn the Metrics and Origins into data structures that Prio can encrypt for us.
Prio, at time of writing, only supports counting up to 2046 "true/false" values at a time.
Thus, from the example, we need to turn "example.net was blocked" into "the boolean at index 11 of chunk 2 is true".
This encoding can be done any way we like so long as we don't change it without informing the aggregation servers (by sending it a new :ref:`encoding name <prio-ping.encoding>`).
This encoding provides no privacy benefit and is just a matter of transforming the data into a format Prio can process.

*Prio Encoding* is the process by which those ordered true/false values that result from App Encoding are turned into an encrypted series of bytes.
You can `read the paper (pdf) <https://www.usenix.org/system/files/conference/nsdi17/nsdi17-corrigan-gibbs.pdf>`_ to learn more about that.
This encoding, together with the overall system architecture, is what provides the privacy quality to Origin Telemetry.

*Base64 Encoding* is how we turn those encrypted bytes into a string of characters we can send over the network.
You can learn more about Base64 encoding `on wikipedia <https://wikipedia.org/wiki/Base64>`_.
This encoding provides no privacy benefit and is just used to make Data Engineers' lives a little easier.

Version History
===============

- Firefox 68: Initial Origin Telemetry support (Nightly Only) (`bug 1536565 <https://bugzilla.mozilla.org/show_bug.cgi?id=1536565>`_).
