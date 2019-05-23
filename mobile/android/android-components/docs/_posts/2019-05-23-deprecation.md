---
layout: post
title: "🔚 Deprecating feature-session-bundling, ui-doorhanger and ui-progress"
date: 2019-05-23 14:27:00 +0200
author: sebastian
---

In our upcoming 0.54.0 release we are going to deprecate three of our components: `feature-session-bundling`, `ui-doorhanger` and `ui-progress`. Primary classes of those components have been marked with the `@Deprecated` annotation. With 0.54.0 those components will still be released. However in a future release those components will be removed.

## feature-session-bundling

The `feature-session-bundling` provided the functionality for an early feature called "sessions" in the [Fenix project](https://github.com/mozilla-mobile/fenix). This feature has been replaced with "collections" and the functionality for this new feature is provided by the new `feature-tab-collections` component.

![](/assets/images/blog/session-bundles.png)

## ui-doorhanger

This component allowed apps to create "doorhangers" - floating heads-up popup that can be anchored to a view; like in Firefox for Android. This implementation was based on Android's [PopupWindow](https://developer.android.com/reference/android/widget/PopupWindow) class. The implementation caused multiple layout issues and component using it (like `feature-sitepermissions`) switched to using `DialogFragment`s instead.

## ui-progress

The `AnimatedProgressBar` was first introduced in Firefox for Android and later used in Firefox Focus and Firefox Lite. A recent performance measurement revealed that the animation of the progress bar can have a negative impact on page load performance. While there was no noticeable difference on the latest high-end devices, on older devices, like a Nexus 5, we saw pages load about ~400ms slower.

![](/assets/images/blog/progress-performance.png) 
