[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [TabsAdapter](./index.md)

# TabsAdapter

`class TabsAdapter : Adapter<`[`TabViewHolder`](../-tab-view-holder/index.md)`>, `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/TabsAdapter.kt#L19)

RecyclerView adapter implementation to display a list/grid of tabs.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabsAdapter(delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md)`> = ObserverRegistry())`<br>RecyclerView adapter implementation to display a list/grid of tabs. |

### Functions

| Name | Summary |
|---|---|
| [displaySessions](display-sessions.md) | `fun displaySessions(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../../mozilla.components.browser.session/-session/index.md)`>, selectedIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the given list of sessions. |
| [getItemCount](get-item-count.md) | `fun getItemCount(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [onBindViewHolder](on-bind-view-holder.md) | `fun onBindViewHolder(holder: `[`TabViewHolder`](../-tab-view-holder/index.md)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCreateViewHolder](on-create-view-holder.md) | `fun onCreateViewHolder(parent: <ERROR CLASS>, viewType: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`TabViewHolder`](../-tab-view-holder/index.md) |
| [onSessionMoved](on-session-moved.md) | `fun onSessionMoved(fromPosition: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, toPosition: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateSessions() when a session changes it position. |
| [onSessionsChanged](on-sessions-changed.md) | `fun onSessionsChanged(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateSessions() when count number of sessions are updated at the given position. |
| [onSessionsInserted](on-sessions-inserted.md) | `fun onSessionsInserted(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateSessions() when count number of sessions are inserted at the given position. |
| [onSessionsRemoved](on-sessions-removed.md) | `fun onSessionsRemoved(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateSessions() when count number of sessions are removed from the given position. |
| [onViewRecycled](on-view-recycled.md) | `fun onViewRecycled(holder: `[`TabViewHolder`](../-tab-view-holder/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [unsubscribeHolders](unsubscribe-holders.md) | `fun unsubscribeHolders(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [updateSessions](update-sessions.md) | `fun updateSessions(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../../mozilla.components.browser.session/-session/index.md)`>, selectedIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the list of sessions. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../../mozilla.components.concept.tabstray/-tabs-tray/as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Convenience method to cast the implementation of this interface to an Android View object. |
