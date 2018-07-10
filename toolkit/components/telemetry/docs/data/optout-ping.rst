
"optout" ping
=============

This ping is generated when a user turns off FHR upload from the Preferences panel, changing the related ``datareporting.healthreport.uploadEnabled`` :doc:`preference <../internals/preferences>`.

This ping contains no client id and no environment data.

Structure:

.. code-block:: js

    {
      version: 4,
      type: "optout",
      ... common ping data
      payload: { }
    }

Expected behaviours
-------------------
The following is a list of expected behaviours for the ``optout`` ping:

- Sending the "optout" ping is best-effort. Telemetry tries to send the ping once and discards it immediately if sending fails.
- The ping might be delayed if ping sending is throttled (e.g. around midnight).
- The ping can be lost if the browser is shutdown before the ping is sent out the "optout" ping is discarded.

Version History
---------------

- Firefox 63:

  - "optout" ping replaced the "deletion" ping (`bug 1445921 <https://bugzilla.mozilla.org/show_bug.cgi?id=1445921>`_).
