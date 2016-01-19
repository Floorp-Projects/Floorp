#####
config = {
    "application": "thunderbird",
    "minimum_tests_zip_dirs": [
        "bin/*",
        "certs/*",
        "config/*",
        "extensions/*",
        "marionette/*",
        "modules/*",
        "mozbase/*",
        "tools/*",
    ],
    "all_mozmill_suites": {
        "mozmill": ["--list=tests/mozmill/mozmilltests.list"],
    },
}
