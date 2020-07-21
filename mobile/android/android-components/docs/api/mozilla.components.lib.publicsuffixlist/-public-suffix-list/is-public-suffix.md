[android-components](../../index.md) / [mozilla.components.lib.publicsuffixlist](../index.md) / [PublicSuffixList](index.md) / [isPublicSuffix](./is-public-suffix.md)

# isPublicSuffix

`fun isPublicSuffix(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/publicsuffixlist/src/main/java/mozilla/components/lib/publicsuffixlist/PublicSuffixList.kt#L57)

Returns true if the given [domain](is-public-suffix.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$isPublicSuffix(kotlin.String)/domain) is a public suffix; false otherwise.

E.g.:

```
co.uk       -> true
com         -> true
mozilla.org -> false
org         -> true
```

Note that this method ignores the default "prevailing rule" described in the formal public suffix list algorithm:
If no rule matches then the passed [domain](is-public-suffix.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$isPublicSuffix(kotlin.String)/domain) is assumed to *not* be a public suffix.

### Parameters

`domain` - *must* be a valid domain. [PublicSuffixList](index.md) performs no validation, and if any unexpected values
are passed (e.g., a full URL, a domain with a trailing '/', etc) this may return an incorrect result.