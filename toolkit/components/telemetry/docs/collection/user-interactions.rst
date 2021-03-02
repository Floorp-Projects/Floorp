.. _userinteractionstelemetry:

=================
User Interactions
=================

The Background Hang Reporter is a tool that collects stacks during hangs on pre-release channels.
User Interactions are a way of annotating Background Hang Reports with additional information about what the user was doing when a hang occurs.
This allows for grouping and prioritization of hangs based on the user interactions that they occur during.

Since the built-in profiler is often the first tool that developers reach for to debug performance issues,
User Interactions also will add profiler markers for each recording.

.. important::

    Every new or changed data collection in Firefox needs a `data collection review <https://wiki.mozilla.org/Firefox/Data_Collection>`__ from a Data Steward.

.. _userinteractionsserializationformat:

Serialization format
====================

User Interactions are submitted in a :doc:`../data/backgroundhangmonitor-ping` as a property under the `annotations` for a hang, e.g.:

.. code-block:: js

  ...
  {
    "duration": 105.547582,
    // ...
    "annotations": [
      ["UserInteracting", "true"]
      ["browser.tabs.opening", "animated"]
    ],
    "stack": [
      "XREMain::XRE_main",
      "-[GeckoNSApplication nextEventMatchingMask:untilDate:inMode:dequeue:]",
      "nsAppShell::ProcessGeckoEvents",
      "nsRefreshDriver::Tick",
      "EventDispatcher::Dispatch",
      "EventDispatcher::Dispatch",
      "",
      "browser/content/tabbrowser-tabs.js:1699",
      "browser/content/tabbrowser-tabs.js:1725",
      "browser/content/tabbrowser-tabs.js:142",
      "browser/content/tabbrowser-tabs.js:153",
      "(jit frame)",
      "(unresolved)",
      [
        1,
        "418de17"
      ],
      [
        1,
        "418de91"
      ],
      [
        1,
        "4382e56"
      ],
      [
        8,
        "108e3"
      ],
      [
        9,
        "2624"
      ],
      [
        9,
        "129f"
      ]
    ]
    // ...
  },

Each User Interaction is of the form:

.. code-block:: js

  ["User Interaction ID", "value"]

A `User Interaction ID` is its category concatenated with its name.
For example, a User Interaction with category `browser.tabs` and name `opening` has an ID of `browser.tabs.opening`.

.. _userinteractionslimits:

Limits
------

Each ``String`` marked as an identifier (the User Interaction ``name``, ``category``, ``value``) is restricted to be composed of alphanumeric ASCII characters ([a-zA-Z0-9]) plus infix underscores ('_' characters that aren't the first or last).
``category`` is also permitted infix periods ('.' characters, so long as they aren't the first or last character).

Several fields are subject to length limits:

- ``category``: Max. byte length is ``40``.
- ``User Interaction`` name: Max. byte length is ``40``.
- ``value``: A UTF-8 string with max. byte length of ``50``.

Any ``String`` going over its limit will be reported as an error and the operation aborted.


.. _userinteractionsdefinition:

The YAML definition file
========================

Any User Interaction recorded into Firefox Telemetry must be registered before it can be recorded.
For any code that ships as part of Firefox that happens in `UserInteractions.yaml <https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/UserInteractions.yaml>`_.

The User Interactions in the definition file are represented in a fixed-depth, three-level structure.
The first level contains *category* names (grouping multiple User Interactions together),
the second level contains User Interaction IDs, under which the User Interaction properties are listed. E.g.:

.. code-block:: yaml

  # The following is a category of User Interactions named "browser.tabs".
  browser.tabs:
    opening: # This is the name of the User Interaction. The ID for the
             # User Interaction is browser.tabs.opening
      description: >
        Describes this User Interaction in detail, potentially over
        multiple lines.
      # ... and more User Interaction properties.
    # ... and more User Interactions.
  # This is the "browser.places" category.
  browser.places:
    # And the "history" search User Interaction. Its User Interaction ID is
    # browser.places.history_async
    history_async:
      # ...
      description: Session History is searched asynchronously.
      # ... and more User Interaction properties.
    # ...

Category and User Interaction names are subject to the limits :ref:`specified above <userinteractionslimits>`.


Profiler markers
================

The profiler markers automatically added for each User Interaction will have a starting point and ending point corresponding with the recording of the User Interaction.
The name of the marker will be the User Interaction category plus the User Interaction ID.
The value of the marker will be the value passed through the `UserInteraction` API, plus any additional text that is optionally added when the recording is finished.

Further details on what the profiler is and what profiler markers are can be found `here <https://profiler.firefox.com/docs/#/>`_.


The API
=======

Public JS API
-------------

This API is main-thread only, and all functions will return `false` if accessed off of the main thread.

``start()``
~~~~~~~~~~~~~~~~~

.. code-block:: js

  UserInteraction.start(id, value, object);

Starts recording a User Interaction.
Any hangs that occur on the main thread while recording this User Interaction result in an annotation being added to the background hang report.

If a pre-existing UserInteraction already exists with the same ``id`` and the same ``object``, that pre-existing UserInteraction will be overwritten.
The newly created UserInteraction will include a "(clobbered)" suffix on its BHR annotation name.

* ``id``: Required. A string value, limited to 80 characters. This is the category name concatenated with the User Interaction name.
* ``value``: Required. A string value, limited to 50 characters.
* ``object``: Optional. If specified, the User Interaction is associated with this object, so multiple recordings can be done concurrently.

Example:

.. code-block:: js

  UserInteraction.start("browser.tabs.opening", "animated", window1);
  UserInteraction.start("browser.tabs.opening", "animated", window2);

Returns `false` and logs a message to the browser console if the recording does not start for some reason.

Example:

.. code-block:: js

  UserInteraction.start("browser.tabs.opening", "animated", window);
  UserInteraction.start("browser.places.history_search", "synchronous");

``update()``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  UserInteraction.update(id, value, object);

Updates a User Interaction that's already being recorded with a new value.
Any hangs that occur on the main thread will be annotated using the new value.
Updating only works for User Interactions that are already being recorded.

* ``id``: Required. A string value, limited to 80 characters. This is the category name concatenated with the User Interaction name.
* ``value``: Required. The new string value, limited to 50 characters.
* ``object``: Optional. If specified, the User Interaction is associated with this object, so multiple recordings can be done concurrently.

Returns `false` and logs a message to the browser console if the update cannot be done for some reason.


Example:

.. code-block:: js

  // At this point, we don't know if the tab will open with animation
  // or not.
  UserInteraction.start("browser.tabs.opening", "initting", window);
  // ...
  if (animating) {
    UserInteraction.update("browser.tabs.opening", "animating", window);
  } else {
    UserInteraction.update("browser.tabs.opening", "not-animating", window);
  }

``cancel()``
~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  UserInteraction.cancel(id, object);

Cancels a recording User Interaction.
No profiler marker will be added in this case, and no further hangs will be annotated.
Hangs that occurred before the User Interaction was cancelled will not, however, be expunged.

* ``id``: Required. A string value, limited to 80 characters. This is the category name concatenated with the User Interaction name.
* ``object``: Optional. If specified, the User Interaction is associated with this object, so multiple recordings can be done concurrently.

Returns `false` and logs a message to the browser console if the cancellation cannot be completed for some reason.

``running()``
~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  UserInteraction.running(id, object);

Checks to see if a UserInteraction is already running.

* ``id``: Required. A string value, limited to 80 characters. This is the category name concatenated with the User Interaction name.
* ``object``: Optional. If specified, the User Interaction is associated with this object, so multiple recordings can be done concurrently. If you're checking for a running timer that was started with an object, you'll need to pass in that same object here to check its running state.

Returns `true` if a UserInteraction is already running.

``finish()``
~~~~~~~~~~~~~~~~~~~~

.. code-block:: js

  UserInteraction.finish(id, object, additionalText);

Finishes recording the User Interaction.
Any hangs that occur on the main thread will no longer be annotated with this User Interaction.
A profiler marker will also be added, starting at the `UserInteraction.start` point and ending at the `UserInteraction.finish` point, along with any additional text that the author wants to include.

* ``id``: Required. A string value, limited to 80 characters. This is the category name concatenated with the User Interaction name.
* ``object``: Optional. If specified, the User Interaction is associated with this object, so multiple recordings can be done concurrently.
* ``additionalText``: Optional. If specified, the profile marker will have this text appended to the `value`, separated with a comma.

Returns `false` and logs a message to the browser console if finishing cannot be completed for some reason.

Version History
===============

- Firefox 84:  Initial User Interaction support (see `bug 1661304 <https://bugzilla.mozilla.org/show_bug.cgi?id=1661304>`__).
