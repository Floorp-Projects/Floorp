# This configuration uses mozilla-central binaries (en-US, localized complete
# mars) and urls but it generates 'alder' artifacts. With this setup, binaries
# generated on alder are NOT overwriting mozilla-central files.
# Using this configuration, on a successful build, artifacts will be uploaded
# here:
#
# * http://dev-stage01.srv.releng.scl3.mozilla.com/pub/mozilla.org/firefox/nightly/latest-alder-l10n/
#   (in staging environment)
# * https://ftp.mozilla.org/pub/firefox/nightly/latest-alder-l10n/
#   (in production environment)
#
# If you really want to have localized alder builds, use the use the following
# values:
# * "en_us_binary_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/tinderbox-builds/alder-%(platform)s/latest/",
# * "mar_tools_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/tinderbox-builds/alder-%(platform)s/latest/",
# * "repo": "https://hg.mozilla.org/projects/alder",
#

config = {
    "nightly_build": True,
    "branch": "alder",
    "en_us_binary_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/",
    "update_channel": "nightly",
    "latest_mar_dir": '/pub/mozilla.org/firefox/nightly/latest-mozilla-central-l10n',

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar
    "enable_partials": True,
    "mar_tools_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/mar-tools/%(platform)s",
    "previous_mar_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central-l10n",

    # repositories
    "mozilla_dir": "alder",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "revision": "default",
        "dest": "tools",
    }, {
        "vcs": "hgtool",
        "repo": "https://hg.mozilla.org/mozilla-central",
        "revision": "default",
        "dest": "alder",
    }, {
        "vcs": "hgtool",
        "repo": "https://hg.mozilla.org/build/compare-locales",
        "revision": "RELEASE_AUTOMATION"
    }],
    # purge options
    'is_automation': True,
    'purge_minsize': 12,
}
