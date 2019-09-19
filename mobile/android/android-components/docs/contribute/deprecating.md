---
layout: page
title: Deprecating components and code
permalink: /contributing/deprecating
---

The *Android Components* team follows a [rapid release process with major releases coming out frequently]({{ site.baseurl }}/contributing/versioning). Therefore API breakage can happen more often when switching to a new version. The *Android Components* team tries to reduce API breakage and uses the following deprecation process to give app teams notice of upcoming changes as early as possible. Deprecated APIs are removed not earlier than 2 releases after the release that introduced the deprecation warning.

## Avoiding API breakage

Before breaking or deprecating an API consider options to provide new functionality without breaking the (old) API.

Examples:
* When adding a new parameter to a method consider adding a default value so that defining a value for this parameter is not required and existing code using the method still compiles.
* When introducing new constructor parameters consider adding an additional constructor and keep/deprecate the existing constructor.

In general if possible we try to keep "compile compatibility" when an app project upgrades to a new *Android Components* version.

## When to Deprecate

Deprecating a public API is needed whenever we introduce a new API and at the same time keep the old API around so that app teams have time to migrate. The goal of deperecating an API should always be to eventually remove it.

No API guarantees are made for snapshot releases and so far unreleased/unused components. Unreleased APIs can break/change before they are released or used.

## How to Deprecate

* Add the **[@Deprecated](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-deprecated/index.html)** annotation to the deprecated API. Try to use the `message` parameter of the annotation to explain the deprecation. If possible use the `replaceWith` parameter to specify a code fragment which should be used as a replacement for the deprecated API usage.

  ```Kotlin
    @Deprecated("Use `loadAsync` instead", ReplaceWith("loadAsync(context)"))
    suspend fun load(context: Context): Deferred<SearchEngineList> {
        // ...
    }
  ```

* Mention the deprecated API and the new API (if there's one) in the **changelog**.
* **File a new issue** about removing the deprecated API. Mention the target version for the removal in the issue title. The default time for removal is 2 releases after the release that introduced the deprecation warning. For exmaple when deprecating an API in release 12.0.0 then file an issue to remove the API in release 14.0.0. For larger changes that require a major refactoring in app projects (e.g. migrating to a whole new component) a longer deprecation cycle can be chosen. The [cross-repo search engine "SearchFox"](https://searchfox.org/mozilla-mobile/source) is a helpful tool to find API usage in other Mozilla mobile projects.
* For larger API changes and breakage consider **sending a mail to the [android-components mailing list](https://lists.mozilla.org/listinfo/android-components)** to notify all consumers of the upcoming change. If a specific team at Mozilla will be required to do a large refactoring to accomodate the API change then try to coordinate the deprecation process and timing with that team.