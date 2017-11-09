
"anonymous" ping
================

.. note::

    This ping is no longer sent by Firefox or Fennec.

This ping is only for product survey purpose and will not track/associate client ID. It's used
to evaluate custom tab usage and see which app is using our custom tab.

Submission interval & triggers
Since this ping is used to measure the feature usage, it should be sent each time the client app uses our custom tab.

Dataset:
Only opt-in users will send out this ping.
Since all other pings will collect client ID. We need this custom ping to not do that.

Size and volume:
The size of submitted payload is small. And this custom ping should be deprecated after it's released for 6 months.

Privacy:
We won't collect customer information so there'll be no PI leak.

Data contents:
The content of this ping will let us know which app is using our custom tab.
Just like other feature usage measurement, we only need it for opt-in users (which consider as heavy users).

Structure:

.. code-block:: js

    {
      "payload": {
        "client":  <string> // The package name of the caller app.
      }
      type: <string>, // "anonymous", "activation", "deletion", "saved-session", ...
      id: <UUID>, // a UUID that identifies this ping
      creationDate: <ISO date>, // the date the ping was generated
      version: <number>, // the version of the ping format, currently 4

      application: {
        architecture: <string>, // build architecture, e.g. x86
        buildId: <string>, // "20141126041045"
        name: <string>, // "Firefox"
        version: <string>, // "35.0"
        displayVersion: <string>, // "35.0b3"
        vendor: <string>, // "Mozilla"
        platformVersion: <string>, // "35.0"
        xpcomAbi: <string>, // e.g. "x86-msvc"
        channel: <string>, // "beta"
      },
    }

Field details
-------------

client
~~~~~~
It could be ``com.example.app``, which is the identifier of the app.

Version history
---------------
* v1: initial version - Will be shipped in `Fennec 55 <https://bugzilla.mozilla.org/show_bug.cgi?id=1329157>`_.

Notes
~~~~~
There's no option in this custom ping since we don't collect clientId nor environment data.
