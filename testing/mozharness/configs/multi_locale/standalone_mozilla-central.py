import os
# The name of the directory we'll pull our source into.
BUILD_DIR = "mozilla-central"
# This is everything that comes after https://hg.mozilla.org/
# e.g. "releases/mozilla-aurora"
REPO_PATH = "mozilla-central"
# This is where the l10n repos are (everything after https://hg.mozilla.org/)
# for mozilla-central, that's "l10n-central".
# For mozilla-aurora, that's "releases/l10n/mozilla-aurora"
L10N_REPO_PATH = "l10n-central"
# Currently this is assumed to be a subdirectory of your build dir
OBJDIR = "objdir-droid"
# Set this to mobile/xul for XUL Fennec
ANDROID_DIR = "mobile/android"
# Absolute path to your mozconfig.
# By default it looks at "./mozconfig"
MOZCONFIG = os.path.join(os.getcwd(), "mozconfig")

config = {
    "work_dir": ".",
    "log_name": "multilocale",
    "objdir": OBJDIR,
    "locales_file": "%s/%s/locales/maemo-locales" % (BUILD_DIR, ANDROID_DIR),
    "locales_dir": "%s/locales" % ANDROID_DIR,
    "ignore_locales": ["en-US", "multi"],
    "repos": [{
        "repo": "https://hg.mozilla.org/%s" % REPO_PATH,
        "tag": "default",
        "dest": BUILD_DIR,
    }],
    "l10n_repos": [{
        "repo": "https://hg.mozilla.org/build/compare-locales",
        "tag": "RELEASE_AUTOMATION"
    }],
    "hg_l10n_base": "https://hg.mozilla.org/%s" % L10N_REPO_PATH,
    "hg_l10n_tag": "default",
    "l10n_dir": "l10n",
    "merge_locales": True,
    "mozilla_dir": BUILD_DIR,
    "mozconfig": MOZCONFIG,
    "default_actions": [
        "pull-locale-source",
        "build",
        "package-en-US",
        "backup-objdir",
        "restore-objdir",
        "add-locales",
        "package-multi",
        "summary",
    ],
}
