[![Build Status](https://travis-ci.org/Pike/compare-locales.svg?branch=master)](https://travis-ci.org/Pike/compare-locales)
# compare-locales
Lint Mozilla localizations

Finds
* missing strings
* obsolete strings
* errors on runtime errors without false positives
* warns on possible runtime errors

It also includes `l10n-merge` functionality, which pads localizations with
missing English strings, and replaces entities with errors with English.

If you want to check your original code for errors like duplicated messages,
use `moz-l10n-lint`, which is also part of this package. You can also use
this to check for conflicts between your strings and those already exposed
to l10n.

# Configuration

You configure `compare-locales` (and `moz-l10n-lint`) through a
[project configuration](https://moz-l10n-config.readthedocs.io/en/latest/fileformat.html)
file, `l10n.toml`.

# Examples

To check all locales in a project use

```bash
compare-locales l10n.toml .
```

To check Firefox against a local check-out of l10n-central, use

```bash
compare-locales browser/locales/l10n.toml ../l10n-central
```

If you just want to check particular locales, specify them as additional
commandline parameters.

To lint your local work, use

```bash
moz-l10n-lint l10n.toml
```

To check for conflicts against already existing strings:

```bash
moz-l10n-lint --reference-project ../android-l10n/mozilla-mobile/fenix l10n.toml
moz-l10n-lint --l10n-reference ../gecko-strings browser/locales/l10n.toml
```

to check for a monolithic project like Fenix or a gecko project like Firefox,
resp.
