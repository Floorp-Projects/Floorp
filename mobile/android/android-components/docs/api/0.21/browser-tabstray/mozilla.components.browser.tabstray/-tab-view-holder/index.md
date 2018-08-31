---
title: TabViewHolder - 
---

[mozilla.components.browser.tabstray](../index.html) / [TabViewHolder](./index.html)

# TabViewHolder

`class TabViewHolder : ViewHolder, Observer`

A RecyclerView ViewHolder implementation for "tab" items.

### Constructors

| [&lt;init&gt;](-init-.html) | `TabViewHolder(itemView: View)`<br>A RecyclerView ViewHolder implementation for "tab" items. |

### Functions

| [bind](bind.html) | `fun bind(session: Session, isSelected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, observable: Observable<Observer>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the data of the given session and notifies the given observable about events. |
| [onUrlChanged](on-url-changed.html) | `fun onUrlChanged(session: Session, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [unbind](unbind.html) | `fun unbind(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The attached view no longer needs to display any data. |

