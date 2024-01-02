# This is a template config file for marionette test.

config = {
    # marionette options
    "marionette_address": "localhost:2828",
    "test_manifest": "unit-tests.toml",
    "default_actions": [
        "clobber",
        "download-and-extract",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "suite_definitions": {
        "marionette_desktop": {
            "options": [
                "-vv",
                "--log-errorsummary=%(error_summary_file)s",
                "--log-html=%(html_report_file)s",
                "--binary=%(binary)s",
                "--address=%(address)s",
                "--symbols-path=%(symbols_path)s",
            ],
            "run_filename": "",
            "testsdir": "marionette",
        },
    },
}
