[android-components](../../index.md) / [mozilla.components.support.base.facts](../index.md) / [Action](./index.md)

# Action

`enum class Action` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/facts/Action.kt#L10)

A user or system action that causes [Fact](../-fact/index.md) instances to be emitted.

### Enum Values

| Name | Summary |
|---|---|
| [CLICK](-c-l-i-c-k.md) | The user has clicked on something. |
| [TOGGLE](-t-o-g-g-l-e.md) | The user has toggled something. |
| [COMMIT](-c-o-m-m-i-t.md) | The user has committed an input (e.g. entered text into an input field and then pressed enter). |
| [PLAY](-p-l-a-y.md) | The user has started playing something. |
| [PAUSE](-p-a-u-s-e.md) | The user has paused something. |
| [STOP](-s-t-o-p.md) | The user has stopped something. |
| [INTERACTION](-i-n-t-e-r-a-c-t-i-o-n.md) | A generic interaction that can be caused by a previous action (e.g. the user clicks on a button which causes a [Fact](../-fact/index.md) with [CLICK](-c-l-i-c-k.md) action to be emitted. This click may causes something to load which emits a follow-up a [Fact](../-fact/index.md) with [INTERACTION](-i-n-t-e-r-a-c-t-i-o-n.md) action. |
