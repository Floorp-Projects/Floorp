# [Android Components](../../../README.md) > Browser > Domains

This component provides APIs for managing localized and customizable domain lists (see [Domains](#domains) and [CustomDomains](#customdomains)). It also contains auto-complete functionality for these lists (see [DomainAutoCompleteProvider](#domainautocompleteprovider)) which can be used in conjuction with our [UI autocomplete component](../../ui/autocomplete/README.md).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-domains:{latest-version}"
```

### Domains

The `Domains` object is used to load the built-in localized domain lists which are shipped as part of this component. These lists are grouped by country and can be found [in our repository](src/main/assets/domains).

```Kotlin
// Load the domain lists for all countries in the default locale (fallback is US)
val domains = Domains.load(context)
```

### CustomDomains

The `CustomDomains` object can be used to manage a custom domain list which will be stored in `SharedPreferences`.

```Kotlin
// Load the custom domain list
val domains = CustomDomains.load(context)

// Save custom domains
CustomDomains.save(context, listOf("mozilla.org", "getpocket.com"))

// Remove custom domains
CustomDomains.remove(context, listOf("nolongerexists.org"))
```

### DomainAutoCompleteProvider

The class provides auto-complete functionality for both `Domains` and `CustomDomains`.

```Kotlin
// Initialize the provider
val provider = DomainAutocompleteProvider()
provider.initialize(
  context,
  useShippedDomains = true,
  useCustomDomains = true,
  loadDomainsFromDisk = true
)
```

Note that when `loadDomainsFromDisk` is set to true there is no need to manually call `load` on either `Domains` or `CustomDomains`.

```Kotlin
// Autocomplete domain lists
val result = provider.autocomplete("moz")
```

The result will contain the autocompleted text (`result.text`), the URL (`result.url`), and the source of the match (`result.source`), which is either `DEFAULT_LIST` if a result was found in the shipped domain list or `CUSTOM_LIST` otherwise. The custom domain list takes precendece over the built-in shipped domain list and the API will only return the first match.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
