#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
import glob
import os
import re
import subprocess
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

# import the guts
import mozharness
from mozharness.base.log import DEBUG, ERROR, FATAL
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import VCSScript
from mozharness.mozilla.tooltool import TooltoolMixin

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    "external_tools",
)


class OpenH264Build(TransferMixin, VCSScript, TooltoolMixin):
    all_actions = [
        "clobber",
        "get-tooltool",
        "checkout-sources",
        "build",
        "test",
        "package",
        "dump-symbols",
    ]

    default_actions = [
        "get-tooltool",
        "checkout-sources",
        "build",
        "package",
        "dump-symbols",
    ]

    config_options = [
        [
            ["--repo"],
            {
                "dest": "repo",
                "help": "OpenH264 repository to use",
                "default": "https://github.com/dminor/openh264.git",
            },
        ],
        [
            ["--rev"],
            {"dest": "revision", "help": "revision to checkout", "default": "master"},
        ],
        [
            ["--debug"],
            {
                "dest": "debug_build",
                "action": "store_true",
                "help": "Do a debug build",
            },
        ],
        [
            ["--arch"],
            {
                "dest": "arch",
                "help": "Arch type to use (x64, x86, arm, or aarch64)",
            },
        ],
        [
            ["--os"],
            {
                "dest": "operating_system",
                "help": "Specify the operating system to build for",
            },
        ],
        [
            ["--branch"],
            {
                "dest": "branch",
                "help": "dummy option",
            },
        ],
        [
            ["--build-pool"],
            {
                "dest": "build_pool",
                "help": "dummy option",
            },
        ],
    ]

    def __init__(
        self,
        require_config_file=False,
        config={},
        all_actions=all_actions,
        default_actions=default_actions,
    ):
        # Default configuration
        default_config = {
            "debug_build": False,
            "upload_ssh_key": "~/.ssh/ffxbld_rsa",
            "upload_ssh_user": "ffxbld",
            "upload_ssh_host": "upload.ffxbld.productdelivery.prod.mozaws.net",
            "upload_path_base": "/tmp/openh264",
        }
        default_config.update(config)

        VCSScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            config=default_config,
            all_actions=all_actions,
            default_actions=default_actions,
        )

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        dirs = super(OpenH264Build, self).query_abs_dirs()
        dirs["abs_upload_dir"] = os.path.join(dirs["abs_work_dir"], "upload")
        self.abs_dirs = dirs
        return self.abs_dirs

    def get_tooltool(self):
        c = self.config
        if not c.get("tooltool_manifest_file"):
            self.info("Skipping tooltool fetching since no tooltool manifest")
            return
        dirs = self.query_abs_dirs()
        self.mkdir_p(dirs["abs_work_dir"])
        manifest = os.path.join(
            dirs["abs_src_dir"],
            "testing",
            "mozharness",
            "configs",
            "openh264",
            "tooltool-manifests",
            c["tooltool_manifest_file"],
        )
        self.info("Getting tooltool files from manifest (%s)" % manifest)
        try:
            self.tooltool_fetch(
                manifest=manifest,
                output_dir=os.path.join(dirs["abs_work_dir"]),
                cache=c.get("tooltool_cache"),
            )
        except KeyError:
            self.error("missing a required key.")

    def query_package_name(self):
        if self.config["arch"] in ("x64", "aarch64"):
            bits = "64"
        else:
            bits = "32"
        version = self.config["revision"]

        if sys.platform in ("linux2", "linux"):
            if self.config.get("operating_system") == "android":
                return "openh264-android-{arch}-{version}.zip".format(
                    version=version, arch=self.config["arch"]
                )
            elif self.config.get("operating_system") == "darwin":
                suffix = ""
                if self.config["arch"] != "x64":
                    suffix = "-" + self.config["arch"]
                return "openh264-macosx{bits}{suffix}-{version}.zip".format(
                    version=version, bits=bits, suffix=suffix
                )
            elif self.config["arch"] == "aarch64":
                return "openh264-linux64-aarch64-{version}.zip".format(version=version)
            else:
                return "openh264-linux{bits}-{version}.zip".format(
                    version=version, bits=bits
                )
        elif sys.platform == "win32":
            if self.config["arch"] == "aarch64":
                return "openh264-win64-aarch64-{version}.zip".format(version=version)
            else:
                return "openh264-win{bits}-{version}.zip".format(
                    version=version, bits=bits
                )
        self.fatal("can't determine platform")

    def query_make_params(self):
        retval = []
        if self.config["debug_build"]:
            retval.append("BUILDTYPE=Debug")

        if self.config["arch"] in ("x64", "aarch64"):
            retval.append("ENABLE64BIT=Yes")
        else:
            retval.append("ENABLE64BIT=No")

        if self.config["arch"] == "x86":
            retval.append("ARCH=x86")
        elif self.config["arch"] == "x64":
            retval.append("ARCH=x86_64")
        elif self.config["arch"] == "aarch64":
            retval.append("ARCH=arm64")
        else:
            self.fatal("Unknown arch: {}".format(self.config["arch"]))

        if "operating_system" in self.config:
            retval.append("OS=%s" % self.config["operating_system"])
            if self.config["operating_system"] == "android":
                retval.append("TARGET=invalid")
                retval.append("NDKLEVEL=%s" % self.config["min_sdk"])
                retval.append("NDKROOT=%s/android-ndk" % os.environ["MOZ_FETCHES_DIR"])
                retval.append("NDK_TOOLCHAIN_VERSION=clang")
            if self.config["operating_system"] == "darwin":
                retval.append("OS=darwin")

        if self._is_windows():
            retval.append("OS=msvc")
            retval.append("CC=clang-cl")
            retval.append("CXX=clang-cl")
            if self.config["arch"] == "aarch64":
                retval.append("CXX_LINK_O=-nologo --target=aarch64-windows-msvc -Fe$@")
        else:
            retval.append("CC=clang")
            retval.append("CXX=clang++")

        return retval

    def query_upload_ssh_key(self):
        return self.config["upload_ssh_key"]

    def query_upload_ssh_host(self):
        return self.config["upload_ssh_host"]

    def query_upload_ssh_user(self):
        return self.config["upload_ssh_user"]

    def query_upload_ssh_path(self):
        return "%s/%s" % (self.config["upload_path_base"], self.config["revision"])

    def run_make(self, target, capture_output=False):
        make = (
            f"{os.environ['MOZ_FETCHES_DIR']}/mozmake/mozmake"
            if sys.platform == "win32"
            else "make"
        )
        cmd = [make, target] + self.query_make_params()
        dirs = self.query_abs_dirs()
        repo_dir = os.path.join(dirs["abs_work_dir"], "openh264")
        env = None
        if self.config.get("partial_env"):
            env = self.query_env(self.config["partial_env"])
        kwargs = dict(cwd=repo_dir, env=env)
        if capture_output:
            return self.get_output_from_command(cmd, **kwargs)
        else:
            return self.run_command(cmd, **kwargs)

    def _git_checkout(self, repo, repo_dir, rev):
        try:
            subprocess.run(["git", "clone", "-q", "--no-checkout", repo, repo_dir])
            subprocess.run(["git", "checkout", "-q", "-f", f"{rev}^0"], cwd=repo_dir)
        except Exception:
            self.rmtree(repo_dir)
            raise
        return True

    def checkout_sources(self):
        repo = self.config["repo"]
        rev = self.config["revision"]

        dirs = self.query_abs_dirs()
        repo_dir = os.path.join(dirs["abs_work_dir"], "openh264")

        if self._is_windows():
            # We don't have git on our windows builders, so download a zip
            # package instead.
            path = repo.replace(".git", "/archive/") + rev + ".zip"
            self.download_file(path)
            self.unzip(rev + ".zip", dirs["abs_work_dir"])
            self.move(
                os.path.join(dirs["abs_work_dir"], "openh264-" + rev),
                os.path.join(dirs["abs_work_dir"], "openh264"),
            )

            # Retrieve in-tree version of gmp-api
            self.copytree(
                os.path.join(dirs["abs_src_dir"], "dom", "media", "gmp", "gmp-api"),
                os.path.join(repo_dir, "gmp-api"),
            )

            # We need gas-preprocessor.pl for arm64 builds
            if self.config["arch"] == "aarch64":
                openh264_dir = os.path.join(dirs["abs_work_dir"], "openh264")
                self.download_file(
                    (
                        "https://raw.githubusercontent.com/libav/"
                        "gas-preprocessor/c2bc63c96678d9739509e58"
                        "7aa30c94bdc0e636d/gas-preprocessor.pl"
                    ),
                    parent_dir=openh264_dir,
                )
                self.chmod(os.path.join(openh264_dir, "gas-preprocessor.pl"), 744)

                # gas-preprocessor.pl expects cpp to exist
                # os.symlink is not available on Windows until we switch to
                # Python 3.
                os.system(
                    "ln -s %s %s"
                    % (
                        os.path.join(
                            os.environ["MOZ_FETCHES_DIR"], "clang", "bin", "clang.exe"
                        ),
                        os.path.join(openh264_dir, "cpp"),
                    )
                )
            return 0

        self.retry(
            self._git_checkout,
            error_level=FATAL,
            error_message="Automation Error: couldn't clone repo",
            args=(repo, repo_dir, rev),
        )

        # Checkout gmp-api
        # TODO: Nothing here updates it yet, or enforces versions!
        if not os.path.exists(os.path.join(repo_dir, "gmp-api")):
            retval = self.run_make("gmp-bootstrap")
            if retval != 0:
                self.fatal("couldn't bootstrap gmp")
        else:
            self.info("skipping gmp bootstrap - we have it locally")

        # Checkout gtest
        # TODO: Requires svn!
        if not os.path.exists(os.path.join(repo_dir, "gtest")):
            retval = self.run_make("gtest-bootstrap")
            if retval != 0:
                self.fatal("couldn't bootstrap gtest")
        else:
            self.info("skipping gtest bootstrap - we have it locally")

        return retval

    def build(self):
        retval = self.run_make("plugin")
        if retval != 0:
            self.fatal("couldn't build plugin")

    def package(self):
        dirs = self.query_abs_dirs()
        srcdir = os.path.join(dirs["abs_work_dir"], "openh264")
        package_name = self.query_package_name()
        package_file = os.path.join(dirs["abs_work_dir"], package_name)
        if os.path.exists(package_file):
            os.unlink(package_file)
        to_package = []
        for f in glob.glob(os.path.join(srcdir, "*gmpopenh264*")):
            if not re.search(
                "(?:lib)?gmpopenh264(?!\.\d)\.(?:dylib|so|dll|info)(?!\.\d)", f
            ):
                # Don't package unnecessary zip bloat
                # Blocks things like libgmpopenh264.2.dylib and libgmpopenh264.so.1
                self.log("Skipping packaging of {package}".format(package=f))
                continue
            to_package.append(os.path.basename(f))
        self.log("Packaging files %s" % to_package)
        cmd = ["zip", package_file] + to_package
        retval = self.run_command(cmd, cwd=srcdir)
        if retval != 0:
            self.fatal("couldn't make package")
        self.copy_to_upload_dir(
            package_file, dest=os.path.join(srcdir, "artifacts", package_name)
        )

        # Taskcluster expects this path to exist, but we don't use it
        # because our builds are private.
        path = os.path.join(
            self.query_abs_dirs()["abs_work_dir"], "..", "public", "build"
        )
        self.mkdir_p(path)

    def dump_symbols(self):
        dirs = self.query_abs_dirs()
        c = self.config
        srcdir = os.path.join(dirs["abs_work_dir"], "openh264")
        package_name = self.run_make("echo-plugin-name", capture_output=True)
        if not package_name:
            self.fatal("failure running make")
        zip_package_name = self.query_package_name()
        if not zip_package_name[-4:] == ".zip":
            self.fatal("Unexpected zip_package_name")
        symbol_package_name = "{base}.symbols.zip".format(base=zip_package_name[:-4])
        symbol_zip_path = os.path.join(srcdir, "artifacts", symbol_package_name)
        repo_dir = os.path.join(dirs["abs_work_dir"], "openh264")
        env = None
        if self.config.get("partial_env"):
            env = self.query_env(self.config["partial_env"])
        kwargs = dict(cwd=repo_dir, env=env)
        dump_syms = os.path.join(dirs["abs_work_dir"], c["dump_syms_binary"])
        self.chmod(dump_syms, 0o755)
        python = self.query_exe("python3")
        cmd = [
            python,
            os.path.join(external_tools_path, "packagesymbols.py"),
            "--symbol-zip",
            symbol_zip_path,
            dump_syms,
            os.path.join(srcdir, package_name),
        ]
        self.run_command(cmd, **kwargs)

    def test(self):
        retval = self.run_make("test")
        if retval != 0:
            self.fatal("test failures")

    def copy_to_upload_dir(
        self,
        target,
        dest=None,
        log_level=DEBUG,
        error_level=ERROR,
        compress=False,
        upload_dir=None,
    ):
        """Copy target file to upload_dir/dest.

        Potentially update a manifest in the future if we go that route.

        Currently only copies a single file; would be nice to allow for
        recursive copying; that would probably done by creating a helper
        _copy_file_to_upload_dir().
        """
        dest_filename_given = dest is not None
        if upload_dir is None:
            upload_dir = self.query_abs_dirs()["abs_upload_dir"]
        if dest is None:
            dest = os.path.basename(target)
        if dest.endswith("/"):
            dest_file = os.path.basename(target)
            dest_dir = os.path.join(upload_dir, dest)
            dest_filename_given = False
        else:
            dest_file = os.path.basename(dest)
            dest_dir = os.path.join(upload_dir, os.path.dirname(dest))
        if compress and not dest_filename_given:
            dest_file += ".gz"
        dest = os.path.join(dest_dir, dest_file)
        if not os.path.exists(target):
            self.log("%s doesn't exist!" % target, level=error_level)
            return None
        self.mkdir_p(dest_dir)
        self.copyfile(target, dest, log_level=log_level, compress=compress)
        if os.path.exists(dest):
            return dest
        else:
            self.log("%s doesn't exist after copy!" % dest, level=error_level)
            return None


# main {{{1
if __name__ == "__main__":
    myScript = OpenH264Build()
    myScript.run_and_exit()
