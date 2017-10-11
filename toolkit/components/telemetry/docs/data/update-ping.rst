
"update" ping
==================

This opt-out ping is sent from Firefox Desktop when a browser update is ready to be applied and after it was correctly applied.

Structure:

.. code-block:: js

    {
      type: "update",
      ... common ping data
      clientId: <UUID>,
      environment: { ... },
      payload: {
        reason: <string>, // "ready", "success"
        targetChannel: <string>, // "nightly" (only present for reason = "ready")
        targetVersion: <string>, // "56.0a1" (only present for reason = "ready")
        targetBuildId: <string>, // "20080811053724" (only present for reason = "ready")
        targetDisplayVersion: <string>, // "56.0a1" (only present for reason = "ready")
        previousChannel: <string>, // "nightly" or null (only present for reason = "success")
        previousVersion: <string>, // "55.0a1" (only present for reason = "success")
        previousBuildId: <string>, // "20080810053724" (only present for reason = "success")
      }
    }

payload.reason
--------------
This field supports the following values:

- ``ready`` meaning that the ping was generated after an update was downloaded and marked as ready to be processed. For *non-staged* updates this happens as soon as the download finishes and is verified while for *staged* updates this happens before the staging step is started.
- ``success`` the ping was generated after the browser was restarted and the update correctly applied.

payload.targetChannel
-----------------------
The Firefox channel the update was fetched from (only valid for pings with reason "ready").

payload.targetVersion
-----------------------
The Firefox version the browser is updating to. Follows the same format as application.version (only valid for pings with reason "ready").

payload.targetBuildId
-----------------------
The Firefox build id the browser is updating to. Follows the same format as application.buildId (only valid for pings with reason "ready").

payload.targetDisplayVersion
----------------------------
The Firefox display version the browser is updating to. This may contain a different value than ``targetVersion``, e.g. for the ``Beta`` channel this field will report the beta suffix while ``targetVersion`` will only report the version number.

payload.previousChannel
-----------------------
The Firefox channel the profile was on before the update was applied (only valid for pings with reason "success").
This can be ``null``.

payload.previousVersion
-----------------------
The Firefox version the browser is updating from. Follows the same format as application.version (only valid for pings with reason "success").

payload.previousBuildId
-----------------------
The Firefox build id the browser is updating from. Follows the same format as application.buildId (only valid for pings with reason "success").

Expected behaviours
-------------------
The following is a list of conditions and expected behaviours for the ``update`` ping:

- **The ping is generated once every time an update is downloaded, after it was verified:**

  - *for users who saw the privacy policy*, the ``update`` ping is sent immediately;
  - *for users who did not see the privacy policy*, the ``update`` ping is saved to disk and sent after the policy is displayed.
- **If the download of the update retries or other fallback occurs**: the ``update`` ping will not be generated
  multiple times, but only one time once the download is complete and verified.
- **If automatic updates are disabled**: when the user forces a manual update, no ``update`` ping will be generated.
- **If updates fail to apply**: in some cases the client will download the same update blob and generate a new ``update`` ping for the same target version and build id, with a different document id.
- **If the build update channel contains the CCK keyword**, the update ping will not report it but rather report a vanilla channel name (e.g. ``mozilla-cck-test-beta`` gets reported as ``beta``).
- **If a profile refresh occurs before the update is applied**, the update ping with ``reason = success`` will not be generated.
- **If the update is applied on a new profile, different then the one it was downloaded in**, the update ping with ``reason = success`` will not be generated.
- **If a newer browser version is installed over an older**, the update ping with ``reason = success`` will not be generated.