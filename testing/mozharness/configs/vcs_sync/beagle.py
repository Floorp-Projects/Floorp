# This is for gecko-dev, which is a developer-oriented repo with
# release-train and inbound branches.

import os
import socket
hostname = socket.gethostname()

CVS_MANIFEST = """[{
"size": 1301484692,
"digest": "89df462d8d20f54402caaaa4e3c10aa54902a1d7196cdf86b7790b76e62d302ade3102dc3f7da4145dd832e6938b0472370ce6a321e0b3bcf0ad050937bd0e9a",
"algorithm": "sha512",
"filename": "mozilla-cvs-history.tar.bz2"
}]
"""

config = {
    "log_name": "beagle",
    "log_max_rotate": 99,
    "repos": [{
        "repo": "https://hg.mozilla.org/users/hwine_mozilla.com/repo-sync-tools",
        "vcs": "hg",
    }],
    "job_name": "beagle",
    "conversion_dir": "beagle",
    "initial_repo": {
        "repo": "https://hg.mozilla.org/mozilla-central",
        "revision": "default",
        "repo_name": "mozilla-central",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "master",
            },
        },
    },
    "backup_dir": "/mnt/netapp/github_sync/aki/%s" % hostname,
    "cvs_manifest": CVS_MANIFEST,
    "cvs_history_tarball": "/home/pmoore/mozilla-cvs-history.tar.bz2",
    "env": {
        "PATH": "%(PATH)s:/usr/libexec/git-core",
    },
    "conversion_repos": [{
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g18",
        "revision": "default",
        "repo_name": "mozilla-b2g18",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
            "tag_config": {
                "tag_regexes": [
                    "^B2G_",
                ],
            },
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g18",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g26_v1_2",
        "revision": "default",
        "repo_name": "mozilla-b2g26_v1_2",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
            "tag_config": {
                "tag_regexes": [
                    "^B2G_",
                ],
            },
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g26_v1_2",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g26_v1_2f",
        "revision": "default",
        "repo_name": "mozilla-b2g26_v1_2f",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
            "tag_config": {
                "tag_regexes": [
                    "^B2G_",
                ],
            },
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g26_v1_2f",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g28_v1_3",
        "revision": "default",
        "repo_name": "mozilla-b2g28_v1_3",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
            "tag_config": {
                "tag_regexes": [
                    "^B2G_",
                ],
            },
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g28_v1_3",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g28_v1_3t",
        "revision": "default",
        "repo_name": "mozilla-b2g28_v1_3t",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
            "tag_config": {
                "tag_regexes": [
                    "^B2G_",
                ],
            },
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g28_v1_3t",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g30_v1_4",
        "revision": "default",
        "repo_name": "mozilla-b2g30_v1_4",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
            "tag_config": {
                "tag_regexes": [
                    "^B2G_",
                ],
            },
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g30_v1_4",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g34_v2_1",
        "revision": "default",
        "repo_name": "mozilla-b2g34_v2_1",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g34_v2_1",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g37_v2_2",
        "revision": "default",
        "repo_name": "mozilla-b2g37_v2_2",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g37_v2_2",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g37_v2_2r",
        "revision": "default",
        "repo_name": "mozilla-b2g37_v2_2r",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g37_v2_2r",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g32_v2_0",
        "revision": "default",
        "repo_name": "mozilla-b2g32_v2_0",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g32_v2_0",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g32_v2_0m",
        "revision": "default",
        "repo_name": "mozilla-b2g32_v2_0m",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g32_v2_0m",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g18_v1_1_0_hd",
        "revision": "default",
        "repo_name": "mozilla-b2g18_v1_1_0_hd",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g18_v1_1_0_hd",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g18_v1_0_1",
        "revision": "default",
        "repo_name": "mozilla-b2g18_v1_0_1",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g18_v1_0_1",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g18_v1_0_0",
        "revision": "default",
        "repo_name": "mozilla-b2g18_v1_0_0",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g18_v1_0_0",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-aurora",
        "revision": "default",
        "repo_name": "mozilla-aurora",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "aurora",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-beta",
        "revision": "default",
        "repo_name": "mozilla-beta",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "beta",
            },
            "branch_regexes": [
                "^GECKO[0-9_]*RELBRANCH$",
                "^MOBILE[0-9_]*RELBRANCH$",
            ],
        },
        "tag_config": {
            "tag_regexes": [
                "^(B2G|RELEASE_BASE)_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-release",
        "revision": "default",
        "repo_name": "mozilla-release",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "release",
            },
            "branch_regexes": [
                "^GECKO[0-9_]*RELBRANCH$",
                "^MOBILE[0-9_]*RELBRANCH$",
            ],
        },
        "tag_config": {
            "tag_regexes": [
                "^(B2G|RELEASE_BASE)_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-esr17",
        "revision": "default",
        "repo_name": "mozilla-esr17",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "esr17",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-esr31",
        "revision": "default",
        "repo_name": "mozilla-esr31",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "esr31",
            },
            "branch_regexes": [
                "^GECKO[0-9]+esr_[0-9]+_RELBRANCH$",
            ],
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-esr38",
        "revision": "default",
        "repo_name": "mozilla-esr38",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "esr38",
            },
            "branch_regexes": [
                "^GECKO[0-9]+esr_[0-9]+_RELBRANCH$",
            ],
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/integration/mozilla-inbound",
        "revision": "default",
        "repo_name": "mozilla-inbound",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "inbound",
            },
        },
        "tag_config": {},
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/integration/b2g-inbound",
        "revision": "default",
        "repo_name": "b2g-inbound",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "b2g-inbound",
            },
        },
        "tag_config": {},
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }, {
        "repo": "https://hg.mozilla.org/integration/fx-team",
        "revision": "default",
        "repo_name": "fx-team",
        "targets": [{
            "target_dest": "beagle/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "gitmo-beagle",
        }, {
            "target_dest": "github-beagle",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "fx-team",
            },
        },
        "tag_config": {},
        "mapper": {
            "url": "https://api.pub.build.mozilla.org/mapper",
            "project": "gecko-dev"
        },
    }],
    "remote_targets": {
        "github-beagle": {
            "repo": "git@github.com:mozilla/gecko-dev.git",
            "ssh_key": "~/.ssh/releng-github-id_rsa",
            "vcs": "git",
        },
        "gitmo-beagle": {
            "repo": "gitolite3@git.mozilla.org:integration/gecko-dev.git",
            "ssh_key": "~/.ssh/vcs-sync_rsa",
            "vcs": "git",
        },
    },

    "exes": {
        # bug 828140 - shut https warnings up.
        # http://kiln.stackexchange.com/questions/2816/mercurial-certificate-warning-certificate-not-verified-web-cacerts
        "hg": [os.path.join(os.getcwd(), "build", "venv", "bin", "hg"), "--config", "web.cacerts=/etc/pki/tls/certs/ca-bundle.crt"],
        "tooltool.py": [
            os.path.join(os.getcwd(), "build", "venv", "bin", "python"),
            os.path.join(os.getcwd(), "mozharness", "external_tools", "tooltool.py"),
        ],
    },

    "virtualenv_modules": [
        "bottle==0.11.6",
        "dulwich==0.9.0",
        "ordereddict==1.1",
        "hg-git==0.4.0-moz2",
        "mapper==0.1",
        "mercurial==2.6.3",
        "mozfile==0.9",
        "mozinfo==0.5",
        "mozprocess==0.11",
        "requests==2.2.1",
    ],
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    "default_notify_from": "developer-services+%s@mozilla.org" % hostname,
    "notify_config": [{
        "to": "releng-ops-trial@mozilla.com",
        "failure_only": False,
        "skip_empty_messages": True,
    }],

    # Disallow sharing, since we want pristine .hg and .git directories.
    "vcs_share_base": None,
    "hg_share_base": None,
    "default_actions": [
        'list-repos',
        'create-virtualenv',
        'update-stage-mirror',
        'update-work-mirror',
        'publish-to-mapper',
        'push',
        'combine-mapfiles',
        'notify',
    ],
}
