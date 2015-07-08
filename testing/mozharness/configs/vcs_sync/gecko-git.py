# This is for gecko.git, which is a partner-oriented repo with
# B2G release branches + tags.

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
    "log_name": "gecko-git",
    "log_max_rotate": 99,
    "repos": [{
        "repo": "https://hg.mozilla.org/users/hwine_mozilla.com/repo-sync-tools",
        "vcs": "hg",
    }],
    "job_name": "gecko-git",
    "conversion_dir": "gecko-git",
    "initial_repo": {
        "repo": "https://hg.mozilla.org/mozilla-central",
        "revision": "default",
        "repo_name": "mozilla-central",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
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
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g37_v2_2",
        "revision": "default",
        "repo_name": "mozilla-b2g37_v2_2",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v2.2",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g34_v2_1",
        "revision": "default",
        "repo_name": "mozilla-b2g34_v2_1",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v2.1",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g32_v2_0",
        "revision": "default",
        "repo_name": "mozilla-b2g32_v2_0",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v2.0",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g30_v1_4",
        "revision": "default",
        "repo_name": "mozilla-b2g30_v1_4",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v1.4",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g28_v1_3",
        "revision": "default",
        "repo_name": "mozilla-b2g28_v1_3",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v1.3",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g26_v1_2",
        "revision": "default",
        "repo_name": "mozilla-b2g26_v1_2",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v1.2",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g26_v1_2f",
        "revision": "default",
        "repo_name": "mozilla-b2g26_v1_2f",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v1.2f",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g28_v1_3t",
        "revision": "default",
        "repo_name": "mozilla-b2g28_v1_3t",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v1.3t",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g30_v1_4",
        "revision": "default",
        "repo_name": "mozilla-b2g30_v1_4",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v1.4",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g18",
        "revision": "default",
        "repo_name": "mozilla-b2g18",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
            "tag_config": {
                "tag_regexes": [
                    "^B2G_",
                ],
            },
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "gecko-18",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g18_v1_1_0_hd",
        "revision": "default",
        "repo_name": "mozilla-b2g18_v1_1_0_hd",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v1.1.0hd",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g18_v1_0_1",
        "revision": "default",
        "repo_name": "mozilla-b2g18_v1_0_1",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v1.0.1",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }, {
        "repo": "https://hg.mozilla.org/releases/mozilla-b2g18_v1_0_0",
        "revision": "default",
        "repo_name": "mozilla-b2g18_v1_0_0",
        "targets": [{
            "target_dest": "gecko-git/.git",
            "vcs": "git",
            "test_push": True,
        }, {
            "target_dest": "github-gecko-git",
        }],
        "vcs": "hg",
        "branch_config": {
            "branches": {
                "default": "v1.0.0",
            },
        },
        "tag_config": {
            "tag_regexes": [
                "^B2G_",
            ],
        },
    }],
    "remote_targets": {
        "github-gecko-git": {
            "repo": "git@github.com:escapewindow/test-gecko-git.git",
            "ssh_key": "~/.ssh/escapewindow_github_rsa",
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
    ],
    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    "upload_config": [{
        "ssh_key": "~/.ssh/id_rsa",
        "ssh_user": "pmoore",
        "remote_host": "github-sync2",
        "remote_path": "/home/pmoore/upload/gecko-git-upload",
    }],

    "default_notify_from": "developer-services+%s@mozilla.org" % hostname,
    "notify_config": [{
        "to": "releng-ops-trial@mozilla.com",
        "failure_only": False,
        "skip_empty_messages": True,
    }],

    # Disallow sharing, since we want pristine .hg and .git directories.
    "vcs_share_base": None,
    "hg_share_base": None,
}
