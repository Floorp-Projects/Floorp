[android-components](../../index.md) / [mozilla.components.lib.publicsuffixlist](../index.md) / [PublicSuffixList](index.md) / [stripPublicSuffix](./strip-public-suffix.md)

# stripPublicSuffix

`fun stripPublicSuffix(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/publicsuffixlist/src/main/java/mozilla/components/lib/publicsuffixlist/PublicSuffixList.kt#L126)

Strips the public suffix from the given [domain](strip-public-suffix.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$stripPublicSuffix(kotlin.String)/domain). Returns the original domain if no public suffix could be
stripped.

E.g.:

```
wwww.mozilla.org -> www.mozilla
www.bcc.co.uk    -> www.bbc
a.b.ide.kyoto.jp -> a.b
```

### Parameters

`domain` - *must* be a valid domain. [PublicSuffixList](index.md) performs no validation, and if any unexpected values
are passed (e.g., a full URL, a domain with a trailing '/', etc) this may return an incorrect result.