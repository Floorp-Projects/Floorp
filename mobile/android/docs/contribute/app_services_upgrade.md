---
layout: page
title: Application Services version upgrades
permalink: /contributing/app-services-upgrades
---

Upgrading to a newer [version of App Services][as_version] upgrades the dependencies for multiple components, so it can be prudent to run a quick smoke test before it lands to ensure new functionality works and there are no regressions in the existing components.

## Automated PR testing

A quick first step is to run the automated smoke test on the Android Components PR by commenting:

```
bors try
```

This will run the `test-ui-browser` task, which will execute on-device tests of a sample browser built against the A-S version. If it's totally broken, that should blow up.

## Fenix

The next step is to build a [local version of Android Components][local_build] against [Fenix][fenix] and test the followings items:
 - Check general storage components works by adding/editing/removing: bookmarks, history
 - Sign-in/Sign-up for Sync
 - QR code pairing for Sync
 - Changing settings in “Choose what to sync” updates on desktop and vice versa
 - Rename device and verify on desktop
 - Sync Now button works
 - Browse passwords, auto-fill, save new passwords, update them
 - Receiving tab works
   - This needs push messaging keys, but we can test this without keys by using the “Sync Now” button
 - Sending tabs
 - Synced Tabs shows results from another device and vice versa

[as_version]: https://github.com/mozilla-mobile/android-components/blob/main/buildSrc/src/main/java/Dependencies.kt#L33
[local_build]: /contributing/testing-components-inside-app
[fenix]: https://github.com/mozilla-mobile/fenix
