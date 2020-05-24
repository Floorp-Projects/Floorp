[android-components](../../index.md) / [mozilla.components.concept.fetch](../index.md) / [Request](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`Request(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, method: `[`Method`](-method/index.md)` = Method.GET, headers: `[`MutableHeaders`](../-mutable-headers/index.md)`? = MutableHeaders(), connectTimeout: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)`>? = null, readTimeout: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)`>? = null, body: `[`Body`](-body/index.md)`? = null, redirect: `[`Redirect`](-redirect/index.md)` = Redirect.FOLLOW, cookiePolicy: `[`CookiePolicy`](-cookie-policy/index.md)` = CookiePolicy.INCLUDE, useCaches: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`

The [Request](index.md) data class represents a resource request to be send by a [Client](../-client/index.md).

It's API is inspired by the Request interface of the Web Fetch API:
https://developer.mozilla.org/en-US/docs/Web/API/Request

