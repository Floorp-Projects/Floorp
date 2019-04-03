# lint_ignore=E501
config = {
    "products": {
        "installer": {
            "product-name": "Firefox-%(version)s",
            "check_uptake": True,
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
        "stub-installer": {
            "product-name": "Firefox-%(version)s-stub",
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
