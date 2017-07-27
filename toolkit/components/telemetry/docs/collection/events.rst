.. _eventtelemetry:

======
Events
======

Across the different Firefox initiatives, there is a common need for a mechanism for recording, storing, sending & analysing application usage in an event-oriented format.
*Event Telemetry* specifies a common events dataformat, which allows for broader, shared usage of data processing tools.

For events recorded into Firefox Telemetry we also provide an API that opaquely handles storage and submission to our servers.

.. important::

    Every new data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection#Requesting_Approval>`_ from a data collection peer. Just set the feedback? flag for :bsmedberg or one of the other data peers. We try to reply within a business day.

Serialization format
====================

Events are submitted as an array, e.g.:

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
- ``method``: ``String``, identifier. This describes the type of event that occured, e.g. ``click``, ``keydown`` or ``focus``.
- ``object``: ``String``, identifier. This is the object the event occured on, e.g. ``reload_button`` or ``urlbar``.
- ``value``: ``String``, optional, may be ``null``. This is a user defined value, providing context for the event.
- ``extra``: ``Object``, optional, may be ``null``. This is an object of the form ``{"key": "value", ...}``, both keys and values need to be strings. This is used for events where additional richer context is needed.

.. _eventlimits:

Limits
------

Each ``String`` marked as an identifier is restricted to the following regex pattern: ``^[:alpha:][:alnum:_.]*[:alnum:]$``.

For the Firefox Telemetry implementation, several fields are subject to limits:

- ``category``: Max. byte length is ``30``.
- ``method``: Max. byte length is ``20``.
- ``object``: Max. byte length is ``20``.
- ``value``: Max. byte length is ``80``.
- ``extra``: Max. number of keys is ``10``.
  - Each extra key name: Max. string length is ``15``.
  - Each extra value: Max. byte length is ``80``.

The YAML definition file
========================

Any event recorded into Firefox Telemetry must be registered before it can be recorded.
This happens in `Events.yaml <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Events.yaml>`_.

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

Category and probe names are subject to the limits and regex patterns :ref:`specified above <eventlimits>`.

The following event properties are valid:

- ``methods`` *(optional, list of strings)*: The valid event methods. If not set this defaults to ``[eventName]``.
- ``objects`` *(required, list of strings)*: The valid event objects.
- ``description`` *(required, string)*: Description of the event and its semantics.
- ``release_channel_collection`` *(optional, string)*: This can be set to ``opt-in`` (default) or ``opt-out``.
- ``record_in_processes`` *(required, list of strings)*: A list of processes the event can be recorded in. Currently supported values are:

  - ``main``
  - ``content``
  - ``gpu``
  - ``all_child`` (record in all the child processes)
  - ``all`` (record in all the processes).

- ``bug_numbers`` *(required, list of numbers)*: A list of bugzilla bug numbers that are relevant to this event.
- ``notification_emails`` *(required, list of strings)*: A list of emails of owners for this event. This is used for contact for data reviews and potentially to email alerts.
- expiry: There are two properties that can specify expiry, at least one needs to be set:

  - ``expiry_version`` *(string)*: The version number in which the event expires, e.g. ``"50"``, or ``"never"``. A version number of type "N" and "N.0" is automatically converted to "N.0a1" in order to expire the event also in the development channels. For events that never expire the value ``never`` can be used.
  - ``expiry_date`` *(date)*: A date of the format ``2014-01-28``. If the local client clock reaches this date, the event will expire and not be recorded.

- ``extra_keys`` *(optional, object)*: An object that specifies valid keys for the ``extra`` argument and a description - see the example above.

The API
=======

Public JS API
-------------

``recordEvent()``
~~~~~~~~~~~~~~~~~

.. code-block:: js

  Services.telemetry.recordEvent(category, method, object, value, extra);

Record a registered event.

* ``value``: Optional, may be ``null``. A string value, limitted to 80 bytes.
* ``extra``: Optional. An object with string keys & values. Key strings are limitted to what was registered. Value strings are limitted to 80 bytes.

Throws if the combination of ``category``, ``method`` and ``object`` is unknown.
Recording an expired event will not throw, but print a warning into the browser console.

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

.. code-block:: js

  Services.telemetry.setEventRecordingEnabled(category, enabled);

Event recording is currently disabled by default. Privileged addons and Firefox code can enable & disable recording events for specific categories using this function.

Example:

.. code-block:: js

  Services.telemetry.setEventRecordingEnabled("ui", true);
  // ... now events in the "ui" category will be recorded.
  Services.telemetry.setEventRecordingEnabled("ui", false);
  // ... now "ui" events will not be recorded anymore.

Internal API
~~~~~~~~~~~~

.. code-block:: js

  Services.telemetry.snapshotBuiltinEvents(dataset, clear);
  Services.telemetry.clearEvents();

These functions are only supposed to be used by Telemetry internally or in tests.

Version History
===============

- Firefox 52: Initial event support (`bug 1302663 <https://bugzilla.mozilla.org/show_bug.cgi?id=1302663>`_).
- Firefox 53: Event recording disabled by default (`bug 1329139 <https://bugzilla.mozilla.org/show_bug.cgi?id=1329139>`_).
- Firefox 54: Added child process events (`bug 1313326 <https://bugzilla.mozilla.org/show_bug.cgi?id=1313326>`_).
