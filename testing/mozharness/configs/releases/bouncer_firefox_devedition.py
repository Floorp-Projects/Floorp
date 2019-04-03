# lint_ignore=E501
config = {
    "products": {
        "installer": {
            "product-name": "Devedition-%(version)s",
            "check_uptake": True,
            "paths": {
                "linux": {
                    "path": "/devedition/releases/%(version)s/linux-i686/:lang/firefox-%(version)s.tar.bz2",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/devedition/releases/%(version)s/linux-x86_64/:lang/firefox-%(version)s.tar.bz2",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/devedition/releases/%(version)s/mac/:lang/Firefox%%20%(version)s.dmg",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/devedition/releases/%(version)s/win32/:lang/Firefox%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/devedition/releases/%(version)s/win64/:lang/Firefox%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win64",
                },
            },
        },
        "installer-ssl": {
            "product-name": "Devedition-%(version)s-SSL",
            "check_uptake": True,
            "paths": {
                "linux": {
                    "path": "/devedition/releases/%(version)s/linux-i686/:lang/firefox-%(version)s.tar.bz2",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/devedition/releases/%(version)s/linux-x86_64/:lang/firefox-%(version)s.tar.bz2",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/devedition/releases/%(version)s/mac/:lang/Firefox%%20%(version)s.dmg",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/devedition/releases/%(version)s/win32/:lang/Firefox%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/devedition/releases/%(version)s/win64/:lang/Firefox%%20Setup%%20%(version)s.exe",
                    "bouncer-platform": "win64",
                },
            },
        },
        "msi": {
            "product-name": "Devedition-%(version)s-msi-SSL",
            "check_uptake": True,
            "paths": {
                "win32": {
                    "path": "/devedition/releases/%(version)s/win32/:lang/Firefox%%20Setup%%20%(version)s.msi",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/devedition/releases/%(version)s/win64/:lang/Firefox%%20Setup%%20%(version)s.msi",
                    "bouncer-platform": "win64",
                },
            },
        },
        "stub-installer": {
            "product-name": "Devedition-%(version)s-stub",
            "check_uptake": True,
            "paths": {
                "win32": {
                    "path": "/devedition/releases/%(version)s/win32/:lang/Firefox%%20Installer.exe",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/devedition/releases/%(version)s/win32/:lang/Firefox%%20Installer.exe",
                    "bouncer-platform": "win64",
                },
            },
        },
        "complete-mar": {
            "product-name": "Devedition-%(version)s-Complete",
            "check_uptake": True,
            "paths": {
                "linux": {
                    "path": "/devedition/releases/%(version)s/update/linux-i686/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/devedition/releases/%(version)s/update/linux-x86_64/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/devedition/releases/%(version)s/update/mac/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/devedition/releases/%(version)s/update/win32/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/devedition/releases/%(version)s/update/win64/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "win64",
                },
            },
        },
    },
    "partials": {
        "releases-dir": {
            "product-name": "Devedition-%(version)s-Partial-%(prev_version)s",
            "check_uptake": True,
            "paths": {
                "linux": {
                    "path": "/devedition/releases/%(version)s/update/linux-i686/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/devedition/releases/%(version)s/update/linux-x86_64/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/devedition/releases/%(version)s/update/mac/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/devedition/releases/%(version)s/update/win32/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/devedition/releases/%(version)s/update/win64/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "win64",
                },
            },
        },
    },
}
