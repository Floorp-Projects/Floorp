#!/usr/bin/env python

config = {
    "log_name": "gaia_bump",
    "log_max_rotate": 99,
    "ssh_key": "~/.ssh/id_rsa",
    "ssh_user": "asasaki@mozilla.com",
    "hg_user": "Test Pusher <aki@escapewindow.com>",
    "revision_file": "b2g/config/gaia.json",
    "exes": {
        # Get around the https warnings
        "hg": ['hg', "--config", "web.cacerts=/etc/pki/tls/certs/ca-bundle.crt"],
    },
    "repo_list": [{
        "polling_url": "https://hg.mozilla.org/integration/gaia-central/json-pushes?full=1",
        "branch": "default",
        "repo_url": "https://hg.mozilla.org/integration/gaia-central",
        "repo_name": "gaia-central",
        "target_push_url": "ssh://hg.mozilla.org/users/asasaki_mozilla.com/birch",
        "target_pull_url": "https://hg.mozilla.org/users/asasaki_mozilla.com/birch",
        "target_tag": "default",
        "target_repo_name": "birch",
    }, {
        "polling_url": "https://hg.mozilla.org/integration/gaia-1_2/json-pushes?full=1",
        "branch": "default",
        "repo_url": "https://hg.mozilla.org/integration/gaia-1_2",
        "repo_name": "gaia-1_2",
        "target_push_url": "ssh://hg.mozilla.org/users/asasaki_mozilla.com/mozilla-aurora",
        "target_pull_url": "https://hg.mozilla.org/users/asasaki_mozilla.com/mozilla-aurora",
        "target_tag": "default",
        "target_repo_name": "mozilla-aurora",
    }, {
        "polling_url": "https://hg.mozilla.org/integration/gaia-1_2/json-pushes?full=1",
        "branch": "default",
        "repo_url": "https://hg.mozilla.org/integration/gaia-1_2",
        "repo_name": "gaia-1_2",
        "target_push_url": "ssh://hg.mozilla.org/users/asasaki_mozilla.com/mozilla-aurora",
        "target_pull_url": "https://hg.mozilla.org/users/asasaki_mozilla.com/mozilla-aurora",
        "target_tag": "default",
        "target_repo_name": "mozilla-aurora",
    }],
}
