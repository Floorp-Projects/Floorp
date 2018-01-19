
"health" ping
=============

This ping is intended to provide data about problems arise when submitting other pings.
The ping is submitted at most once per hour. On shutdown an additional ping is submitted
to avoid losing collected data.

This ping is intended to be really small.
The client id is submitted with this ping.

.. code-block:: js

    {
      "type": "health", // type
      ... common ping data
      "clientId": <UUID>, // client id, e.g.
                            // "c641eacf-c30c-4171-b403-f077724e848a"
      "payload": {
        "os": {
            "name": <string>, // OS name
            "version": <string> // OS version
        },
        "reason": <string>, // When ping was triggered, e.g. "immediate" or "shutdown".
        "pingDiscardedForSize": {
            "main": <number>, // Amount of occurrences for a specific ping type.
            "core": <number>
            ...
        },
        "sendFailure": {
            "timeout": <number>, // Amount of occurrences for a specific failure.
            "abort": <number>
            ...
        }
      }
    }

Send behavior
-------------

``TelemetryHealthPing.jsm`` tracks several problems:

* The size of other assembled pings exceeds the ping limit.
* Failures while sending other pings.

After recording the data, a health ping will be sent:

* immediately, with the reason ``immediate`` , if it is first ping in the session or it passed at least one hour from the previous submission.
* after 1 hour minus the time passed from previous submission, with the reason ``delayed`` , if less than an hour passed from the previous submission.
* on shutdown, with the reason ``shutdown`` using :doc:`../internals/pingsender`, if recorded data is not empty.

Field details
-------------

reason
~~~~~~
The ``reason`` field contains the information about why the "health" ping was submitted. It presently supports three reasons:

* immediate: The health ping was submitted immediately after recording a failure.
* delayed: The health ping was submitted after a delay.
* shutdown: The health ping was submitted on shutdown.

pingDiscardedForSize
~~~~~~~~~~~~~~~~~~~~
The ``pingDiscardedForSize`` field contains the information about the top ten pings whose size exceeded the
ping size limit (1 MB). This field lists the number of discarded pings per ping type.

The ping type "<unknown>" is used to indicate that a pending pings size exceeded the limit. This is because we don't have the pending pings type available cheaply at the moment.

This field is optional.

sendFailure
~~~~~~~~~~~
The ``sendFailure`` field contains the information about pings, which had failures on sending.
This field lists the number of failed pings per ping send failure type.

The recorded failure types are:

* "eOK" - No error.
* "eRequest" - There was some error in the request before we started to service it.
* "eUnreachable" - The remote server was unreachable.
* "eChannelOpen" - The connection failed when we tried to open the channel.
* "eRedirect" - The connection failed when being redirected.
* "abort" - What XMLHttpRequest means by "abort" (see `MDN <https://developer.mozilla.org/en-US/docs/Web/Events/abort>`_)
* "timeout" - What XMLHttpRequest means by "timeout" (see `MDN <https://developer.mozilla.org/en-US/docs/Web/Events/timeout>`_)

This field is optional.

.. note::

    Although both ``pingDiscardedForSize`` and ``sendFailure`` fields are optional, the health ping will only
    be submitted if one of this field not empty.
