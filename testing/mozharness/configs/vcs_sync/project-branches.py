# This is for gecko-projects, which is a low-SLA developer-oriented repo
# with mozilla-central based project branches.

import os
import socket
hostname = socket.gethostname()

# These all need to be under hg.m.o/projects.
# If you need to add a different repo, add it to CONVERSION_REPOS.
PROJECT_BRANCHES = [
    # twig projects
    "alder",
    "ash",
    "birch",
    "cedar",
    "cypress",
    "date",
    "elm",
    "fig",
    "gum",
    "holly",
    "jamun",
    "larch",
    "maple",
    "oak",
    "pine",
    # Non-twig projects
    "build-system",
    "graphics",
    "ux",
]

# Non-hg.m.o/projects/ repos.
CONVERSION_REPOS = [{
    "repo": "https://hg.mozilla.org/services/services-central",
    "revision": "default",
    "repo_name": "services-central",
    "targets": [{
        "target_dest": "github-project-branches",
    }],
    "vcs": "hg",
    "branch_config": {
        "branches": {
            "default": "services",
        },
    },
    "tag_config": {},
}]

config = {
    "log_name": "project-branches",
    "log_max_rotate": 99,
    "repos": [{
        "repo": "https://hg.mozilla.org/users/hwine_mozilla.com/repo-sync-tools",
        "vcs": "hg",
    }],
    "job_name": "project-branches",
    "conversion_dir": "project-branches",
    "mapfile_name": "project-branches-mapfile",
    "env": {
        "PATH": "%(PATH)s:/usr/libexec/git-core",
    },
    "conversion_type": "project-branches",
    "project_branches": PROJECT_BRANCHES,
    "project_branch_repo_url": "https://hg.mozilla.org/projects/%(project)s",
    "conversion_repos": CONVERSION_REPOS,
    "remote_targets": {
        "github-project-branches": {
            "repo": "git@github.com:mozilla/gecko-projects.git",
            "ssh_key": "~/.ssh/releng-github-id_rsa",
            "vcs": "git",
            "force_push": True,
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

    "combined_mapfile": "combined_gecko_mapfile",

    "default_notify_from": "developer-services@mozilla.org",
    "notify_config": [{
        "to": "releng-ops-trial@mozilla.com",
        "failure_only": False,
        "skip_empty_messages": True,
    }],

    # Disallow sharing, since we want pristine .hg and .git directories.
    "vcs_share_base": None,
    "hg_share_base": None,
}
