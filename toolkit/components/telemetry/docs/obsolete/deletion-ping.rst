
"deletion" ping (obsolete)
==========================

This ping is generated when a user turns off FHR upload from the Preferences panel, changing the related ``datareporting.healthreport.uploadEnabled`` preference. This requests that all associated data from that user be deleted.

This ping contains the client id and no environment data.

Structure:

.. code-block:: js

    {
      version: 4,
      type: "deletion",
      ... common ping data
      clientId: <UUID>,
      payload: { }
    }

Version History
---------------

- Firefox 63:

  - Replaced by "optout" ping (`bug 1445921 <https://bugzilla.mozilla.org/show_bug.cgi?id=1445921>`_).
