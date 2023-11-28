# The Bergamot Translator

The [Bergamot Translator](https://github.com/browsermt/bergamot-translator) is the translations engine used to power Firefox translations. The project configures a fork of [Marian NMT](https://marian-nmt.github.io/) that enables translations through a Wasm API.

Bergamot adds a few additional pieces of code on top of the Marian code, which includes HTML alignments (matching up source and target tags in a translation) and sentence iteration. It provides the [Wasm API](https://github.com/browsermt/bergamot-translator/tree/main/wasm) that Firefox uses in its own translation implementation. The Bergamot Translator uses a forked copy of the Marian NMT package in order to provide support for quantized translation models.

---
## Building Bergamot

The Wasm and the JS file that integrate with Firefox can be generated using the `build-bergamot.py` script.

```sh
cd toolkit/components/translations/bergamot-translator
./build-bergamot.py
```

There are a few additional options and up to date documentation for building which are documented by:

```sh
./build-bergamot.py --help
```

After building, the Wasm can be loaded locally for testing by uncommenting the lines at the bottom of `toolkit/components/translations/jar.mn`. In addition, debug symbols can be built with the `--debug` option. This is useful for using the Firefox Profiler.

---
## Uploading to Remote Settings

The Wasm artifact is uploaded and distributed via [Remote Settings](https://remote-settings.readthedocs.io/en/latest/index.html). An upload script is available for updating the Wasm in Remote Setting via:

```sh
cd toolkit/components/translations/bergamot-translator
./upload-bergamot.py --help
```

The help flag will output up to date documentation on how to run the script. In order to do a full release:

### Breaking changes

If the Bergamot Translator has a breaking change, then the `BERGAMOT_MAJOR_VERSION` in `toolkit/components/translations/actors/TranslationsParent.sys.mjs` will need to be incremented by one. Any given release of Firefox will pull in minor changes when the records are updated, but major changes will need to ride the release trains.

### Releasing

1. Run the `./build-bergamot.py` script

1. Bump the `remote_settings.version` in `toolkit/components/translations/bergamot-translator/moz.yaml`.
    - A minor release would be `"1.0"` ➡️ `"1.1"`.
    - A major release would be `"1.1"` ➡️ `"2.0"`.

1. Run the `./upload-bergamot.py --server prod`
    - Follow the instructions for adding the Bearer Token.
    - By default new updates use JEXL filters and are filtered to just Nightly and local builds.

1. Request review on the changes.
    - Log in to the [Mozilla Corporate VPN](https://mozilla-hub.atlassian.net/wiki/spaces/IT/pages/15761733/Mozilla+Corporate+VPN)
    - Log into the [Remote Settings admin](https://remote-settings.mozilla.org/v1/admin)
    - If this is a major change, then the `filter_expression` can be removed, as the change will ride the trains.
    - Request review on the changes.

1. Verify the changes on Nightly.
    - Install the [Remote Settings Devtool](https://github.com/mozilla-extensions/remote-settings-devtools/releases).
    - Open the Remote Settings Devtool.
    - Switch the environment to `Prod (preview)`.
    - Clear all local data.
    - Restart Nightly.
    - Verify that it is working in Nightly by trigging different translations.

1. Publish to Nightly
    - Notify release drivers (<release-drivers@mozilla.org>) that a new translation engine release is hitting Nightly (see example emails below). This is optional for a major release, since it will ride the trains.
    - Have another team member approve the release from Remote Settings.

1. Prepare to publish to Beta / Release
    - (Do not do this step if it's a major release.)
    - Wait a few days to verify there are no issues on Nightly.
    - Log into the [Remote Settings admin](https://remote-settings.mozilla.org/v1/admin)
    - Remove the "filter_expression" text from the `bergamot-translator` version.
    - Request review.
    - Repeat Step 5 to verify for Beta and Release.

1. Publish to Beta / Release
    - (Do not do this step if it's a major release.)
    - Notify release drivers (<release-drivers@mozilla.org>) that a new translation engine release is hitting Beta / Release (see example emails below).
    - Publish the changes
    - Monitor for any increased breakage via [telemetry](https://sql.telemetry.mozilla.org/dashboard/translations?p_date=d_last_7_days).


### Example Nightly release email

```
Hello Release Drivers,

The Translations team is releasing a new version of the translations engine via remote
settings. We are releasing a test update on Nightly [Fx123], and plan to follow-up on
[DATE] with a release to both Beta [Fx123] and Release [Fx123] if we've found there are
no issues. We can roll back the release if any unexpected issues are found.

The plan for this release is available:

https://firefox-source-docs.mozilla.org/toolkit/components/translations/resources/03_bergamot.html#release

Thank you,
[NAME]
```

### Example Beta / Release release email

```
Hello Release Drivers,

The Translations team is moving forward with a release of a new translations engine
to both Beta [Fx123] and Release [Fx123]. It has been in Nightly [Fx123] with no issues
found. We can roll back the release if any unexpected issues are found.

The plan for this release is available:

https://firefox-source-docs.mozilla.org/toolkit/components/translations/resources/03_bergamot.html#release

Thank you,
[NAME]
```
