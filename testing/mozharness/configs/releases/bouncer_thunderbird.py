# lint_ignore=E501
config = {
    "shipped-locales-url": "https://hg.mozilla.org/%(repo)s/raw-file/%(revision)s/mail/locales/shipped-locales",
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
                "opensolaris-i386": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.opensolaris-i386.tar.bz2",
                    "bouncer-platform": "opensolaris-i386",
                },
                "opensolaris-sparc": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.opensolaris-sparc.tar.bz2",
                    "bouncer-platform": "opensolaris-sparc",
                },
                "solaris-i386": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.solaris-i386.tar.bz2",
                    "bouncer-platform": "solaris-i386",
                },
                "solaris-sparc": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.solaris-sparc.tar.bz2",
                    "bouncer-platform": "solaris-sparc",
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
                "opensolaris-i386": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.opensolaris-i386.tar.bz2",
                    "bouncer-platform": "opensolaris-i386",
                },
                "opensolaris-sparc": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.opensolaris-sparc.tar.bz2",
                    "bouncer-platform": "opensolaris-sparc",
                },
                "solaris-i386": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.solaris-i386.tar.bz2",
                    "bouncer-platform": "solaris-i386",
                },
                "solaris-sparc": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.solaris-sparc.tar.bz2",
                    "bouncer-platform": "solaris-sparc",
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
                "opensolaris-i386": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.opensolaris-i386.complete.mar",
                    "bouncer-platform": "opensolaris-i386",
                },
                "opensolaris-sparc": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.opensolaris-sparc.complete.mar",
                    "bouncer-platform": "opensolaris-sparc",
                },
                "solaris-i386": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.solaris-i386.complete.mar",
                    "bouncer-platform": "solaris-i386",
                },
                "solaris-sparc": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(version)s.en-US.solaris-sparc.complete.mar",
                    "bouncer-platform": "solaris-sparc",
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
                "opensolaris-i386": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(prev_version)s-%(version)s.en-US.opensolaris-i386.partial.mar",
                    "bouncer-platform": "opensolaris-i386",
                },
                "opensolaris-sparc": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(prev_version)s-%(version)s.en-US.opensolaris-sparc.partial.mar",
                    "bouncer-platform": "opensolaris-sparc",
                },
                "solaris-i386": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(prev_version)s-%(version)s.en-US.solaris-i386.partial.mar",
                    "bouncer-platform": "solaris-i386",
                },
                "solaris-sparc": {
                    "path": "/thunderbird/releases/%(version)s/contrib/solaris_tarball/thunderbird-%(prev_version)s-%(version)s.en-US.solaris-sparc.partial.mar",
                    "bouncer-platform": "solaris-sparc",
                },
            },
        },
    },
}
