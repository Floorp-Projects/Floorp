[android-components](../../../index.md) / [mozilla.components.concept.engine.prompt](../../index.md) / [PromptRequest](../index.md) / [Authentication](./index.md)

# Authentication

`data class Authentication : `[`PromptRequest`](../index.md)`, `[`Dismissible`](../-dismissible/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/prompt/PromptRequest.kt#L197)

Value type that represents a request for an authentication prompt.
For more related info take a look at
&lt;a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication&gt;MDN docs

### Types

| Name | Summary |
|---|---|
| [Level](-level/index.md) | `enum class Level` |
| [Method](-method/index.md) | `enum class Method` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Authentication(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, userName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, password: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, method: `[`Method`](-method/index.md)`, level: `[`Level`](-level/index.md)`, onlyShowPassword: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, previousFailed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, isCrossOrigin: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, onConfirm: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Value type that represents a request for an authentication prompt. For more related info take a look at &lt;a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication&gt;MDN docs |

### Properties

| Name | Summary |
|---|---|
| [isCrossOrigin](is-cross-origin.md) | `val isCrossOrigin: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>indicates if this request is from a cross-origin sub-resource. |
| [level](level.md) | `val level: `[`Level`](-level/index.md)<br>indicates the level of security of the authentication like [Level.NONE](-level/-n-o-n-e.md), [Level.SECURED](-level/-s-e-c-u-r-e-d.md) and [Level.PASSWORD_ENCRYPTED](-level/-p-a-s-s-w-o-r-d_-e-n-c-r-y-p-t-e-d.md). |
| [message](message.md) | `val message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the body of the dialog. |
| [method](method.md) | `val method: `[`Method`](-method/index.md)<br>type of authentication,  valid values [Method.HOST](-method/-h-o-s-t.md) and [Method.PROXY](-method/-p-r-o-x-y.md). |
| [onConfirm](on-confirm.md) | `val onConfirm: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to indicate the user want to start the authentication flow. |
| [onDismiss](on-dismiss.md) | `val onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>callback to indicate the user dismissed this request. |
| [onlyShowPassword](only-show-password.md) | `val onlyShowPassword: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>indicates if the dialog should only include a password field. |
| [password](password.md) | `val password: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>default value provide for this session. |
| [previousFailed](previous-failed.md) | `val previousFailed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>indicates if this request is the result of a previous failed attempt to login. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>of the dialog. |
| [userName](user-name.md) | `val userName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>default value provide for this session. |
