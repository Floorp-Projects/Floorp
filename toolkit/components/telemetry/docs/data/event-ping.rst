
"event" ping
=============

This ping transmits :ref:`Telemetry Event <eventtelemetry>` records.

The client id is submitted with this ping.
The :doc:`Telemetry Environment <../data/environment>` is submitted in this ping.

.. code-block:: js

    {
      "type": "event",
      ... common ping data
      "clientId": <UUID>,
      "environment": { ... },
      "payload": {
        "reason": {periodic, max, shutdown}, // Why the ping was submitted
        "processStartTimestamp": <UNIX Timestamp>, // Minute precision, for calculating absolute time across pings
        "sessionId": <UUID>, // For linking to "main" pings
        "subsessionId": <UUID>, // For linking to "main" pings
        "lostEventsCount": <number>, // How many events we had to drop. Valid only for reasons "max" and "shutdown"
        "events": {
          "parent": [ // process name, one of the keys from Processes.yaml
            [timestamp, category, method, object, value, extra],
            ... // At most 1000
          ]
        }
      }
    }

Send behavior
-------------

The ping is submitted at most once per ten minute interval, and at least once per hour in
which an event was recorded. Upon reaching 1000 events, the ping is sent immediately
unless it would be within ten minutes of the previous ping, in which case some event
records may be lost. A count of these lost records is included in the ping.
to avoid losing collected data.

On shutdown, during profile-before-change, a final ping is sent with any remaining event
records, regardless of frequency but obeying the event record limit.

The 1000-record limit and one-hour and ten-minute frequencies are controlled by
:doc:`preferences <../internals/preferences>`.

Field details
-------------

reason
~~~~~~
The ``reason`` field contains the information about why the "event" ping was submitted:

* ``periodic``: The event ping was submitted because at least one event happened in the past hour.
* ``max``: The event ping was submitted because the 1000-record limit was reached.
* ``shutdown``: The event ping was submitted because Firefox is shutting down and some events
  have yet to be submitted.

processStartTimestamp
~~~~~~~~~~~~~~~~~~~~~
The minute the user's Firefox main process was created. Event record timestamps are recorded
relative to Firefox's main process start. This provides the basis for reconstructing a user's full
session of events in order, as well as offering a mechanism for grouping event pings.

sessionId
~~~~~~~~~
The id of the session that was current when the ping was sent.

subsessionId
~~~~~~~~~~~~
The id of the subsession that was current when the ping was sent.

.. note::

  This may not be the same subsession that the events occurred in if a
  :ref:`session split <sessionsplit>` happened in between.

lostEventsCount
~~~~~~~~~~~~~~~
The number of events we had to discard because we reached the 1000-per-ping limit before
we were able to send the ping. Should only have a non-zero value on "event" pings with
reason set to "max" or "shutdown". Which events are missing should be calculable via the
client's "main" pings using :ref:`Event Summary <events.event-summary>`.

events
~~~~~~
A map from process names to arrays of event records that have been :ref:`serialized <events.serializationformat>`.

Version History
---------------

- Firefox 62: Started sending the "event" ping (`bug 1460595 <https://bugzilla.mozilla.org/show_bug.cgi?id=1460595>`_).
