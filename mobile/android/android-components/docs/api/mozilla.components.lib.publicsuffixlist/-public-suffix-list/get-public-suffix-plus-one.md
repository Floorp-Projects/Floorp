[android-components](../../index.md) / [mozilla.components.lib.publicsuffixlist](../index.md) / [PublicSuffixList](index.md) / [getPublicSuffixPlusOne](./get-public-suffix-plus-one.md)

# getPublicSuffixPlusOne

`fun getPublicSuffixPlusOne(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/publicsuffixlist/src/main/java/mozilla/components/lib/publicsuffixlist/PublicSuffixList.kt#L78)

Returns the public suffix and one more level; known as the registrable domain. Returns `null` if
[domain](get-public-suffix-plus-one.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$getPublicSuffixPlusOne(kotlin.String)/domain) is a public suffix itself.

E.g.:

```
wwww.mozilla.org -> mozilla.org
www.bcc.co.uk    -> bbc.co.uk
a.b.ide.kyoto.jp -> b.ide.kyoto.jp
```

### Parameters

`domain` - *must* be a valid domain. [PublicSuffixList](index.md) performs no validation, and if any unexpected values
are passed (e.g., a full URL, a domain with a trailing '/', etc) this may return an incorrect result.