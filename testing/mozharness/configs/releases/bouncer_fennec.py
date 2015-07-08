# lint_ignore=E501
config = {
    "shipped-locales-url": "https://hg.mozilla.org/%(repo)s/raw-file/%(revision)s/mobile/android/locales/all-locales",    
    "products": {
        "installer": {
            "product-name": "Fennec-%(version)s",
            "ssl-only": False,
            "add-locales": True,
            "paths": {
                "android": {
                    "path": "/mobile/releases/%(version)s/android/:lang/fennec-%(version)s.:lang.android-arm.apk",
                    "bouncer-platform": "android",
                },
            },
        },
    },   
}