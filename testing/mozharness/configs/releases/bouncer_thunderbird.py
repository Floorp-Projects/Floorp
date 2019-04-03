# lint_ignore=E501
config = {
    "products": {
        "installer": {
            "product-name": "Thunderbird-%(version)s",
            "check_uptake": True,
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
            ],
        },
        "installer-ssl": {
            "product-name": "Thunderbird-%(version)s-SSL",
            "check_uptake": True,
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
            ],
        },
        "complete-mar": {
            "product-name": "Thunderbird-%(version)s-Complete",
            "check_uptake": True,
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
            ],
        },
    },
    "partials": {
        "releases-dir": {
            "product-name": "Thunderbird-%(version)s-Partial-%(prev_version)s",
            "check_uptake": True,
            "platforms": [
                "linux",
                "linux64",
                "osx",
                "win",
            ],
        },
    },
}
