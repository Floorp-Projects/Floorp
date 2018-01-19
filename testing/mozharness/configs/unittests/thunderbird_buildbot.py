import platform

# Specify tooltool directory for thunderbird buildbot tests explicitly.
# The default configuration in `linux_unittest.py` specifies a
# taskcluster-specific location. Override that here until thunderbird is being
# built on taskcluster.
# See https://bugzilla.mozilla.org/show_bug.cgi?id=1356520
# and https://bugzilla.mozilla.org/show_bug.cgi?id=1371734
config = {
    "tooltool_cache": "/builds/tooltool_cache",
}

# Specify virtualenv directory for thunderbird buildbot tests explicitly.
# The default configuration in `linux_unittest.py` specifies a path that
# doesn't exist on buildbot. We need to specify the other paths here too, since
# config entries are overriden wholesale.
if platform.system() == "Linux":
    config["exes"] = {
        "python": "/tools/buildbot/bin/python",
        "tooltool.py": "/tools/tooltool.py",
    }
