# lint_ignore=E501
config = {
    "products": {
        "apk": {
            "product-name": "Fennec-%(version)s",
            "check_uptake": True,
            "alias": "fennec-latest",
            "ssl-only": False,
            "add-locales": False,  # Do not add locales to let "multi" work
            "paths": {
                "android-api-15": {
                    "path": "/mobile/releases/%(version)s/android-api-15/:lang/fennec-%(version)s.:lang.android-arm.apk",
                    "bouncer-platform": "android",
                },
                "android-api-9": {
                    "path": "/mobile/releases/%(version)s/android-api-9/:lang/fennec-%(version)s.:lang.android-arm.apk",
                    "bouncer-platform": "android-api-9",
                },
                "android-x86": {
                    "path": "/mobile/releases/%(version)s/android-x86/:lang/fennec-%(version)s.:lang.android-i386.apk",
                    "bouncer-platform": "android-x86",
                },
            },
        },
    },
}
