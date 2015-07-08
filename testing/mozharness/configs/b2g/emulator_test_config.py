# This is a template config file for b2g emulator unittest testing

config = {
    # mozharness script options
    "application": "b2g",
    "test_suite": "xpcshell",                               # reftest, mochitest or xpcshell

    "emulator_url": "http://127.0.1.1/b2g/emulator.zip",    # url to emulator zip file
    "installer_url": "http://127.0.1.1/b2g/b2g.tar.gz",     # url to gecko build
    "xre_url": "http://127.0.1.1/b2g/xpcshell.zip",         # url to xpcshell zip file
    "test_url": "http://127.0.1.1/b2g/tests.zip",           # url to tests.zip
    "busybox_url": "http://127.0.1.1/b2g/busybox",          # url to busybox binary

    # testsuite options
    #"adb_path": "path/to/adb",           # defaults - os.environ['ADB_PATH']
    #"test_manifest": "path/to/manifest", # defaults - mochitest: "b2g.json"
                                          #              reftest: "tests/layout/reftests/reftest.list"
                                          #             xpcshell: "tests/xpcshell.ini"
    "total_chunks": 1,
    "this_chunk": 1,
}
