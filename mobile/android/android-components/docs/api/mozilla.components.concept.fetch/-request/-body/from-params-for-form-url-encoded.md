[android-components](../../../index.md) / [mozilla.components.concept.fetch](../../index.md) / [Request](../index.md) / [Body](index.md) / [fromParamsForFormUrlEncoded](./from-params-for-form-url-encoded.md)

# fromParamsForFormUrlEncoded

`fun fromParamsForFormUrlEncoded(vararg unencodedParams: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Body`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/fetch/src/main/java/mozilla/components/concept/fetch/Request.kt#L78)

Create a [Body](index.md) from the provided [unencodedParams](from-params-for-form-url-encoded.md#mozilla.components.concept.fetch.Request.Body.Companion$fromParamsForFormUrlEncoded(kotlin.Array((kotlin.Pair((kotlin.String, )))))/unencodedParams) in the format of Content-Type
"application/x-www-form-urlencoded". Parameters are formatted as "key1=value1&key2=value2..."
and values are percent-encoded. If the given map is empty, the response body will contain the
empty string.

**See Also**

[Headers.Values.CONTENT_TYPE_FORM_URLENCODED](../../-headers/-values/-c-o-n-t-e-n-t_-t-y-p-e_-f-o-r-m_-u-r-l-e-n-c-o-d-e-d.md)

