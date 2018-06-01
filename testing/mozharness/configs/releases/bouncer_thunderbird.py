# lint_ignore=E501
config = {
    "shipped-locales-url": "https://hg.mozilla.org/%(repo)s/raw-file/%(revision)s/mail/locales/shipped-locales",
    "bouncer_prefix": "https://download.mozilla.org/",
    "products": {
        "installer": {
            "product-name": "Thunderbird-%(version)s",
            "check_uptake": True,
            "alias": "thunderbird-latest",
            "ssl-only": False,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/thunderbird/releases/%(version)s/linux-i686/:lang/thunderbird-%(version)s.tar.bz2",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/thunderbird/releases/%(version)s/linux-x86_64/:lang/thunderbird-%(version)s.tar.bz2",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/thunderbird/releases/%(version)s/mac/:lang/Thunderbird%%20%(version)s.dmg",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/thunderbird/releases/%(version)s/win32/:lang/Thunderbird%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win",
                },
            },
        },
        "installer-ssl": {
            "product-name": "Thunderbird-%(version)s-SSL",
            "check_uptake": True,
            "ssl-only": True,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/thunderbird/releases/%(version)s/linux-i686/:lang/thunderbird-%(version)s.tar.bz2",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/thunderbird/releases/%(version)s/linux-x86_64/:lang/thunderbird-%(version)s.tar.bz2",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/thunderbird/releases/%(version)s/mac/:lang/Thunderbird%%20%(version)s.dmg",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/thunderbird/releases/%(version)s/win32/:lang/Thunderbird%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win",
                },
            },
        },
        "complete-mar": {
            "product-name": "Thunderbird-%(version)s-Complete",
            "check_uptake": True,
            "ssl-only": False,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/thunderbird/releases/%(version)s/update/linux-i686/:lang/thunderbird-%(version)s.complete.mar",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/thunderbird/releases/%(version)s/update/linux-x86_64/:lang/thunderbird-%(version)s.complete.mar",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/thunderbird/releases/%(version)s/update/mac/:lang/thunderbird-%(version)s.complete.mar",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/thunderbird/releases/%(version)s/update/win32/:lang/thunderbird-%(version)s.complete.mar",
                    "bouncer-platform": "win",
                },
            },
        },
    },
    "partials": {
        "releases-dir": {
            "product-name": "Thunderbird-%(version)s-Partial-%(prev_version)s",
            "check_uptake": True,
            "ssl-only": False,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/thunderbird/releases/%(version)s/update/linux-i686/:lang/thunderbird-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/thunderbird/releases/%(version)s/update/linux-x86_64/:lang/thunderbird-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/thunderbird/releases/%(version)s/update/mac/:lang/thunderbird-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/thunderbird/releases/%(version)s/update/win32/:lang/thunderbird-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "win",
                },
            },
        },
    },
}
