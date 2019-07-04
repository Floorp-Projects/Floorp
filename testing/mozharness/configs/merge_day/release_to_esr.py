import os

ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
NEW_ESR_REPO = "https://hg.mozilla.org/releases/mozilla-esr68"

config = {
    "log_name": "relese_to_esr",
    "version_files": [
        {"file": "browser/config/version_display.txt", "suffix": "esr"},
    ],
    "replacements": [
        # File, from, to
        ("browser/confvars.sh",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-release",
         "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-esr"),
        ("browser/confvars.sh",
         "MAR_CHANNEL_ID=firefox-mozilla-release",
         "MAR_CHANNEL_ID=firefox-mozilla-esr"),
        ("build/mozconfig.common",
         "# Enable enforcing that add-ons are signed by the trusted root",
         "# Disable enforcing that add-ons are signed by the trusted root"),
        ("build/mozconfig.common",
         "MOZ_REQUIRE_SIGNING=${MOZ_REQUIRE_SIGNING-1}",
         "MOZ_REQUIRE_SIGNING=${MOZ_REQUIRE_SIGNING-0}"),
    ],
    "vcs_share_base": os.path.join(ABS_WORK_DIR, 'hg-shared'),
    # Pull from ESR repo, since we have already branched it and have landed esr-specific patches on it
    # We will need to manually merge mozilla-release into before runnning this.
    "from_repo_url": NEW_ESR_REPO,
    "to_repo_url": NEW_ESR_REPO,

    "base_tag": "FIREFOX_ESR_%(major_version)s_BASE",
    "migration_behavior": "release_to_esr",
    "require_remove_locales": False,
    "requires_head_merge": False,
    "pull_all_branches": False,
}
