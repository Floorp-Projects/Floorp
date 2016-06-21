config = {
    # Additional Android 4.3 settings required when running in taskcluster.
    "avds_dir": "/home/worker/workspace/build/.android",
    "tooltool_cache": "/home/worker/tooltool_cache",
    "download_tooltool": True,
    "tooltool_servers": ['http://relengapi/tooltool/'],
    "exes": {
        'adb': '%(abs_work_dir)s/android-sdk18/platform-tools/adb',
    }
}
