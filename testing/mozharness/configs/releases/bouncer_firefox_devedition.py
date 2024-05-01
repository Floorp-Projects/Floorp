# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# lint_ignore=E501
config = {
    "products": {
        # for installers, stubs, msi (ie not updates) ...
        # products containing "latest" are for www.mozilla.org via cron-bouncer-check
        # products using versions are for release automation via release-bouncer-check-firefox
        "installer": {
            "product-name": "Devedition-%(version)s",
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "installer-latest": {
            "product-name": "Firefox-devedition-latest",
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "installer-ssl": {
            "product-name": "Devedition-%(version)s-SSL",
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "installer-latest-ssl": {
            "product-name": "Firefox-devedition-latest-SSL",
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "msi": {
            "product-name": "Devedition-%(version)s-msi-SSL",
            "platforms": [
                "win",
                "win64",
            ],
        },
        "msi-latest": {
            "product-name": "Firefox-devedition-msi-latest-SSL",
            "platforms": [
                "win",
                "win64",
            ],
        },
        "msix": {
            "product-name": "Devedition-%(version)s-msix-SSL",
            "platforms": [
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "msix-latest": {
            "product-name": "Firefox-devedition-msix-latest-SSL",
            "platforms": [
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "stub-installer": {
            "product-name": "Devedition-%(version)s-stub",
            "platforms": [
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "stub-installer-latest": {
            "product-name": "Firefox-devedition-stub",
            "platforms": [
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "complete-mar": {
            "product-name": "Devedition-%(version)s-Complete",
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
    },
    "partials": {
        "releases-dir": {
            "product-name": "Devedition-%(version)s-Partial-%(prev_version)s",
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
    },
}
