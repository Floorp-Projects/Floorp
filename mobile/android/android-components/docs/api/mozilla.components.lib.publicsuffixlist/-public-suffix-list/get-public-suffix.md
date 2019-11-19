[android-components](../../index.md) / [mozilla.components.lib.publicsuffixlist](../index.md) / [PublicSuffixList](index.md) / [getPublicSuffix](./get-public-suffix.md)

# getPublicSuffix

`fun getPublicSuffix(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/publicsuffixlist/src/main/java/mozilla/components/lib/publicsuffixlist/PublicSuffixList.kt#L102)

Returns the public suffix of the given [domain](get-public-suffix.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$getPublicSuffix(kotlin.String)/domain); known as the effective top-level domain (eTLD). Returns `null`
if the [domain](get-public-suffix.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$getPublicSuffix(kotlin.String)/domain) is a public suffix itself.

E.g.:

```
wwww.mozilla.org -> org
www.bcc.co.uk    -> co.uk
a.b.ide.kyoto.jp -> ide.kyoto.jp
```

### Parameters

`domain` - *must* be a valid domain. [PublicSuffixList](index.md) performs no validation, and if any unexpected values
are passed (e.g., a full URL, a domain with a trailing '/', etc) this may return an incorrect result.