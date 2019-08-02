[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Request](index.md) / [readTimeout](./read-timeout.md)

# readTimeout

`val readTimeout: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)`>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Request.kt#L45)

A timeout to be used when reading from the resource. If the timeout expires before there is
data available for read, a java.net.SocketTimeoutException is raised. A timeout of zero is interpreted as an infinite
timeout.

### Property

`readTimeout` - A timeout to be used when reading from the resource. If the timeout expires before there is
data available for read, a java.net.SocketTimeoutException is raised. A timeout of zero is interpreted as an infinite
timeout.