
"downgrade" ping
================

This ping is captured when attempting to use a profile that was previously used
with a newer version of the application.

This ping is submitted directly through the ```pingsender```. The common ping
data relates to the profile and application that the user attempted to use.

The client ID is submitted with this ping. No environment block is included with
this ping.

Structure:

.. code-block:: js

    {
      type: "downgrade",
      ... common ping data
      clientId: <UUID>,
      payload: {
        lastVersion: "", // The last version of the application that ran this profile
        hasSync: <bool>, // Whether the profile is signed in to sync
        hasBinary: <bool>, // Whether the last version of the application is available to run
        button: <int> // The button the user chose to click from the UI:
                      //   0 - Quit
                      //   1 - Create new profile
      }
    }
