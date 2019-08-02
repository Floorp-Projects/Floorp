[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Request](index.md) / [connectTimeout](./connect-timeout.md)

# connectTimeout

`val connectTimeout: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Request.kt#L44)

A timeout to be used when connecting to the resource.  If the timeout expires before the
connection can be established, a [java.net.SocketTimeoutException](https://developer.android.com/reference/java/net/SocketTimeoutException.html) is raised. A timeout of zero is interpreted as an
infinite timeout.

### Property

`connectTimeout` - A timeout to be used when connecting to the resource.  If the timeout expires before the
connection can be established, a [java.net.SocketTimeoutException](https://developer.android.com/reference/java/net/SocketTimeoutException.html) is raised. A timeout of zero is interpreted as an
infinite timeout.