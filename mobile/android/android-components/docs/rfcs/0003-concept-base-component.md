---
layout: page
title: Adding a `concept-base` component
permalink: /rfc/0003-concept-base-component
---

* Start date: 2020-07-20
* RFC PR: [#7776](https://github.com/mozilla-mobile/android-components/pull/7776)

## Summary

Adding a `concept-base` component for basic interfaces needed by multiple components.

## Motivation

We already have a `support-base` component that contains basic building blocks, like a logger, that other components may need. Some of those building blocks are interfaces, like `CrashReporting` that are implemented by other components (e.g. `lib-crash`). This works well in most cases, but becomes problematic once a `concept` component requires such an interface. Having a `concept` component depend on actual code with `support-base` is breaking our contract of concepts only depending on other concepts.

* In [#7689](https://github.com/mozilla-mobile/android-components/issues/7689) I want to introduce a `Profiler` interface that allows other components to add profiler markers. The Firefox profiler provides this functionality in our `browser-engine-gecko*` components (exposed by `concept-engine`). If this interface lives in `support-base` then `concept-engine` would need to depend on `support-base`.
* In [#7775](https://github.com/mozilla-mobile/android-components/issues/7775) I want to introduce an interface that allows components to make leaks detectable.

## Reference-level explanation

We introduce a `concept-base` component that contains those "basic" interfaces. Other components and concepts can depend on this component. This component will be the home for the `Profiler` interface ([#7689](https://github.com/mozilla-mobile/android-components/issues/7689)) and leak detection interface ([#7775](https://github.com/mozilla-mobile/android-components/issues/7775)).

In addition to that we can move interface-only pieces from `support-base` to `concept-base`: `CrashReporting`, `MemoryConsumer`.

## Drawbacks

* This introduces another base component that most other components will depend on. Since it only contains interfaces the impact of that will be low though.

## Rationale and alternatives

* Alternatively we could create distinct `concept` components for every interface theme. This would end up with a very fine-grained list of components that mostly may contain only a single interface. This would also break with the idea that a concept describes a component that will have an actual implementation component (e.g. `concept-toolbar` -> `browser-toolbar`). Interfaces like `MemoryConsumer` are not describing a full component, but instead just one unified aspect of it.
