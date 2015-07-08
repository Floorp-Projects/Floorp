#####
config = {
    "application": "thunderbird",
    "minimum_tests_zip_dirs": [
        "bin/*",
        "certs/*",
        "extensions/*",
        "modules/*",
        "mozbase/*",
        "config/*"],
    "all_mozmill_suites": {
        "mozmill": ["--list=tests/mozmill/mozmilltests.list"],
    },
}
