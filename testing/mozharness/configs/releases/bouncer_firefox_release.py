# lint_ignore=E501
config = {
    "shipped-locales-url": "https://hg.mozilla.org/%(repo)s/raw-file/%(revision)s/browser/locales/shipped-locales",
    "products": {
        "installer": {
            "product-name": "Firefox-%(version)s",
            "check_uptake": True,
            "alias": "firefox-latest",
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
                "opensolaris-i386": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.opensolaris-i386.tar.bz2",
                    "bouncer-platform": "opensolaris-i386",
                },
                "opensolaris-sparc": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.opensolaris-sparc.tar.bz2",
                    "bouncer-platform": "opensolaris-sparc",
                },
                "solaris-i386": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.solaris-i386.tar.bz2",
                    "bouncer-platform": "solaris-i386",
                },
                "solaris-sparc": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.solaris-sparc.tar.bz2",
                    "bouncer-platform": "solaris-sparc",
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
                "opensolaris-i386": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.opensolaris-i386.tar.bz2",
                    "bouncer-platform": "opensolaris-i386",
                },
                "opensolaris-sparc": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.opensolaris-sparc.tar.bz2",
                    "bouncer-platform": "opensolaris-sparc",
                },
                "solaris-i386": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.solaris-i386.tar.bz2",
                    "bouncer-platform": "solaris-i386",
                },
                "solaris-sparc": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.solaris-sparc.tar.bz2",
                    "bouncer-platform": "solaris-sparc",
                },
            },
        },
        "stub-installer": {
            "product-name": "Firefox-%(version)s-stub",
            "check_uptake": True,
            "alias": "firefox-stub",
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
                "opensolaris-i386": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.opensolaris-i386.complete.mar",
                    "bouncer-platform": "opensolaris-i386",
                },
                "opensolaris-sparc": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.opensolaris-sparc.complete.mar",
                    "bouncer-platform": "opensolaris-sparc",
                },
                "solaris-i386": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.solaris-i386.complete.mar",
                    "bouncer-platform": "solaris-i386",
                },
                "solaris-sparc": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(version)s.en-US.solaris-sparc.complete.mar",
                    "bouncer-platform": "solaris-sparc",
                },
            },
        },
        "complete-mar-candidates": {
            "product-name": "Firefox-%(version)sbuild%(build_number)s-Complete",
            "check_uptake": False,
            "ssl-only": False,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/linux-i686/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/linux-x86_64/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/mac/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/win32/:lang/firefox-%(version)s.complete.mar",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/win64/:lang/firefox-%(version)s.complete.mar",
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
                "opensolaris-i386": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(prev_version)s-%(version)s.en-US.opensolaris-i386.partial.mar",
                    "bouncer-platform": "opensolaris-i386",
                },
                "opensolaris-sparc": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(prev_version)s-%(version)s.en-US.opensolaris-sparc.partial.mar",
                    "bouncer-platform": "opensolaris-sparc",
                },
                "solaris-i386": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(prev_version)s-%(version)s.en-US.solaris-i386.partial.mar",
                    "bouncer-platform": "solaris-i386",
                },
                "solaris-sparc": {
                    "path": "/firefox/releases/%(version)s/contrib/solaris_tarball/firefox-%(prev_version)s-%(version)s.en-US.solaris-sparc.partial.mar",
                    "bouncer-platform": "solaris-sparc",
                },
            },
        },
        "candidates-dir": {
            "product-name": "Firefox-%(version)sbuild%(build_number)s-Partial-%(prev_version)sbuild%(prev_build_number)s",
            "check_uptake": False,
            "ssl-only": False,
            "add-locales": True,
            "paths": {
                "linux": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/linux-i686/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "linux",
                },
                "linux64": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/linux-x86_64/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "linux64",
                },
                "macosx64": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/mac/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "osx",
                },
                "win32": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/win32/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "win",
                },
                "win64": {
                    "path": "/firefox/candidates/%(version)s-candidates/build%(build_number)s/update/win64/:lang/firefox-%(prev_version)s-%(version)s.partial.mar",
                    "bouncer-platform": "win64",
                },
            },
        },
    },
}
