# Specify tooltool directory for thunderbird buildbot tests explicitly.
# The default configuration in `linux_unittest.py` specifies a
# taskcluster-specific location. Override that here until thunderbird is being
# built on taskcluster.
# See https://bugzilla.mozilla.org/show_bug.cgi?id=1356520
# and https://bugzilla.mozilla.org/show_bug.cgi?id=1371734
config = {
    "tooltool_cache": "/builds/tooltool_cache",
}
