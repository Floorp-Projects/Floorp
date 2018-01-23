import os

platform = "win32"

download_config = {
        "target.zip": os.environ.get("SIGNED_ZIP"),
        "setup.exe": os.environ.get("SIGNED_SETUP"),
        "mar.exe": os.environ.get("UNSIGNED_MAR"),
        # stub installer is only built on win32
        "setup-stub.exe": os.environ.get("SIGNED_SETUP_STUB"),
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
    ], [
        "installer",
        "--tag", "{abs_mozilla_dir}\\browser\\installer\\windows\\stub.tag",
         "--setupexe", "{abs_work_dir}\\inputs\\setup-stub.exe",
         "-o", "{output_home}\\target.stub-installer.exe"
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
