# Contributing

The following content goes more in-depth than the [Overview](./01_overview.md) section
to provide helpful information regarding contributing to Firefox Translations.

- [Source Code](#source-code)
- [Architecture](#architecture)
    - [JSActors](#jsactors)
- [Remote Settings](#remote-settings)
    - [Admin Dashboards](#admin-dashboards)
    - [Prod Admin Access](#prod-admin-access)
    - [Pulling From Different Sources](#pulling-from-different-sources)
    - [Versioning](#versioning)
      - [Non-Breaking Changes](#non-breaking-changes)
      - [Breaking Changes](#breaking-changes)
- [Language Identification](#language-identification)

---
## Source Code

The primary source code for Firefox Translations lives in the following directory:

> **[toolkit/components/translations]**

---
## Architecture

### JSActors

Translations functionality is divided into different classes based on which access privileges are needed.
Generally, this split is between [Parent] and [Child] versions of [JSWindowActors].

The [Parent] actors have access to privileged content and is responsible for things like downloading models
from [Remote Settings](#remote-settings), modifying privileged UI components etc.

The [Child] actors are responsible for interacting with content on the page itself, requesting content from
the [Parent] actors, and creating [Workers] to carry out tasks.

---
## Remote Settings

The machine-learning models and [WASM] binaries are all hosted in Remote Settings and are downloaded/cached when needed.

### Admin Dashboards

In order to get access to Firefox Translations content in the Remote Settings admin dashboards, you will need to request
access in the Remote Settings component on [Bugzilla].

Once you have access to Firefox Translations content in Remote Settings, you will be able to view it in the admin dashboards:

**Dev**<br>
> [https://settings.dev.mozaws.net/v0/admin](https://settings.dev.mozaws.net/v1/admin)

**Stage**<br>
> [https://remote-settings.allizom.org/v0/admin](https://settings-writer.stage.mozaws.net/v1/admin)

### Prod Admin Access

In order to access the prod admin dashboard, you must also have access to a VPN that is authorized to view the dashboard.
To gain access to the VPN, follow [Step 3] on this page in the Remote Settings documentation.

**Prod**<br>
> [https://remote-settings.mozilla.org/v1/admin](https://settings-writer.prod.mozaws.net/v1/admin)


### Pulling From Different Sources

When you are running Firefox, you can choose to pull data from **Dev**, **Stage**, or **Prod** by downloading and installing
the latest [remote-settings-devtools] Firefox extension.

### Versioning

Firefox Translations uses semantic versioning for all of its records via the **`version`** property.

#### Non-breaking Changes

Firefox Translations code will always retrieve the maximum compatible version of each record from Remote Settings.
If two records exist with different versions, (e.g. **`1.0`** and **`1.1`**) then only the version **`1.1`** record
will be considered.

This allows us to update and ship new versions of models that are compatible with the current source code and wasm runtimes
in both backward-compatible and forward-compatible ways. These can be released through remote settings independent of the
[Firefox Release Schedule].

#### Breaking Changes

Breaking changes for Firefox Translations are a bit more tricky. These are changes that make older-version records
incompatible with the current Firefox source code and/or [WASM] runtimes.

While a breaking change will result in a change of the semver number (e.g. **`1.1 ⟶ 2.0`**), this alone is not
sufficient. Since Firefox Translations always attempts to use the maximum compatible version, only bumping this number
would result in older versions of Firefox attempting to use a newer-version record that is no longer compatible with the
Firefox source code or [WASM] runtimes.

To handle these changes, Firefox Translations utilizes Remote Settings [Filter Expressions] to make certain records
available to only particular releases of Firefox. This will allow Firefox Translations to make different sets of Remote Settings records available to different versions
of Firefox.

```{admonition} Example

Let's say that Firefox 108 through Firefox 120 is compatible with translations model records in the **`1.*`** major-version range, however Firefox 121 and onward is compatible with only model records in the **`2.*`** major-version range.

This will allow us to mark the **`1.*`** major-version records with the following filter expression:

**`
"filter_expression": "env.version|versionCompare('108.a0') >= 0 && env.version|versionCompare('121.a0') < 0"
`**

This means that these records will only be available in Firefox versions greater than or equal to 108, and less than 121.

Similarly, we will be able to mark all of the **`2.*`** major-version records with this filter expression:

**`
"filter_expression": "env.version|versionCompare('121.a0') >= 0"
`**

This means that these records will only be available in Firefox versions greater than or equal to Firefox 121.

```

Tying breaking changes to releases in this way frees up Firefox Translations to make changes as large as entirely
switching one third-party library for another in the compiled source code, while allowing older versions of Firefox to continue utilizing the old library and allowing newer versions of Firefox to utilize the new library.

---
## Language Identification

Translations currently uses the [CLD2] language detector.

We have previously experimented with using the [fastText] language detector, but we opted to use [CLD2] due to complications with [fastText] [WASM] runtime performance. The benefit of the [CLD2] language detector is that it already exists in the Firefox source tree. In the future, we would still like to explore moving to a more modern language detector such as [CLD3], or perhaps something else.


<!-- Hyperlinks -->
[Bugzilla]: https://bugzilla.mozilla.org/enter_bug.cgi?product=Cloud%20Services&component=Server%3A%20Remote%20Settings
[Child]: https://searchfox.org/mozilla-central/search?q=TranslationsChild
[CLD2]: https://github.com/CLD2Owners/cld2
[CLD3]: https://github.com/google/cld3
[Download and Install]: https://emscripten.org/docs/getting_started/downloads.html#download-and-install
[emscripten (2.0.3)]: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#203-09102020
[emscripten (2.0.18)]: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#2018-04232021
[emscripten (3.1.35)]: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md#3135---040323
[Environments]: https://remote-settings.readthedocs.io/en/latest/getting-started.html#environments
[eval()]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval
[fastText]: https://fasttext.cc/
[Filter Expressions]: https://remote-settings.readthedocs.io/en/latest/target-filters.html#filter-expressions
[Firefox Release Schedule]: https://wiki.mozilla.org/Release_Management/Calendar
[generate functions]: https://emscripten.org/docs/api_reference/emscripten.h.html?highlight=dynamic_execution#functions
[Getting Set Up To Work On The Firefox Codebase]: https://firefox-source-docs.mozilla.org/setup/index.html
[importScripts()]: https://developer.mozilla.org/en-US/docs/Web/API/WorkerGlobalScope/importScripts
[JSWindowActors]: https://firefox-source-docs.mozilla.org/dom/ipc/jsactors.html#jswindowactor
[minify]: https://github.com/tdewolff/minify
[Parent]: https://searchfox.org/mozilla-central/search?q=TranslationsParent
[Step 3]: https://remote-settings.readthedocs.io/en/latest/getting-started.html#create-a-new-official-type-of-remote-settings
[remote-settings-devtools]: https://github.com/mozilla-extensions/remote-settings-devtools/releases
[Remote Settings]: https://remote-settings.readthedocs.io/en/latest/
[toolkit/components/translations]: https://searchfox.org/mozilla-central/search?q=toolkit%2Fcomponents%2Ftranslations
[WASM]: https://webassembly.org/
[Workers]: https://searchfox.org/mozilla-central/search?q=%2Ftranslations.*worker&path=&case=false&regexp=true
