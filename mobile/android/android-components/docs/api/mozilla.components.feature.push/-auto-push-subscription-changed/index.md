[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushSubscriptionChanged](./index.md)

# AutoPushSubscriptionChanged

`data class AutoPushSubscriptionChanged` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L397)

The subscription from AutoPush that has changed on the remote push servers.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AutoPushSubscriptionChanged(scope: `[`PushScope`](../-push-scope.md)`, channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>The subscription from AutoPush that has changed on the remote push servers. |

### Properties

| Name | Summary |
|---|---|
| [channelId](channel-id.md) | `val channelId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [scope](scope.md) | `val scope: `[`PushScope`](../-push-scope.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
