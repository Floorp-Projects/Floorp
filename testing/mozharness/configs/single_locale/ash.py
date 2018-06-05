# This configuration uses mozilla-central binaries (en-US, localized complete
# mars) and urls but it generates 'ash' artifacts. With this setup, binaries
# generated on ash are NOT overwriting mozilla-central files.
# Using this configuration, on a successful build, artifacts will be uploaded
# here:
#
# * http://dev-stage01.srv.releng.scl3.mozilla.com/pub/mozilla.org/firefox/nightly/latest-ash-l10n/
#   (in staging environment)
# * https://ftp.mozilla.org/pub/firefox/nightly/latest-ash-l10n/
#   (in production environment)
#
# If you really want to have localized ash builds, use the use the following
# values:
# * "en_us_binary_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/tinderbox-builds/ash-%(platform)s/latest/",
# * "mar_tools_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/tinderbox-builds/ash-%(platform)s/latest/",
# * "repo": "https://hg.mozilla.org/projects/ash",
#

import os

config = {
    "nightly_build": True,
    "branch": "ash",
    "en_us_binary_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/",
    "update_channel": "nightly",

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar - passed in environ from taskcluster
    "mar_tools_url": os.environ["MAR_TOOLS_URL"],

    # purge options
    'is_automation': True,
}
