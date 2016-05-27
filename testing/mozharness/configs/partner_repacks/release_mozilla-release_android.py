FTP_SERVER = "stage.mozilla.org"
FTP_USER = "ffxbld"
FTP_SSH_KEY = "~/.ssh/ffxbld_rsa"
FTP_UPLOAD_BASE_DIR = "/pub/mozilla.org/mobile/candidates/%(version)s-candidates/build%(buildnum)d"
DOWNLOAD_BASE_URL = "http://%s%s" % (FTP_SERVER, FTP_UPLOAD_BASE_DIR)
APK_BASE_NAME = "fennec-%(version)s.%(locale)s.android-arm.apk"
HG_SHARE_BASE_DIR = "/builds/hg-shared"
KEYSTORE = "/home/cltsign/.android/android-release.keystore"
KEY_ALIAS = "release"

config = {
    "log_name": "partner_repack",
    "locales_file": "buildbot-configs/mozilla/l10n-changesets_mobile-release.json",
    "additional_locales": ['en-US'],
    "platforms": ["android"],
    "repos": [{
        "repo": "https://hg.mozilla.org/build/buildbot-configs",
        "branch": "default",
    }],
    'vcs_share_base': HG_SHARE_BASE_DIR,
    "ftp_upload_base_dir": FTP_UPLOAD_BASE_DIR,
    "ftp_ssh_key": FTP_SSH_KEY,
    "ftp_user": FTP_USER,
    "ftp_server": FTP_SERVER,
    "installer_base_names": {
        "android": APK_BASE_NAME,
    },
    "partner_config": {
        "google-play": {},
    },
    "download_unsigned_base_subdir": "unsigned/%(platform)s/%(locale)s",
    "download_base_url": DOWNLOAD_BASE_URL,

    "release_config_file": "buildbot-configs/mozilla/release-fennec-mozilla-release.py",

    "default_actions": ["clobber", "pull", "download", "repack", "upload-unsigned-bits", "summary"],

    # signing (optional)
    "keystore": KEYSTORE,
    "key_alias": KEY_ALIAS,
    "exes": {
        # This path doesn't exist and this file probably doesn't work
        # Comment out to avoid confusion
#        "jarsigner": "/tools/jdk-1.6.0_17/bin/jarsigner",
        "zipalign": "/tools/android-sdk-r8/tools/zipalign",
    },
}
