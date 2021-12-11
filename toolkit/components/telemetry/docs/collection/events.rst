.. _eventtelemetry:

======
Events
======

Across the different Firefox initiatives, there is a common need for a mechanism for recording, storing, sending & analysing application usage in an event-oriented format.
*Event Telemetry* specifies a common events data format, which allows for broader, shared usage of data processing tools.
Adding events is supported in artifact builds and build faster workflows.

For events recorded into Firefox Telemetry we also provide an API that opaquely handles storage and submission to our servers.

.. important::

    Every new or changed data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection>`__ from a Data Steward.

.. _events.serializationformat:

Serialization format
====================

Events are submitted in an :doc:`../data/event-ping` as an array, e.g.:

.. code-block:: js

  [
    [2147, "ui", "click", "back_button"],
    [2213, "ui", "search", "search_bar", "google"],
    [2892, "ui", "completion", "search_bar", "yahoo",
      {"querylen": "7", "results": "23"}],
    [5434, "dom", "load", "frame", null,
      {"prot": "https", "src": "script"}],
    // ...
  ]

Each event is of the form:

.. code-block:: js

  [timestamp, category, method, object, value, extra]

Where the individual fields are:

- ``timestamp``: ``Number``, positive integer. This is the time in ms when the event was recorded, relative to the main process start time.
- ``category``: ``String``, identifier. The category is a group name for events and helps to avoid name conflicts.
- ``method``: ``String``, identifier. This describes the type of event that occurred, e.g. ``click``, ``keydown`` or ``focus``.
- ``object``: ``String``, identifier. This is the object the event occurred on, e.g. ``reload_button`` or ``urlbar``.
- ``value``: ``String``, optional, may be ``null``. This is a user defined value, providing context for the event.
- ``extra``: ``Object``, optional, may be ``null``. This is an object of the form ``{"key": "value", ...}``, both keys and values need to be strings, keys are identifiers. This is used for events where additional richer context is needed.

.. _eventlimits:

Limits
------

Each ``String`` marked as an identifier (the event ``name``, ``category``, ``method``,
``object``, and the keys of ``extra``) is restricted to be composed of alphanumeric ASCII
characters ([a-zA-Z0-9]) plus infix underscores ('_' characters that aren't the first or last).
``category`` is also permitted infix periods ('.' characters, so long as they aren't the
first or last character).

For the Firefox Telemetry implementation, several fields are subject to length limits:

- ``category``: Max. byte length is ``30``.
- ``method``: Max. byte length is ``20``.
- ``object``: Max. byte length is ``20``.
- ``value``: Max. byte length is ``80``.
- ``extra``: Max. number of keys is ``10``.

  - Each extra key name: Max. string length is ``15``.
  - Each extra value: Max. byte length is ``80``.

Only ``value`` and the values of ``extra`` will be truncated if over the specified length.
Any other ``String`` going over its limit will be reported as an error and the operation
aborted.

.. _eventdefinition:

The YAML definition file
========================

Any event recorded into Firefox Telemetry must be registered before it can be recorded.
For any code that ships as part of Firefox that happens in `Events.yaml <https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Events.yaml>`_.

The probes in the definition file are represented in a fixed-depth, three-level structure. The first level contains *category* names (grouping multiple events together), the second level contains *event* names, under which the events properties are listed. E.g.:

.. code-block:: yaml

  # The following is a category of events named "browser.ui".
  browser.ui:
    click: # This is the event named "click".
      objects: ["reload-btn"] # List the objects for this event.
      description: >
        Describes this event in detail, potentially over
        multiple lines.
      # ... and more event properties.
    # ... and more events.
  # This is the "dom" category.
  search:
    # And the "completion" event.
    completion:
      # ...
      description: Recorded when a search completion suggestion was clicked.
      extra_keys:
        distance: The edit distance to the current search query input.
        loadtime: How long it took to load this completion entry.
    # ...

Category and event names are subject to the limits :ref:`specified above <eventlimits>`.

The following event properties are valid:

- ``methods`` *(optional, list of strings)*: The valid event methods. If not set this defaults to ``[eventName]``.
- ``objects`` *(required, list of strings)*: The valid event objects.
- ``description`` *(required, string)*: Description of the event and its semantics.
- ``release_channel_collection`` *(optional, string)*: This can be set to ``opt-in`` (default) or ``opt-out``.
- ``record_in_processes`` *(required, list of strings)*: A list of processes the event can be recorded in. Currently supported values are:

  - ``main``
  - ``content``
  - ``gpu``
  - ``all_children`` (record in all the child processes)
  - ``all`` (record in all the processes).

- ``bug_numbers`` *(required, list of numbers)*: A list of Bugzilla bug numbers that are relevant to this event.
- ``notification_emails`` *(required, list of strings)*: A list of emails of owners for this event. This is used for contact for data reviews and potentially to email alerts.
- expiry: There are two properties that can specify expiry, at least one needs to be set:

  - ``expiry_version`` *(required, string)*: The version number in which the event expires, e.g. ``"50"``, or ``"never"``. A version number of type "N" is automatically converted to "N.0a1" in order to expire the event also in the development channels. For events that never expire the value ``never`` can be used.

- ``extra_keys`` *(optional, object)*: An object that specifies valid keys for the ``extra`` argument and a description - see the example above.
- ``products`` *(required, list of strings)*: A list of products the event can be recorded on. Currently supported values are:

  - ``firefox`` - Collected in Firefox Desktop for submission via Firefox Telemetry.
  - ``thunderbird`` - Collected in Thunderbird for submission via Thunderbird Telemetry.

- ``operating_systems`` *(optional, list of strings)*: This field restricts recording to certain operating systems only. It defaults to ``all``. Currently supported values are:

   - ``mac``
   - ``linux``
   - ``windows``
   - ``android``
   - ``unix``
   - ``all`` (record on all operating systems)

.. note::

  Combinations of ``category``, ``method``, and ``object`` defined in the file must be unique.

The API
=======

Public JS API
-------------

``recordEvent()``
~~~~~~~~~~~~~~~~~

.. code-block:: js

  Services.telemetry.recordEvent(category, method, object, value, extra);

Record a registered event.

* ``value``: Optional, may be ``null``. A string value, limited to 80 bytes.
* ``extra``: Optional. An object with string keys & values. Key strings are limited to what was registered. Value strings are limited to 80 bytes.

Throws if the combination of ``category``, ``method`` and ``object`` is unknown.
Recording an expired event will not throw, but print a warning into the browser console.

.. note::

  Each ``recordEvent`` of a known non-expired combination of ``category``, ``method``, and
  ``object``, will be :ref:`summarized <events.event-summary>`.

.. warning::

  Event Telemetry recording is designed to be cheap, not free. If you wish to record events in a performance-sensitive piece of code, store the events locally and record them only after the performance-sensitive piece ("hot path") has completed.

Example:

.. code-block:: js

  Services.telemetry.recordEvent("ui", "click", "reload-btn");
  // event: [543345, "ui", "click", "reload-btn"]
  Services.telemetry.recordEvent("ui", "search", "search-bar", "google");
  // event: [89438, "ui", "search", "search-bar", "google"]
  Services.telemetry.recordEvent("ui", "completion", "search-bar", "yahoo",
                                 {"querylen": "7", "results": "23"});
  // event: [982134, "ui", "completion", "search-bar", "yahoo",
  //           {"qerylen": "7", "results": "23"}]

``setEventRecordingEnabled()``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  Services.telemetry.setEventRecordingEnabled(category, enabled);

Event recording is currently disabled by default for events registered in Events.yaml.
Dynamically-registered events (those registered using ``registerEvents()``) are enabled by default, and cannot be disabled.
Privileged add-ons and Firefox code can enable & disable recording events for specific categories using this function.

Example:

.. code-block:: js

  Services.telemetry.setEventRecordingEnabled("ui", true);
  // ... now events in the "ui" category will be recorded.
  Services.telemetry.setEventRecordingEnabled("ui", false);
  // ... now "ui" events will not be recorded anymore.

.. note::

  Even if your event category isn't enabled, counts of events that attempted to be recorded will
  be :ref:`summarized <events.event-summary>`.

.. _registerevents:

``registerEvents()``
~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  Services.telemetry.registerEvents(category, eventData);

Register new events from add-ons.

* ``category`` - *(required, string)* The category the events are in.
* ``eventData`` - *(required, object)* An object of the form ``{eventName1: event1Data, ...}``, where each events data is an object with the entries:

  * ``methods`` - *(required, list of strings)* The valid event methods.
  * ``objects`` - *(required, list of strings)* The valid event objects.
  * ``extra_keys`` - *(optional, list of strings)* The valid extra keys for the event.
  * ``record_on_release`` - *(optional, bool)*
  * ``expired`` - *(optional, bool)* Whether this event entry is expired. This allows recording it without error, but it will be discarded. Defaults to false.

For events recorded from add-ons, registration happens at runtime. Any new events must first be registered through this function before they can be recorded.
The registered categories will automatically be enabled for recording, and cannot be disabled.
If a dynamic event uses the same category as a static event, the category will also be enabled upon registration.

After registration, the events can be recorded through the ``recordEvent()`` function. They will be submitted in event pings like static events are, under the ``dynamic`` process.

New events registered here are subject to the same limitations as the ones registered through ``Events.yaml``, although the naming was in parts updated to recent policy changes.

When add-ons are updated, they may re-register all of their events. In that case, any changes to events that are already registered are ignored. The only exception is expiry; an event that is re-registered with ``expired: true`` will not be recorded anymore.

Example:

.. code-block:: js

  Services.telemetry.registerEvents("myAddon.interaction", {
    "click": {
      methods: ["click"],
      objects: ["red_button", "blue_button"],
    }
  });
  // Now events can be recorded.
  Services.telemetry.recordEvent("myAddon.interaction", "click", "red_button");

Internal API
------------

.. code-block:: js

  Services.telemetry.snapshotEvents(dataset, clear, eventLimit);
  Services.telemetry.clearEvents();

These functions are only supposed to be used by Telemetry internally or in tests.

Also, the ``event-telemetry-storage-limit-reached`` topic is notified when the event ping event
limit is reached (1000 event records).
This is intended only for use internally or in tests.

.. _events.event-summary:

Event Summary
=============

Calling ``recordEvent`` on any non-expired registered event will accumulate to a
:doc:`Scalar <scalars>` for ease of analysing uptake and usage patterns. Even if the event category
isn't enabled.

The scalar is ``telemetry.event_counts`` for statically-registered events (the ones in
``Events.yaml``) and ``telemetry.dynamic_event_counts`` for dynamically-registered events (the ones
registered via ``registerEvents``). These are :ref:`keyed scalars <scalars.keyed-scalars>` where
the keys are of the form ``category#method#object`` and the values are counts of the number of
times ``recordEvent`` was called with that combination of ``category``, ``method``, and ``object``.

These two scalars have a default maximum key limit of 500 per process.

Example:

.. code-block:: js

  // telemetry.event_counts summarizes in the same process the events were recorded

  // Let us suppose in the parent process this happens:
  Services.telemetry.recordEvent("interaction", "click", "document", "xuldoc");
  Services.telemetry.recordEvent("interaction", "click", "document", "xuldoc-neighbour");

  // And in each of child processes 1 through 4, this happens:
  Services.telemetry.recordEvent("interaction", "click", "document", "htmldoc");

In the case that ``interaction.click.document`` is statically-registered, this will result in the
parent-process scalar ``telemetry.event_counts`` having a key ``interaction#click#document`` with
value ``2`` and the content-process scalar ``telemetry.event_counts`` having a key
``interaction#click#document`` with the value ``4``.

All dynamically-registered events end up in the dynamic-process ``telemetry.dynamic_event_counts``
(notice the different name) regardless of in which process the events were recorded. From the
example above, if ``interaction.click.document`` was registered with ``registerEvents`` then
the dynamic-process scalar ``telemetry.dynamic_event_counts`` would have a key
``interaction#click#document`` with the value ``6``.

Testing
=======

Tests involving Event Telemetry often follow this four-step form:

1. ``Services.telemetry.clearEvents();`` To minimize the effects of prior code and tests.
2. ``Services.telemetry.setEventRecordingEnabled(myCategory, true);`` To enable the collection of
   your events. (May or may not be relevant in your case)
3. ``runTheCode();`` This is part of the test where you call the code that's supposed to collect
   Event Telemetry.
4. ``TelemetryTestUtils.assertEvents(expected, filter, options);`` This will check the
   events recorded by Event Telemetry against your provided list of expected events.
   If you only need to check the number of events recorded, you can use
   ``TelemetryTestUtils.assertNumberOfEvents(expectedNum, filter, options);``.
   Both utilities have `helpful inline documentation <https://hg.mozilla.org/mozilla-central/file/tip/toolkit/components/telemetry/tests/utils/TelemetryTestUtils.jsm>`_.


Version History
===============

- Firefox 79:  ``geckoview`` support removed (see `bug 1620395 <https://bugzilla.mozilla.org/show_bug.cgi?id=1620395>`__).
- Firefox 52: Initial event support (`bug 1302663 <https://bugzilla.mozilla.org/show_bug.cgi?id=1302663>`_).
- Firefox 53: Event recording disabled by default (`bug 1329139 <https://bugzilla.mozilla.org/show_bug.cgi?id=1329139>`_).
- Firefox 54: Added child process events (`bug 1313326 <https://bugzilla.mozilla.org/show_bug.cgi?id=1313326>`_).
- Firefox 56: Added support for recording new probes from add-ons (`bug 1302681 <bug https://bugzilla.mozilla.org/show_bug.cgi?id=1302681>`_).
- Firefox 58:

   - Ignore re-registering existing events for a category instead of failing (`bug 1408975 <https://bugzilla.mozilla.org/show_bug.cgi?id=1408975>`_).
   - Removed support for the ``expiry_date`` property, as it was unused (`bug 1414638 <https://bugzilla.mozilla.org/show_bug.cgi?id=1414638>`_).
- Firefox 61:

   - Enabled support for adding events in artifact builds and build-faster workflows (`bug 1448945 <https://bugzilla.mozilla.org/show_bug.cgi?id=1448945>`_).
   - Added summarization of events (`bug 1440673 <https://bugzilla.mozilla.org/show_bug.cgi?id=1440673>`_).
- Firefox 66: Replace ``cpp_guard`` with ``operating_systems`` (`bug 1482912 <https://bugzilla.mozilla.org/show_bug.cgi?id=1482912>`_)`
