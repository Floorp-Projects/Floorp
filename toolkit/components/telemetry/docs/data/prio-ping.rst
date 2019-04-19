
"prio" ping
===========

This ping transmits :ref:`Origin Telemetry <origintelemetry>` data.

The client id is not submitted with this ping.
The :doc:`Telemetry Environment <../data/environment>` is not submitted in this ping.

.. code-block:: js

    {
      "type": "prio",
      ... common ping data
      "payload": {
        "reason": {periodic, max, shutdown}, // Why the ping was submitted
        "prioData": [{
          encoding: <encoding name>, // Name of App Encoding applied. e.g. "content-blocking-1"
          prio: {
            // opaque prio-specific payload. Like { a: <base64 string>, b: <base64 string> }
          },
        }, ... ],
      }
    }

Scheduling
----------

The ping is submitted at least once a day for sessions that last longer than 24h.
The ping is immediately sent if the ``prioData`` array has 10 elements.
The ping is submitted on shutdown.

The ping is enabled on pre-release channels (Nightly, Beta), except when disabled by :doc:`preference <../internals/preferences>`.
The 10-element limit is also controlled by :doc:`preference <../internals/preferences>`.

Field details
-------------

reason
~~~~~~
The ``reason`` field contains the information about why the "prio" ping was submitted:

* ``periodic``: The ping was submitted because some Origin Telemetry was recorded in the past day.
* ``max``: The ping was submitted because the 10-element limit was reached.
* ``shutdown``: The ping was submitted because Firefox is shutting down and some Origin Telemetry data have yet to be submitted.

prioData
~~~~~~~~
An array of ``encoding``/``prio`` pairs.

Multiple elements of the ``prioData`` array may have the same ``encoding``.
This is how we encode how many times something happened.

.. _prio-ping.encoding:

encoding
~~~~~~~~
The name of the encoding process used to encode the payload.
In short: This field identifies how :ref:`Origin Telemetry <origintelemetry>` encoded information of the form "tracker 'example.net' was blocked" into a boolean, split it into small enough lists for Prio to encode, encrypted it with a Prio batch ID, and did all the rest of the stuff it needed to do in order for it to be sent.
This name enables the servers to be able to make sense of the base64 strings we're sending it so we can get the aggregated information out on the other end.

See :ref:`Encoding <origin.encoding>` for more details.

prio
~~~~
The prio-encoded data.
For the prototype's encoding this is an object of the form:

.. code-block:: js

  {
    a: <base64 string>,
    b: <base64 string>,
  }

Version History
---------------

- Firefox 68: Initial Origin Telemetry support (Nightly only) (`bug 1536565 <https://bugzilla.mozilla.org/show_bug.cgi?id=1536565>`_).
