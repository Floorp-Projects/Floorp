# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# lint_ignore=E501
config = {
    "products": {
        "installer-latest": {
            "product-name": "Firefox-nightly-latest",
            "platforms": [
                "linux",
                "linux64",
                "linux64-aarch64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "installer-latest-ssl": {
            "product-name": "Firefox-nightly-latest-SSL",
            "platforms": [
                "linux",
                "linux64",
                "linux64-aarch64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "installer-latest-l10n-ssl": {
            "product-name": "Firefox-nightly-latest-l10n-SSL",
            "platforms": [
                "linux",
                "linux64",
                "linux64-aarch64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "msi-latest": {
            "product-name": "Firefox-nightly-msi-latest-SSL",
            "platforms": [
                "win",
                "win64",
            ],
        },
        "msi-latest-l10n": {
            "product-name": "Firefox-nightly-msi-latest-l10n-SSL",
            "platforms": [
                "win",
                "win64",
            ],
        },
        "stub-installer": {
            "product-name": "Firefox-nightly-stub",
            "platforms": [
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "stub-installer-l10n": {
            "product-name": "Firefox-nightly-stub-l10n",
            "platforms": [
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "pkg-latest": {
            "product-name": "Firefox-nightly-pkg-latest-ssl",
            "platforms": ["osx"],
        },
        "pkg-latest-l10n": {
            "product-name": "Firefox-nightly-pkg-latest-l10n-ssl",
            "platforms": ["osx"],
        },
    },
}
