[android-components](../../../index.md) / [mozilla.components.browser.session](../../index.md) / [SessionManager](../index.md) / [Snapshot](./index.md)

# Snapshot

`data class Snapshot` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L316)

### Types

| Name | Summary |
|---|---|
| [Item](-item/index.md) | `data class Item` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Snapshot(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Item`](-item/index.md)`>, selectedSessionIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)` |

### Properties

| Name | Summary |
|---|---|
| [selectedSessionIndex](selected-session-index.md) | `val selectedSessionIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [sessions](sessions.md) | `val sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Item`](-item/index.md)`>` |

### Functions

| Name | Summary |
|---|---|
| [isEmpty](is-empty.md) | `fun isEmpty(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [empty](empty.md) | `fun empty(): `[`Snapshot`](./index.md) |
| [singleItem](single-item.md) | `fun singleItem(item: `[`Item`](-item/index.md)`): `[`Snapshot`](./index.md) |
