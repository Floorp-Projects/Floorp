# lint_ignore=E501
config = {
    "products": {
        "installer": {
            "product-name": "Firefox-%(version)s",
            "check_uptake": True,
            # convert to firefox-esr-latest when ESR52 stops
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
            ],
        },
        "installer-ssl": {
            "product-name": "Firefox-%(version)s-SSL",
            "check_uptake": True,
            # convert to firefox-esr-latest-ssl when ESR52 stops
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
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
        "complete-mar": {
            "product-name": "Firefox-%(version)s-Complete",
            "check_uptake": True,
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
                "win64",
            ],
        },
        "complete-mar-bz2": {
            "product-name": "Firefox-%(version)s-Complete-bz2",
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
            ],
        },
    },
}
