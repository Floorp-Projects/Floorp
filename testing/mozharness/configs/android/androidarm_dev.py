# This config contains dev values that will replace
# the values specified in the production config
# if specified like this (order matters):
# --cfg android/androidarm.py
# --cfg android/androidarm_dev.py
import os
config = {
    "tooltool_cache_path": os.path.join(os.getenv("HOME"), "cache"),
}
