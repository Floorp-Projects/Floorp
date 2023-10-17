---
layout: page
title: Add State-based navigation
permalink: /rfc/0010-add-state-based-navigation
---

* Start date: 2023-10-17
* RFC PR: [4126](https://github.com/mozilla-mobile/firefox-android/pull/4126)

## Summary

Following the acceptance of [0009 - Remove Interactors and Controllers](0009-remove-interactors-and-controllers), Fenix should have a method of navigation that is tied to the `lib-state` model to  provide a method of handling navigation side-effects that is consistent with architectural goals.

## Motivation

Currently, methods of navigation throughout the app are varied. The `SessionControlController` provides [3 examples](https://searchfox.org/mozilla-mobile/rev/aa6bee71a6e0ea73f041a54ddf4d5d4e2f603429/firefox-android/fenix/app/src/main/java/org/mozilla/fenix/home/sessioncontrol/SessionControlController.kt#180) alone:

- `HomeActivity::openToBrowserAndLoad`
- Calling a `NavController` directly
- Callbacks like `showTabTray()`

To move to a more consistent `Store` model, we need a way for features to fire `Action`s and have that result in navigation. This has the added benefit of decoupling our business logic from Android platform implementation details.

## Proposal

There are two cases to consider for navigation state: the currently displayed fragment and transient UI like CFRs, dialogs, etc.

For screens: add a `Screen` property to `AppStore` and react to changes by observing them in a navigation `AbstractBinding` hosted by the `HomeActivity`. This is roughly equivalent to the `Navigator` [implementation in Focus](https://searchfox.org/mozilla-mobile/rev/aa6bee71a6e0ea73f041a54ddf4d5d4e2f603429/firefox-android/focus-android/app/src/main/java/org/mozilla/focus/navigation/Navigator.kt#22). 

Handling this State in AppStore should allow us to have a consistent touch point for all navigation since this Store is globally accessible. 

For transient state: For now, I am proposing to implement feature- or screen-specific middleware that consume actions to directly tie them to their side-effects.

## Alternatives

### For screens:
One big alternative consideration is: do we want to track nav State in the Store at all? It is potentially error-prone or repetitious of other code (nav-graph and the code that actually invokes the NavController). The main alternative would be to handle navigation in middleware as a reaction to `Action`s, but options for this come with there own set of challenges:

1. Global navigation middleware attached to the AppStore. This is made more difficult because we do not have access to an `Activity` context (to retrieve a `NavController` from) when instantiating the `AppStore` in `Component`s. We could create a mutable getter/setter for the Middleware's NavController, but this carries risks for things like race conditions, public mutability, and null accesses if the Middleware held a dangling reference to a null activity after destructive lifecycle events.
2. Individual navigation middleware attached to each Fragment's Store. This carries risk of repetition and boilerplate.

### For transient UI state:
1. Focus handles some cases of this similarly to screens. There are examples in of CFRs in [Focus' AppState](https://searchfox.org/mozilla-mobile/rev/ebe8346d6074c72af319e3f47d9dec49de381533/firefox-android/focus-android/app/src/main/java/org/mozilla/focus/state/AppState.kt#33) for example. This is potentially more in line with the proposal to add a `Screen` property, but it carries its own risks:
	1. State must be consumed correctly, or configuration changes can cause effects like dialogs or CFRs to erroneously re-display.
	2. It suffers from the main drawback of the `Screen` proposal, in that we introduce additional code to track what should be treated as a side-effect (and therefore handled by middleware).

