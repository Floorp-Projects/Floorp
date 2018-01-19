import os

platform = "win64"

download_config = {
        "target.zip": os.environ.get("SIGNED_ZIP"),
        "setup.exe": os.environ.get("SIGNED_SETUP"),
        "mar.exe": os.environ.get("UNSIGNED_MAR"),
    }

repackage_config = [[
        "installer",
        "--package", "{abs_work_dir}\\inputs\\target.zip",
        "--tag", "{abs_mozilla_dir}\\browser\\installer\\windows\\app.tag",
        "--setupexe", "{abs_work_dir}\\inputs\\setup.exe",
        "-o", "{output_home}\\target.installer.exe"
    ], [
        "mar",
        "-i", "{abs_work_dir}\\inputs\\target.zip",
        "--mar", "{abs_work_dir}\\inputs\\mar.exe",
        "-o", "{output_home}\\target.complete.mar"
    ]]

config = {
    "input_home": "{abs_work_dir}\\inputs",
    "output_home": "{base_work_dir}\\public\\build{locale}",

    "locale": os.environ.get("LOCALE"),

    "download_config": download_config,

    "repackage_config": repackage_config,

    # ToolTool
    "tooltool_manifest_src": 'browser\\config\\tooltool-manifests\\{}\\releng.manifest'.format(platform),
    'tooltool_url': 'https://api.pub.build.mozilla.org/tooltool/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),

    'run_configure': False,
}
