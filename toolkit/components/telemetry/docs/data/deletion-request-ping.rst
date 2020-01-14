"deletion-request" ping
=======================

This ping is submitted when a user opts out of sending technical and interaction data to Mozilla.
(In other words, when the
``datareporting.healthreport.uploadEnabled``
:doc:`preference <../internals/preferences>` is set to ``false``.)

This ping contains the client id.
This ping does not contain any environment data.

This ping is intended to communicate to the Data Pipeline that the user wishes
to have their reported Telemetry data deleted.
As such it attempts to send itself at the moment the user opts out of data collection,
and continues to try and send itself.

This ping contains scalars present in the "deletion-request" store.

Structure:

.. code-block:: js

    {
      version: 4,
      type: "deletion-request",
      ... common ping data (including clientId)
      payload: {
        scalars: {
          <process-type>: { // like "parent" or "content"
            <id name>: <id>, // like "deletion.request.impression_id": "<RFC 4122 GUID>"
          },
        },
      }
    }

Expected behaviours
-------------------
The following is a list of expected behaviours for the ``deletion-request`` ping:

- Telemetry will try to send the ping even if upload is disabled.
- Telemetry may persist this ping if it can't be immediately sent, and may try to resend it later.

Version History
---------------

- Firefox 72:

  - "deletion-request" ping replaces the "optout" ping (`bug 1585410 <https://bugzilla.mozilla.org/show_bug.cgi?id=1585410>`_).

- Firefox 73:

  - Added support for subordinate ids in the "deletion-request" store (`bug 1604312 <https://bugzilla.mozilla.org/show_bug.cgi?id=1604312>`_).
