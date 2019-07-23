[android-components](../index.md) / [mozilla.components.browser.session.intent](index.md) / [getSessionId](./get-session-id.md)

# getSessionId

`fun <ERROR CLASS>.getSessionId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/intent/IntentExtensions.kt#L18)
`fun `[`SafeIntent`](../mozilla.components.support.utils/-safe-intent/index.md)`.getSessionId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/intent/IntentExtensions.kt#L26)

Retrieves [mozilla.components.browser.session.Session](../mozilla.components.browser.session/-session/index.md) ID from the intent.

**Return**
The session ID previously added with [putSessionId](put-session-id.md),
or null if no ID was found.

