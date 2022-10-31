---
layout: post
title: "✨ browser-state: Better state management for apps and components"
date: 2019-09-02 10:30:00 +0200
author: sebastian
---

We have been working on a new component called `browser-state` to eventually replace `browser-session`. Now we are ready to start migrating components from `browser-session` to `browser-state`. This blog posting explains why we want to decommission `browser-session`, describes how `browser-state` works and what our migration plans are.

## What's the problem with `browser-session`?

For maintaining the global browser state (e.g. "What tabs are open? What URLs are they pointing to?") the *Android Components* project provides the `browser-session` component. The initial implementation of `browser-session` was a clean, generic re-implementation of what we had developed (more organically) for [Firefox Focus](https://github.com/mozilla-mobile/focus-android).

In 2018 we [noticed some flaws](https://github.com/mozilla-mobile/android-components/issues/400) in the design of `browser-session`. Those flaws came down to being able to observe the state while being able to modify it at the same time ("mutable state"). This unintended behavior could lead to "event order issues" and observers not really seeing a particular state change. Luckily back then we hadn't seen those issues causing any problems in our apps yet.

We looked at [multiple ways to prevent those side effects](https://github.com/mozilla-mobile/android-components/pull/453) but that turned out to be almost impossible as long as the state is mutable. After more brainstorming, researching and prototyping we came up with a new design for a completely new component called `browser-state` to eventually replace `browser-session`.

In 2019 we completed and tweaked the design of the new `browser-state` component until we felt that it was ready to be used in other components.

## A closer look at `browser-state`

Concepts used in the `browser-state` component are similar to [Redux](https://redux.js.org/) - a state container library for JavaScript. The Redux documentation is a great way to get familiar with some of the concepts:
 * [Core Concepts](https://redux.js.org/introduction/core-concepts)
 * [Three Principles](https://redux.js.org/introduction/three-principles)

### BrowserState

The global state of the browser is represented by an instance of an immutable data class: `BrowserState` ([API](https://mozac.org/api/mozilla.components.browser.state.state/-browser-state/)). Since it is immutable, an instance of this data class can be observed and processed without any side effects changing it. A state change is represented by the creation of a new `BrowserState` instance.

### BrowserStore

The `BrowserStore` ([API](https://mozac.org/api/mozilla.components.browser.state.store/-browser-store/)) is the single source of truth. It holds the current `BrowserState` instance and components, as well as app code, can observe it in order to always receive the latest `BrowserState`. The only way to change the state is by dispatching a `BrowserAction` ([API](https://mozac.org/api/mozilla.components.browser.state.action/-browser-action.html)) on the store. A dispatched `BrowserAction` will be processed internally and a new `BrowserState` object will be emitted by the store.

## How are we going to migrate apps and components to `browser-state`?

The `browser-session` component is at the heart of many components and most apps using our components. It is obvious that we cannot migrate all components and apps from `browser-session` to `browser-state` from one *Android Components*  release to the next one. Therefore the *Android Components* team made it possible to use `browser-state` and `browser-session` simultaneously and keep the state in both components synchronized.

```kotlin
val store = BrowserStore()

// Passing the BrowserStore instance to SessionManager makes sure that both
// components will be kept in sync.
val sessionManager = SessionManager(engine, store)
```

With the ability to use both components simultaneously, the *Android Components* team will start migrating components over from `browser-session` to `browser-state`. As part of this work the *Android Components* team will extend and add to `BrowserState` to eventually reach feature parity with the state in `SessionManager` and `Session`. The only thing that may be different for app teams is that some components may require a `BrowserStore` instance instead of a `SessionManager` instance after migration.

```kotlin
// Before the migration
feature = ToolbarFeature(
    layout.toolbar,
    components.sessionManager,
    components.sessionUseCases.loadUrl,
    components.defaultSearchUseCase)

// After the migration
feature = ToolbarFeature(
    layout.toolbar,
    components.store,
    components.sessionUseCases.loadUrl,
    components.defaultSearchUseCase)
```

Once the migration of components is largely done, the *Android Components* team will start to help the app teams to plan migrating app code from `browser-session` to `browser-state`.