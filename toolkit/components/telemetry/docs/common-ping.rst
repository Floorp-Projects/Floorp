
Common ping format
==================

This defines the top-level structure of a Telemetry ping.
It contains basic information shared between different ping types, which enables proper storage and processing of the raw pings server-side.

It also contains optional further information:

* the :doc:`environment data <environment>`, which contains important info to correlate the measurements against
* the ``clientId``, a UUID identifying a profile and allowing user-oriented correlation of data

*Note:* Both are not submitted with all ping types due to privacy concerns. This and the data it that can be correlated against is inspected under the `data collection policy <https://wiki.mozilla.org/Firefox/Data_Collection>`_.

Finally, the structure also contains the `payload`, which is the specific data submitted for the respective *ping type*.

Structure::

    {
      type: <string>, // "main", "activation", "deletion", ...
      id: <UUID>, // a UUID that identifies this ping
      creationDate: <ISO date>, // the date the ping was generated
      version: <number>, // the version of the ping format, currently 2

      application: {
        architecture: <string>, // build architecture, e.g. x86
        buildId: <string>, // "20141126041045"
        name: <string>, // "Firefox"
        version: <string>, // "35.0"
        vendor: <string>, // "Mozilla"
        platformVersion: <string>, // "35.0"
        xpcomAbi: <string>, // e.g. "x86-msvc"
        channel: <string>, // "beta"
      },

      clientId: <UUID>, // optional
      environment: { ... }, // optional, not all pings contain the environment
      payload: { ... }, // the actual payload data for this ping type
    }
