[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [AbstractWebAppShellActivity](./index.md)

# AbstractWebAppShellActivity

`abstract class AbstractWebAppShellActivity : AppCompatActivity, CoroutineScope` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/AbstractWebAppShellActivity.kt#L28)

Activity for "standalone" and "fullscreen" web applications.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AbstractWebAppShellActivity()`<br>Activity for "standalone" and "fullscreen" web applications. |

### Properties

| Name | Summary |
|---|---|
| [engine](engine.md) | `abstract val engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md) |
| [engineView](engine-view.md) | `lateinit var engineView: `[`EngineView`](../../mozilla.components.concept.engine/-engine-view/index.md) |
| [session](session.md) | `lateinit var session: `[`Session`](../../mozilla.components.browser.session/-session/index.md) |
| [sessionManager](session-manager.md) | `abstract val sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md) |

### Functions

| Name | Summary |
|---|---|
| [onCreate](on-create.md) | `open fun onCreate(savedInstanceState: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDestroy](on-destroy.md) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [INTENT_ACTION](-i-n-t-e-n-t_-a-c-t-i-o-n.md) | `const val INTENT_ACTION: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [launchGeckoResult](../../mozilla.components.browser.engine.gecko/kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md) | `fun <T> CoroutineScope.launchGeckoResult(context: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = EmptyCoroutineContext, start: CoroutineStart = CoroutineStart.DEFAULT, block: suspend CoroutineScope.() -> `[`T`](../../mozilla.components.browser.engine.gecko/kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md#T)`): `[`GeckoResult`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoResult.html)`<`[`T`](../../mozilla.components.browser.engine.gecko/kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md#T)`>`<br>Create a GeckoResult from a co-routine. |
