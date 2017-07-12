
"update" ping
==================

This opt-out ping is sent from Firefox Desktop when a browser update is ready to be applied. There is a
plan to send this ping after an update is successfully applied and the work will happen in `bug 1380256 <https://bugzilla.mozilla.org/show_bug.cgi?id=1380256>`_.

Structure:

.. code-block:: js

    {
      type: "update",
      ... common ping data
      clientId: <UUID>,
      environment: { ... },
      payload: {
        reason: <string>, // "ready"
        targetChannel: <string>, // "nightly"
        targetVersion: <string>, // "56.01a"
        targetBuildId: <string>, // "20080811053724"
      }
    }

payload.reason
--------------
This field only supports the value ``ready``, meaning that the ping was generated after an update was downloaded
and marked as ready to be processed. For *non-staged* updates this happens as soon as the download
finishes and is verified while for *staged* updates this happens before the staging step is started.

payload.targetChannel
-----------------------
The Firefox channel the update was fetched from (only valid for pings with reason "ready").

payload.targetVersion
-----------------------
The Firefox version the browser is updating to. Follows the same format a application.version (only valid for pings with reason "ready").

payload.targetBuildId
-----------------------
The Firefox build id the browser is updating to. Follows the same format a application.buildId (only valid for pings with reason "ready").

Expected behaviours
-------------------
The following is a list of conditions and expected behaviours for the ``update`` ping:

- **The ping is generated once every time an update is downloaded, after it was verified:**

  - *for users who saw the privacy policy*, the ``update`` ping is sent immediately;
  - *for users who did not see the privacy policy*, the ``update`` ping is saved to disk and after the policy is displayed.
- **If the download of the update retries or other fallback occur**: the ``update`` ping will not be generated
  multiple times, but only one time once the download is complete and verified.
