# Reproduction

Use isolated HOME/profile and free Xvfb207/Marionette28799/fixture18799. session.py runs the protected product and loopback fixture, polls command.json, writes result-ID.json. Requires preinstalled marionette_driver venv and Xvfb/sysroot paths as recorded, certutil and inherited proxy/CA. Never source the legacy .floorp-cloud/env.sh sandbox override.

The runner only stops owned product/display on stop command. For a fresh replay copy runner and signed XPI to a new directory and change its P path accordingly; do not delete prior evidence/profile. Set xpinstall.signatures.required=true before install, use AddonManager.getInstallForFile then install. Check signedState2/active. Open moz-extension://<actual UUID>/1p-filters.html, enter the two report filters in My filters and apply.

Configure floorp.panelSidebar.data entry {id:"qa-ubo",type:"web",url:"http://127.0.0.1:18799/panel",width:600,userContextId:0,userAgent:false}; use real [data-panel-id="qa-ubo"] button to open. Load /tab in normal tab. Compare computed display, image width and requests.json. Read panel remote document via messageManager frame script only for observation; no DOM mutation used for pass results.

Obtain actual tab IDs with ExtensionParent.apiManager.global.tabTracker.getId(normalTab/panelWindow.gBrowser.selectedTab). Open uBO popup-fenix.html?tabId=<id>&intab=1 in actual normal tab. Switch Marionette to that tab handle before clicking #gotoPick; popup closes itself on launch, so create a fresh popup for the other target. Observe genuine overlay and screenshot. A popup closing itself or stale tab handle is harness behavior, not product failure.

Attempt YouTube separately; record network error if proxy blocks. Stop runner with command.json {"stop":true}. Existing result JSON and screenshot files record executed observations.
