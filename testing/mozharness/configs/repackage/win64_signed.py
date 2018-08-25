import os

platform = "win64"

repackage_config = [[
        "installer",
        "--package-name", "firefox",
        "--package", "{abs_input_dir}\\target.zip",
        "--tag", "{abs_mozilla_dir}\\browser\\installer\\windows\\app.tag",
        "--setupexe", "{abs_input_dir}\\setup.exe",
        "-o", "{abs_output_dir}\\target.installer.exe",
        "--sfx-stub", "other-licenses/7zstub/firefox/7zSD.sfx",
    ], [
        "mar",
        "-i", "{abs_input_dir}\\target.zip",
        "--mar", "{abs_input_dir}\\mar.exe",
        "-o", "{abs_output_dir}\\target.complete.mar",
    ]]

config = {
    "locale": os.environ.get("LOCALE"),

    "repackage_config": repackage_config,

    # ToolTool
    "tooltool_manifest_src": 'browser\\config\\tooltool-manifests\\{}\\releng.manifest'.format(platform),
    'tooltool_url': 'https://tooltool.mozilla-releng.net/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),

    'run_configure': False,
}
