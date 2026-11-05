# Reported-version follow-up reproduction

Read actual156 packagedsource/pref-source-evidence.json before launching. Extract fresh owned product from official12.18 archive after checking SHA256; never relabel the old updatedprefix. Snapshot full product file/symlink hashes.

Use supplied browser-session.py with --pin-version --fixture none, private DBus/Xvfb/ports, fresh profile and pre-start remote.prefs.recommended=false. The pin flag writes only app.update.disabledForTesting=true in this QA user.js. Preserve proxy/CA/stock checks. Confirm actual nsIApplicationUpdateServiceStub gate and Services.appinfo via check-fixed-version.py <case>, which also checks46archive hashes.

Seed: normal FloorpStart settings checkbox DisableFloorpStart, independent configread, normalQuit and wait for process exit before port/profile reuse. Native/restart: same ownedprofile, --reuse-profile --pin-version --uri about:newtab, fresh outputdir and freeports. Require native.content-main; do not call init/renderer. native-fixture.py registers the data favicon using alreadyinitialized nativeTopSites. run-native-paints.py performs supported3design×reload/cache then normalQuit; same script --restart-only checks persistedpaint after another normalprocessstartup withoutrepinning. Dismiss only ordinary defaultbrowser prompt via Escape. Do not alter TOU or security prefs.

At each startup run check-fixed-version.py. The paint script invokes its final checkpoint beforeQuit for reported-fixed cases. Require native156.0.1/build20260916130640/preftrue/servicegatetrue and all46hashes at everycheckpoint, plus finalafterQuit hash. Abort qualification on anydrift; don't update IDs or reuse changedcopy. Sourcescript errors are not productfailures. The oldinterface/probe mistakes remain in rawsource gate requests; v3 uses the correct shippedXPCOMstub.

Scripts carry absolute task/sysroot paths; adapt them only to another ownedQA root. Fixture URL is loopbackidentity with embeddedPNG, not proof of HTTPfaviconfetch. verify-evidence.py uses exportedrelativepaths to recheckactualpixels/checkpointhashes/exits. No Linuxupdate/MAR comparison or Windows rerun is partofthis followup.
