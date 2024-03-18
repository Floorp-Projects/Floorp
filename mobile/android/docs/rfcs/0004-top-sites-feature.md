---
layout: page
title: Introduce a Top Sites Feature
permalink: /rfc/0004-top-sites-feature
---

* Start date: 2020-07-30
* RFC PR: [#7931](https://github.com/mozilla-mobile/android-components/pull/7931)

## Summary

Create a TopSitesFeature that abstracts out the co-ordination between the multiple data storages.

## Motivation

A large part of the top sites implementation is spread across A-C and Fenix. There is a lot of co-ordination between the History API and the current Top Sites API to manage the final state of what sites should be displayed that can exist as a single implementation.

## Guide-level explanation

To begin, we should rename the `TopSitesStorage`  to `PinnedSitesStorage` and create a `TopSitesFeature` that would orchestrate the different storages and present them to the app.

We can then follow the architecture of existing features by using the presenter/interactor/view model, and introduce a `TopSitesStorage` which abstract out the complexity of the multiple data sources from `PinnedSitesStorage` and `PlacesHistoryStorage`.

To allow the ability for adding a top site from different parts of the app (e.g. context menus), we introduce the `PinnedSitesUseCases` that acts on the storage directly. If the `TopSitesFeature` is started, it will then be notified by the `Observer` to update the UI.

```kotlin

/**
 * Implemented by the application for displaying onto the UI.
 */
interface TopSitesView {
  /**
   * Update the UI.
   */
  fun displaySites(sites: List<TopSite>)

  interface Listener {
    /**
     * Invoked by the UI for us to add to our storage.
     */
    fun onAddTopSite(topSite: TopSite)

    /**
     * Invoked by the UI for us to remove from our storage.
     */
    fun onRemoveTopSite(topSite: TopSite)
  }
}

/**
 * Abstraction layer above the multiple storages.
 */
interface TopSitesStorage {

  /**
   * Return a unified list of top sites based on the given number of sites desired.
   * If `includeFrecent` is true, fill in any missing top sites with frecent top site results.
   */
  suspend fun getTopSites(totalNumberOfSites: Int, includeFrecent: Boolean): List<TopSite>

  interface Observer {
    /**
     * Invoked when changes are made to the storage.
     */
    fun onStorageUpdated()
  }
}

// Already exists in browser-storage-sync.
interface PlacesHistoryStorage {
  fun getTopFrecentSites(num: Int)
}

class DefaultTopSitesStorage(
  val pinnedSitesStorage: PinnedSitesStorage,
  val historyStorage: PlacesHistoryStorage
) : TopSitesStorage {

  /**
   * Merge data sources here, return a single list of top sites.
   */
  override suspend fun getTopSites(totalNumberOfSites: Int, includeFrecent: Boolean): List<TopSite>
}

/**
 * Use cases can be used for adding a pinned site from different places like a context menu.
 */
class PinnedSitesUseCases(pinnedSitesStorage: PinnedSitesStorage) {
  val addPinnedSites: AddPinnedSiteUseCase
  val removePinnedSites: RemovePinnedSiteUseCase
}

/**
 * View-bound feature that updates the UI when changes are made.
 */
class TopSitesFeature(
  val storage: TopSitesStorage,
  val presenter: TopSitesPresenter,
  val view: TopSitesView,
  val defaultSites: () -> List<TopSites>
) : LifecycleAwareFeature {
  override fun start()
  override fun stop()
}
```

## Drawbacks

* `TopSitesStorage` always pulls sites from a persistent storage (Room). This could cause an excessive amount of reads from disk - this is drawback that exists in today's implementation as well. Places has an in-memory cache that should not be affected by this.

## Rationale and alternatives

- This will remove our dependency on `LiveData` in Fenix by updating the `TopSitesView` when we have the top sites available to display; see [#7459](https://github.com/mozilla-mobile/android-components/issues/7459).
- We can reduce the multiple implementations of the TopSitesFeature by introducing it into Android Components.

## Prior art

* Fenix abstraction of Top Sites: [TopSiteStorage.kt](https://github.com/mozilla-mobile/fenix/blob/3d3153039ce794df09a243968b888ae7cb856d9b/app/src/main/java/org/mozilla/fenix/components/TopSiteStorage.kt)
* The RFC structure can be see in other components such as, [FindInPageFeature](https://github.com/mozilla-mobile/android-components/blob/main/components/feature/findinpage/src/main/java/mozilla/components/feature/findinpage/FindInPageFeature.kt)  & [SyncedTabsFeature](https://github.com/mozilla-mobile/android-components/blob/main/components/feature/syncedtabs/src/main/java/mozilla/components/feature/syncedtabs/SyncedTabsFeature.kt).

## Unresolved questions

* When the user removes a top site from the UI that comes from `TopSitesStorage`, we remove it from the storage. When a top site comes from frecent, what do we do? Should we be removing the frecent result?



