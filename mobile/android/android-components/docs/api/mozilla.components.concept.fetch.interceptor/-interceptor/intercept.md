[android-components](../../index.md) / [mozilla.components.concept.fetch.interceptor](../index.md) / [Interceptor](index.md) / [intercept](./intercept.md)

# intercept

`abstract fun intercept(chain: `[`Chain`](-chain/index.md)`): `[`Response`](../../mozilla.components.concept.fetch/-response/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/interceptor/Interceptor.kt#L30)

Allows an [Interceptor](index.md) to intercept a request and modify request or response.

An interceptor can retrieve the request by calling [Chain.request](-chain/request.md).

If the interceptor wants to continue executing the chain (which will execute potentially other interceptors and
may eventually perform the request) it can call [Chain.proceed](-chain/proceed.md) and pass along the original or a modified
request.

Finally the interceptor needs to return a [Response](../../mozilla.components.concept.fetch/-response/index.md). This can either be the [Response](../../mozilla.components.concept.fetch/-response/index.md) from calling
[Chain.proceed](-chain/proceed.md) - modified or unmodified - or a [Response](../../mozilla.components.concept.fetch/-response/index.md) the interceptor created manually or obtained from
a different source.

