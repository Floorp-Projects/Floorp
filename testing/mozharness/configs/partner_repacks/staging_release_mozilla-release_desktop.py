config = {
    "appName": "Firefox",
    "log_name": "partner_repack",
    "repack_manifests_url": "git@github.com:mozilla-partners/repack-manifests.git",
    "repo_file": "https://raw.githubusercontent.com/mozilla/git-repo/master/repo",
    "secret_files": [
        {
            "filename": "/builds/partner-github-ssh",
            "secret_name": "project/releng/gecko/build/level-%(scm-level)s/partner-github-ssh",
            "min_scm_level": 3,
            "mode": 0o600,
        },
    ],
    "ssh_key": "/builds/partner-github-ssh",
}
