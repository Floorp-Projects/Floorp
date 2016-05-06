# lint_ignore=E501
config = {
    "shipped-locales-url": "https://hg.mozilla.org/%(repo)s/raw-file/%(revision)s/browser/locales/shipped-locales",
    "products": {
        "installer": {
            "product-name": "Firefox-%(version)s",
            "check_uptake": True,
            "alias": "firefox-beta-latest",
            "ssl-only": False,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/firefox/releases/%(version)s/linux-i686/:lang/firefox-%(version)s.tar.bz2",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/firefox/releases/%(version)s/linux-x86_64/:lang/firefox-%(version)s.tar.bz2",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/firefox/releases/%(version)s/mac/:lang/Firefox%%20%(version)s.dmg",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/firefox/releases/%(version)s/win32/:lang/Firefox%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/firefox/releases/%(version)s/win64/:lang/Firefox%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win64",
                },
            },
        },
        "installer-ssl": {
            "product-name": "Firefox-%(version)s-SSL",
            "check_uptake": True,
            "ssl-only": True,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/firefox/releases/%(version)s/linux-i686/:lang/firefox-%(version)s.tar.bz2",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/firefox/releases/%(version)s/linux-x86_64/:lang/firefox-%(version)s.tar.bz2",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/firefox/releases/%(version)s/mac/:lang/Firefox%%20%(version)s.dmg",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/firefox/releases/%(version)s/win32/:lang/Firefox%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/firefox/releases/%(version)s/win64/:lang/Firefox%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win64",
                },
            },
        },
        "stub-installer": {
            "product-name": "Firefox-%(version)s-stub",
            "check_uptake": True,
            "alias": "firefox-beta-stub",
            "ssl-only": True,
            "add-locales": True,
            "paths": {
                "win32": {
                    "path": "/firefox/releases/%(version)s/win32/:lang/Firefox%%20Setup%%20Stub%%20%(version)s.exe",
                    "bouncer-platform": "win",
                },
            },
        },
        "complete-mar": {
            "product-name": "Firefox-%(version)s-Complete",
            "check_uptake": True,
            "ssl-only": False,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/firefox/releases/%(version)s/update/linux-i686/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/firefox/releases/%(version)s/update/linux-x86_64/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/firefox/releases/%(version)s/update/mac/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/firefox/releases/%(version)s/update/win32/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/firefox/releases/%(version)s/update/win64/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "win64",
                },
            },
        },
    },
    "partials": {
        "releases-dir": {
            "product-name": "Firefox-%(version)s-Partial-%(prev_version)s",
            "check_uptake": True,
            "ssl-only": False,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/firefox/releases/%(version)s/update/linux-i686/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/firefox/releases/%(version)s/update/linux-x86_64/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/firefox/releases/%(version)s/update/mac/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/firefox/releases/%(version)s/update/win32/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/firefox/releases/%(version)s/update/win64/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "win64",
                },
            },
        },
    },
}
