import os

platform = "win32"

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

if not os.environ.get("NO_STUB_INSTALLER"):
    # Some channels, like esr don't build a stub installer
    repackage_config.append([
        "installer",
        "--tag", "{abs_mozilla_dir}\\browser\\installer\\windows\\stub.tag",
        "--setupexe", "{abs_input_dir}\\setup-stub.exe",
        "-o", "{abs_output_dir}\\target.stub-installer.exe",
        "--sfx-stub", "other-licenses/7zstub/firefox/7zSD.sfx",
    ])

config = {
    "locale": os.environ.get("LOCALE"),

    "repackage_config": repackage_config,

    # ToolTool
    "tooltool_manifest_src": 'browser\\config\\tooltool-manifests\\{}\\releng.manifest'.format(platform),
    'tooltool_url': 'https://tooltool.mozilla-releng.net/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),

    'run_configure': False,
}
