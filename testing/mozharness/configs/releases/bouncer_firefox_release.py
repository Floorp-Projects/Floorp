# lint_ignore=E501
config = {
    "products": {
        # for installers, stubs, msi (ie not updates) ...
        # products containing "latest" are for www.mozilla.org via cron-bouncer-check
        # products using versions are for release automation via release-bouncer-check-firefox
        "installer": {
            "product-name": "Firefox-%(version)s",
            "check_uptake": True,
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
            "product-name": "Firefox-latest",
            "check_uptake": True,
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
            "product-name": "Firefox-%(version)s-SSL",
            "check_uptake": True,
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
            "product-name": "Firefox-latest-SSL",
            "check_uptake": True,
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
            "product-name": "Firefox-%(version)s-msi-SSL",
            "check_uptake": True,
            "platforms": [
                "win",
                "win64",
            ],
        },
        "msi-latest": {
            "product-name": "Firefox-msi-latest-SSL",
            "check_uptake": True,
            "platforms": [
                "win",
                "win64",
            ],
        },
        "stub-installer": {
            "product-name": "Firefox-%(version)s-stub",
            "check_uptake": True,
            "platforms": [
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "stub-installer-latest": {
            "product-name": "Firefox-stub",
            "check_uptake": True,
            "platforms": [
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "complete-mar": {
            "product-name": "Firefox-%(version)s-Complete",
            "check_uptake": True,
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "complete-mar-candidates": {
            "product-name": "Firefox-%(version)sbuild%(build_number)s-Complete",
            "check_uptake": False,
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
            "product-name": "Firefox-%(version)s-Partial-%(prev_version)s",
            "check_uptake": True,
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
        "candidates-dir": {
            "product-name": "Firefox-%(version)sbuild%(build_number)s-Partial-%(prev_version)sbuild%(prev_build_number)s",
            "check_uptake": False,
            "platforms": [
                "linux64",
                "osx",
                "win",
                "win64",
                "win64-aarch64",
            ],
        },
    },
}
