#!/usr/bin/env python

import sys
import os
import re
import signal

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

# import the guts
from mozharness.base.script import BaseScript
from mozharness.base.python import VirtualenvMixin
from mozharness.base.script import ScriptMixin


class GetAPK(BaseScript, VirtualenvMixin):
    all_actions = [
        'create-virtualenv',
        "test",
        'download-apk'
    ]

    default_actions = [
        'create-virtualenv',
        'test'
    ]

    config_options = [
        [["--build"], {
            "dest": "build",
            "help": "Specify build number (default 1)",
            "default": "1"
        }],
        [["--version"], {
            "dest": "version",
            "help": "Specify version number to download (e.g. 23.0b7)",
            "default": "None"
        }],
        [["--arch"], {
            "dest": "arch",
            "help": "Specify which architecture to get the apk for",
            "default": "all"
        }],
        [["--locale"], {
            "dest": "locale",
            "help": "Specify which locale to get the apk for",
            "default": "multi"
        }],
        [["--clean"], {
            "dest": "clean",
            "help": "Use this option to clean the download directory",
            "action": "store_true",
            "default": False
        }]
    ]

    arch_values = ["arm", "x86"]
    multi_api_archs = ["arm"]
    multi_apis = ["api-15"] # v11 has been dropped in fx 46 (1155801)
    # v9 has been dropped in fx 48 (1220184)

    download_dir = "apk-download"

    apk_ext = ".apk"
    checksums_ext = ".checksums"
    android_prefix = "android-"

    # Cleanup half downloaded files on Ctrl+C
    def signal_handler(self, signal, frame):
        print("You pressed Ctrl+C!")
        self.cleanup()
        sys.exit(1)

    def cleanup(self):
        ScriptMixin.rmtree(self, self.download_dir)
        self.info("Download directory cleaned")

    def __init__(self, require_config_file=False, config={},
                 all_actions=all_actions,
                 default_actions=default_actions):
        default_config = {
            # the path inside the work_dir ('build') of where we will install the env.
            # pretty sure it's the default and not needed.
            'virtualenv_path': 'venv',
        }

        default_config.update(config)

        BaseScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            config=default_config,
            all_actions=all_actions,
        )

    # Gets called once download is complete
    def download_complete(self, apk_file, checksum_file):
        self.info(apk_file + " has been downloaded successfully")
        ScriptMixin.rmtree(self, checksum_file)

    # Called if download fails due to 404 error or some other connection failure
    def download_error(self):
        self.cleanup()
        self.fatal("Download failed!")

    # Check the given values are correct
    def check_argument(self):
        if self.config["clean"]:
            self.cleanup()
        if self.config["version"] == "None":
            if self.config["clean"]:
                sys.exit(0)
            self.fatal("Version is required")

        if self.config["arch"] not in self.arch_values and not self.config["arch"] == "all":
            error = self.config["arch"] + " is not a valid arch.  " \
                                          "Try one of the following:"+os.linesep
            for arch in self.arch_values:
                error += arch + os.linesep
            error += "Or don't use the --arch option to download all the archs"
            self.fatal(error)

    # Checksum check the APK
    def check_apk(self, apk_file, checksum_file):
        self.info("The checksum for the APK is being checked....")
        checksum = ScriptMixin.read_from_file(self, checksum_file, False)
        checksum = re.sub("\s(.*)", "", checksum.splitlines()[0])

        apk_checksum = self.file_sha512sum(apk_file)

        if checksum == apk_checksum:
            self.info("APK checksum check succeeded!")
            self.download_complete(apk_file, checksum_file)
        else:
            ScriptMixin.rmtree(self, self.download_dir)
            self.fatal("Downloading " + apk_file + " failed!")

    # Helper functions
    def generate_url(self, version, build, locale, api_suffix, arch_file):
        return "https://ftp.mozilla.org/pub/mozilla.org/mobile/candidates/" + version + "-candidates/build" + build + \
               "/" + self.android_prefix + api_suffix + "/" + locale + "/fennec-" + version + "." + locale + \
               "." + self.android_prefix + arch_file

    def get_api_suffix(self, arch):
        if arch in self.multi_api_archs:
            return self.multi_apis
        else:
            return [arch]

    def get_arch_file(self, arch):
        if arch == "x86":
            # the filename contains i386 instead of x86
            return "i386"
        else:
            return arch

    def get_common_file_name(self, version, locale):
        return "fennec-" + version + "." + locale + "." + self.android_prefix

    # Function that actually downloads the file
    def download(self, version, build, arch, locale):
        ScriptMixin.mkdir_p(self, self.download_dir)

        common_filename = self.get_common_file_name(version, locale)
        arch_file = self.get_arch_file(arch)

        for api_suffix in self.get_api_suffix(arch):
            url = self.generate_url(version, build, locale, api_suffix, arch_file)
            apk_url = url + self.apk_ext
            checksum_url = url + self.checksums_ext
            if arch in self.multi_api_archs:
                filename = common_filename + arch_file + "-" + api_suffix
            else:
                filename = common_filename + arch_file

            filename_apk = os.path.join(self.download_dir, filename + self.apk_ext)
            filename_checksums = os.path.join(self.download_dir, filename + self.checksums_ext)

            retry_config = {'attempts': 1, 'cleanup': self.download_error}
            ScriptMixin.download_file(self, apk_url, filename_apk, retry_config=retry_config)

            retry_config = {'attempts': 1, 'cleanup': self.download_error}
            ScriptMixin.download_file(self, checksum_url, filename_checksums, retry_config=retry_config)

            self.check_apk(filename_apk, filename_checksums)

    # Download all the archs if none is given
    def download_all(self, version, build, locale):
        for arch in self.arch_values:
            self.download(version, build, arch, locale)

    # Download apk initial action
    def download_apk(self):
        self.check_argument()
        version = self.config["version"]
        arch = self.config["arch"]
        build = str(self.config["build"])
        locale = self.config["locale"]

        self.info("Downloading version " + version + " build #" + build
                  + " for arch " + arch + " (locale " + locale + ")")
        if arch == "all":
            self.download_all(version, build, locale)
        else:
            self.download(version, build, arch, locale)

    # Test the helper, cleanup and check functions
    def test(self):
        ScriptMixin.mkdir_p(self, self.download_dir)
        testfile = os.path.join(self.download_dir, "testfile")
        testchecksums = os.path.join(self.download_dir, "testchecksums")
        ScriptMixin.write_to_file(self, testfile, "This is a test file!")
        ScriptMixin.write_to_file(self, testchecksums, self.file_sha512sum(testfile))
        self.check_apk(testfile, testchecksums)
        self.download_complete(testfile, testchecksums)
        if os.path.isfile(testchecksums):
            self.fatal("download_complete test failed!")

        self.cleanup()
        if os.path.isdir(self.download_dir):
            self.fatal("cleanup test failed")

        url = self.generate_url("43.0", "2", "multi", "x86", "i386")
        correcturl = "https://ftp.mozilla.org/pub/mozilla.org/mobile/candidates/43.0-candidates/build2/"\
                     + self.android_prefix + "x86/multi/fennec-43.0.multi." + self.android_prefix + "i386"
        if not url == correcturl:
            self.fatal("get_url test failed!")

        if not self.get_api_suffix(self.multi_api_archs[0]) == self.multi_apis:
            self.fatal("get_api_suffix test failed!")

        if not self.get_arch_file("x86") == "i386":
            self.fatal("get_arch_file test failed!")

        if not self.get_common_file_name("43.0", "multi") == "fennec-43.0.multi." + self.android_prefix:
            self.fatal("get_common_file_name test failed!")

# main {{{1
if __name__ == '__main__':
    myScript = GetAPK()
    signal.signal(signal.SIGINT, myScript.signal_handler)
    myScript.run_and_exit()
