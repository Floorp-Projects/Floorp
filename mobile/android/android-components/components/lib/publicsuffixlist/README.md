# [Android Components](../../../README.md) > Libraries > Public Suffix List

A library for reading and using the Public Suffix List.

> A "public suffix" is one under which Internet users can (or historically could) directly register names. Some examples of public suffixes are .com, .co.uk and pvt.k12.ma.us. The Public Suffix List is a list of all known public suffixes. 
> [https://publicsuffix.org/](https://publicsuffix.org/)

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:lib-publicsuffixlist:{latest-version}"
```

### Using the public suffix list

The `PublicSuffixList` class offers multiple methods for using the public suffix list data. For every instance the list needs to be read from disk into memory once. Therefore all methods return [Deferred](https://kotlin.github.io/kotlinx.coroutines/kotlinx-coroutines-core/kotlinx.coroutines/-deferred/) types.  The list data is cached in the `PublicSuffixList` and therefore it is recommended to keep a single instance in memory when frequently accessing the list. The list data can be prefetched to guarantee fast access for subsequent access.

```Kotlin
val publicSuffixList = PublicSuffixList(context)

// Not needed, but allows a consumer to decide when the read is happening:
publicSuffixList.prefetch()
 // Optionally you can wait for the read to complete:
publicSuffixList.prefetch().await()
```

```Kotlin
// Extracting the effective top-level domain (eTLD)
publicSuffixList.getPublicSuffixPlusOne("www.mozilla.org") // -> mozilla.org
publicSuffixList.getPublicSuffixPlusOne("www.bbc.co.uk") // -> bbc.co.uk
publicSuffixList.getPublicSuffixPlusOne("a.b.ide.kyoto.jp") // -> b.ide.kyoto.jp
```

```Kotlin
// Checking whether a value is a public suffix:
publicSuffixList.isPublicSuffix("org") // -> true
publicSuffixList.isPublicSuffix("co.uk") // -> true
publicSuffixList.isPublicSuffix("org") // -> true
publicSuffixList.isPublicSuffix("ide.kyoto.jp") --> true
```

```Kotlin
// Extracting the public suffix from a domain
publicSuffixList.getPublicSuffix("www.mozilla.org") // -> org
publicSuffixList.getPublicSuffix("www.bbc.co.uk") // -> co.uk
publicSuffixList.getPublicSuffix("a.b.ide.kyoto.jp") // -> ide.kyoto.jp
```

```Kotlin
// Removing the public suffix from a domain
publicSuffixList.stripPublicSuffix("www.mozilla.org") // -> www.mozilla
publicSuffixList.stripPublicSuffix("foobar.blogspot.com") // -> foobar
publicSuffixList.stripPublicSuffix("www.example.pvt.k12.ma.us") // -> www.example
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
