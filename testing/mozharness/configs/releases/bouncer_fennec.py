# lint_ignore=E501
config = {
    "bouncer_prefix": "https://download.mozilla.org/",
    "products": {
        "apk": {
            "product-name": "Fennec-%(version)s",
            "check_uptake": True,
            "alias": "fennec-latest",
            "ssl-only": True,
            "add-locales": False,  # Do not add locales to let "multi" work
            "paths": {
                "android-api-16": {
                    "path": "/mobile/releases/%(version)s/android-api-16/:lang/fennec-%(version)s.:lang.android-arm.apk",
                    "bouncer-platform": "android",
                },
                "android-x86": {
                    "path": "/mobile/releases/%(version)s/android-x86/:lang/fennec-%(version)s.:lang.android-i386.apk",
                    "bouncer-platform": "android-x86",
                },
            },
        },
    },
}
