import os

config = {
    "package-name": "firefox",
    "installer-tag": "browser/installer/windows/app.tag",
    "stub-installer-tag": "browser/installer/windows/stub.tag",
    "wsx-stub": "browser/installer/windows/msi/installer.wxs",
    "fetch-dir": os.environ.get('MOZ_FETCHES_DIR'),
}
