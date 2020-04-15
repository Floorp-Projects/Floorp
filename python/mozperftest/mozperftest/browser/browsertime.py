# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import collections
import json
import os
import pathlib
import stat
import sys
import re
import shutil

from mozbuild.util import mkdir
import mozpack.path as mozpath

from mozperftest.browser.noderunner import NodeRunner
from mozperftest.utils import host_platform


AUTOMATION = "MOZ_AUTOMATION" in os.environ
BROWSERTIME_SRC_ROOT = os.path.dirname(__file__)
PILLOW_VERSION = "6.0.0"
PYSSIM_VERSION = "0.4"

# Map from `host_platform()` to a `fetch`-like syntax.
host_fetches = {
    "darwin": {
        "ffmpeg": {
            "type": "static-url",
            "url": "https://github.com/ncalexan/geckodriver/releases/download/v0.24.0-android/ffmpeg-4.1.1-macos64-static.zip",  # noqa
            # An extension to `fetch` syntax.
            "path": "ffmpeg-4.1.1-macos64-static",
        }
    },
    "linux64": {
        "ffmpeg": {
            "type": "static-url",
            "url": "https://github.com/ncalexan/geckodriver/releases/download/v0.24.0-android/ffmpeg-4.1.4-i686-static.tar.xz",  # noqa
            # An extension to `fetch` syntax.
            "path": "ffmpeg-4.1.4-i686-static",
        },
        # TODO: install a static ImageMagick.  All easily available binaries are
        # not statically linked, so they will (mostly) fail at runtime due to
        # missing dependencies.  For now we require folks to install ImageMagick
        # globally with their package manager of choice.
    },
    "win64": {
        "ffmpeg": {
            "type": "static-url",
            "url": "https://github.com/ncalexan/geckodriver/releases/download/v0.24.0-android/ffmpeg-4.1.1-win64-static.zip",  # noqa
            # An extension to `fetch` syntax.
            "path": "ffmpeg-4.1.1-win64-static",
        },
        "ImageMagick": {
            "type": "static-url",
            # 'url': 'https://imagemagick.org/download/binaries/ImageMagick-7.0.8-39-portable-Q16-x64.zip',  # noqa
            # imagemagick.org doesn't keep old versions; the mirror below does.
            "url": "https://ftp.icm.edu.pl/packages/ImageMagick/binaries/ImageMagick-7.0.8-39-portable-Q16-x64.zip",  # noqa
            # An extension to `fetch` syntax.
            "path": "ImageMagick-7.0.8",
        },
    },
}


class BrowsertimeRunner(NodeRunner):
    def __init__(self, mach_cmd):
        super(BrowsertimeRunner, self).__init__(mach_cmd)
        self.topsrcdir = mach_cmd.topsrcdir
        self._mach_context = mach_cmd._mach_context
        self.virtualenv_manager = mach_cmd.virtualenv_manager
        self.proxy = None
        self._created_dirs = []

    @property
    def artifact_cache_path(self):
        """Downloaded artifacts will be kept here."""
        # The convention is $MOZBUILD_STATE_PATH/cache/$FEATURE.
        return mozpath.join(self._mach_context.state_dir, "cache", "browsertime")

    @property
    def state_path(self):
        """Unpacked artifacts will be kept here."""
        # The convention is $MOZBUILD_STATE_PATH/$FEATURE.
        res = mozpath.join(self._mach_context.state_dir, "browsertime")
        mkdir(res)
        return res

    @property
    def browsertime_js(self):
        root = os.environ.get("BROWSERTIME", self.state_path)
        return os.path.join(
            root, "node_modules", "browsertime", "bin", "browsertime.js"
        )

    def setup_prerequisites(self):
        """Install browsertime and visualmetrics.py prerequisites.
        """

        from mozbuild.action.tooltool import unpack_file
        from mozbuild.artifact_cache import ArtifactCache

        if not AUTOMATION and host_platform().startswith("linux"):
            # On Linux ImageMagick needs to be installed manually, and `mach bootstrap` doesn't
            # do that (yet).  Provide some guidance.
            try:
                from shutil import which
            except ImportError:
                from shutil_which import which

            im_programs = ("compare", "convert", "mogrify")
            for im_program in im_programs:
                prog = which(im_program)
                if not prog:
                    print(
                        "Error: On Linux, ImageMagick must be on the PATH. "
                        "Install ImageMagick manually and try again (or update PATH). "
                        "On Ubuntu and Debian, try `sudo apt-get install imagemagick`. "
                        "On Fedora, try `sudo dnf install imagemagick`. "
                        "On CentOS, try `sudo yum install imagemagick`."
                    )
                    return 1

        # Download the visualmetrics.py requirements.
        artifact_cache = ArtifactCache(
            self.artifact_cache_path, log=self.log, skip_cache=False
        )

        fetches = host_fetches[host_platform()]
        for tool, fetch in sorted(fetches.items()):
            archive = artifact_cache.fetch(fetch["url"])
            # TODO: assert type, verify sha256 (and size?).

            if fetch.get("unpack", True):
                cwd = os.getcwd()
                try:
                    mkdir(self.state_path)
                    os.chdir(self.state_path)
                    self.info("Unpacking temporary location {path}", path=archive)

                    if "win64" in host_platform() and "imagemagick" in tool.lower():
                        # Windows archive does not contain a subfolder
                        # so we make one for it here
                        mkdir(fetch.get("path"))
                        os.chdir(os.path.join(self.state_path, fetch.get("path")))
                        unpack_file(archive)
                        os.chdir(self.state_path)
                    else:
                        unpack_file(archive)

                    # Make sure the expected path exists after extraction
                    path = os.path.join(self.state_path, fetch.get("path"))
                    if not os.path.exists(path):
                        raise Exception("Cannot find an extracted directory: %s" % path)

                    try:
                        # Some archives provide binaries that don't have the
                        # executable bit set so we need to set it here
                        for root, dirs, files in os.walk(path):
                            for edir in dirs:
                                loc_to_change = os.path.join(root, edir)
                                st = os.stat(loc_to_change)
                                os.chmod(loc_to_change, st.st_mode | stat.S_IEXEC)
                            for efile in files:
                                loc_to_change = os.path.join(root, efile)
                                st = os.stat(loc_to_change)
                                os.chmod(loc_to_change, st.st_mode | stat.S_IEXEC)
                    except Exception as e:
                        raise Exception(
                            "Could not set executable bit in %s, error: %s"
                            % (path, str(e))
                        )
                finally:
                    os.chdir(cwd)

    def _need_install(self, package):
        from pip._internal.req.constructors import install_req_from_line

        req = install_req_from_line("Pillow")
        req.check_if_exists(use_user_site=False)
        if req.satisfied_by is None:
            return True
        venv_site_lib = os.path.abspath(
            os.path.join(self.virtualenv_manager.bin_path, "..", "lib")
        )
        site_packages = os.path.abspath(req.satisfied_by.location)
        return not site_packages.startswith(venv_site_lib)

    def setup(self, should_clobber=False, new_upstream_url=""):
        """Install browsertime and visualmetrics.py prerequisites and the Node.js package.
        """
        super(BrowsertimeRunner, self).setup()

        # installing Python deps on the fly
        for dep in ("Pillow==%s" % PILLOW_VERSION, "pyssim==%s" % PYSSIM_VERSION):
            if self._need_install(dep):
                self.virtualenv_manager._run_pip(["install", dep])

        # check if the browsertime package has been deployed correctly
        # for this we just check for the browsertime directory presence
        if os.path.exists(self.browsertime_js):
            return

        sys.path.append(mozpath.join(self.topsrcdir, "tools", "lint", "eslint"))
        import setup_helper

        if not new_upstream_url:
            self.setup_prerequisites()

        # preparing ~/.mozbuild/browsertime
        for file in ("package.json", "package-lock.json"):
            src = mozpath.join(BROWSERTIME_SRC_ROOT, file)
            target = mozpath.join(self.state_path, file)
            if not os.path.exists(target):
                shutil.copyfile(src, target)

        package_json_path = mozpath.join(self.state_path, "package.json")

        if new_upstream_url:
            self.info(
                "Updating browsertime node module version in {package_json_path} "
                "to {new_upstream_url}",
                new_upstream_url=new_upstream_url,
                package_json_path=package_json_path,
            )

            if not re.search("/tarball/[a-f0-9]{40}$", new_upstream_url):
                raise ValueError(
                    "New upstream URL does not end with /tarball/[a-f0-9]{40}: '{}'".format(
                        new_upstream_url
                    )
                )

            with open(package_json_path) as f:
                existing_body = json.loads(
                    f.read(), object_pairs_hook=collections.OrderedDict
                )

            existing_body["devDependencies"]["browsertime"] = new_upstream_url

            updated_body = json.dumps(existing_body)

            with open(package_json_path, "w") as f:
                f.write(updated_body)

        # Install the browsertime Node.js requirements.
        if not setup_helper.check_node_executables_valid():
            return

        # To use a custom `geckodriver`, set
        # os.environ[b"GECKODRIVER_BASE_URL"] = bytes(url)
        # to an endpoint with binaries named like
        # https://github.com/sitespeedio/geckodriver/blob/master/install.js#L31.
        if AUTOMATION:
            os.environ["CHROMEDRIVER_SKIP_DOWNLOAD"] = "true"
            os.environ["GECKODRIVER_SKIP_DOWNLOAD"] = "true"

        self.info(
            "Installing browsertime node module from {package_json}",
            package_json=package_json_path,
        )
        setup_helper.package_setup(
            self.state_path,
            "browsertime",
            should_update=new_upstream_url != "",
            should_clobber=should_clobber,
            no_optional=new_upstream_url or AUTOMATION,
        )

    def append_env(self, append_path=True):
        env = super(BrowsertimeRunner, self).append_env(append_path)
        fetches = host_fetches[host_platform()]

        # Ensure that bare `ffmpeg` and ImageMagick commands
        # {`convert`,`compare`,`mogrify`} are found.  The `visualmetrics.py`
        # script doesn't take these as configuration, so we do this (for now).
        # We should update the script itself to accept this configuration.
        path = env.get("PATH", "").split(os.pathsep)
        path_to_ffmpeg = mozpath.join(self.state_path, fetches["ffmpeg"]["path"])

        path_to_imagemagick = None
        if "ImageMagick" in fetches:
            path_to_imagemagick = mozpath.join(
                self.state_path, fetches["ImageMagick"]["path"]
            )

        if path_to_imagemagick:
            # ImageMagick ships ffmpeg (on Windows, at least) so we
            # want to ensure that our ffmpeg goes first, just in case.
            path.insert(
                0,
                self.state_path
                if host_platform().startswith("win")
                else mozpath.join(path_to_imagemagick, "bin"),
            )  # noqa
        path.insert(
            0,
            path_to_ffmpeg
            if host_platform().startswith("linux")
            else mozpath.join(path_to_ffmpeg, "bin"),
        )  # noqa

        # On windows, we need to add the ImageMagick directory to the path
        # otherwise compare won't be found, and the built-in OS convert
        # method will be used instead of the ImageMagick one.
        if "win64" in host_platform() and path_to_imagemagick:
            # Bug 1596237 - In the windows ImageMagick distribution, the ffmpeg
            # binary is directly located in the root directory, so here we
            # insert in the 3rd position to avoid taking precedence over ffmpeg
            path.insert(2, path_to_imagemagick)

        # On macOs, we can't install our own ImageMagick because the
        # System Integrity Protection (SIP) won't let us set DYLD_LIBRARY_PATH
        # unless we deactivate SIP with "csrutil disable".
        # So we're asking the user to install it.
        #
        # if ImageMagick was installed via brew, we want to make sure we
        # include the PATH
        if host_platform() == "darwin":
            for p in os.environ["PATH"].split(os.pathsep):
                p = p.strip()
                if not p or p in path:
                    continue
                path.append(p)

        if path_to_imagemagick:
            env.update(
                {
                    # See https://imagemagick.org/script/download.php.
                    # Harmless on other platforms.
                    "LD_LIBRARY_PATH": mozpath.join(path_to_imagemagick, "lib"),
                    "DYLD_LIBRARY_PATH": mozpath.join(path_to_imagemagick, "lib"),
                    "MAGICK_HOME": path_to_imagemagick,
                }
            )

        return env

    def extra_default_args(self, args=[]):
        # Add Mozilla-specific default arguments.  This is tricky because browsertime is quite
        # loose about arguments; repeat arguments are generally accepted but then produce
        # difficult to interpret type errors.

        def extract_browser_name(args):
            "Extracts the browser name if any"
            # These are BT arguments, it's BT job to check them
            # here we just want to extract the browser name
            res = re.findall("(--browser|-b)[= ]([\w]+)", " ".join(args))
            if res == []:
                return None
            return res[0][-1]

        def matches(args, *flags):
            "Return True if any argument matches any of the given flags (maybe with an argument)."
            for flag in flags:
                if flag in args or any(arg.startswith(flag + "=") for arg in args):
                    return True
            return False

        extra_args = []

        # Default to Firefox.  Override with `-b ...` or `--browser=...`.
        specifies_browser = matches(args, "-b", "--browser")
        if not specifies_browser:
            extra_args.extend(("-b", "firefox"))

        # Default to not collect HAR.  Override with `--skipHar=false`.
        specifies_har = matches(args, "--har", "--skipHar", "--gzipHar")
        if not specifies_har:
            extra_args.append("--skipHar")

        if not matches(args, "--android"):
            # If --firefox.binaryPath is not specified, default to the objdir binary
            # Note: --firefox.release is not a real browsertime option, but it will
            #       silently ignore it instead and default to a release installation.
            specifies_binaryPath = matches(
                args,
                "--firefox.binaryPath",
                "--firefox.release",
                "--firefox.nightly",
                "--firefox.beta",
                "--firefox.developer",
            )

            if not specifies_binaryPath:
                specifies_binaryPath = extract_browser_name(args) == "chrome"

            if not specifies_binaryPath:
                try:
                    extra_args.extend(("--firefox.binaryPath", self.get_binary_path()))
                except Exception:
                    print(
                        "Please run |./mach build| "
                        "or specify a Firefox binary with --firefox.binaryPath."
                    )
                    return 1

        if extra_args:
            self.debug(
                "Running browsertime with extra default arguments: {extra_args}",
                extra_args=extra_args,
            )

        return extra_args

    def get_profile(self, metadata):
        # XXX we'll use conditioned profiles
        from mozprofile import create_profile

        profile = create_profile(app="firefox")
        prefs = metadata.get_browser_prefs()
        profile.set_preferences(prefs)
        self.info("Created profile at %s" % profile.profile)
        self._created_dirs.append(profile.profile)
        return profile

    def __call__(self, metadata):
        # keep the object around
        # see https://bugzilla.mozilla.org/show_bug.cgi?id=1625118
        profile = self.get_profile(metadata)
        test_script = metadata.get_arg("tests")[0]
        output = metadata.get_arg("output")
        if output is not None:
            p = pathlib.Path(output)
            p = p / "browsertime-results"
            result_dir = str(p.resolve())
        else:
            result_dir = os.path.join(
                self.topsrcdir, "artifacts", "browsertime-results"
            )
        if not os.path.exists(result_dir):
            os.makedirs(result_dir, exist_ok=True)

        args = [
            "--resultDir",
            result_dir,
            "--firefox.profileTemplate",
            profile.profile,
            "-vvv",
            "--iterations",
            "1",
            test_script,
        ]

        extra_options = metadata.get_arg("extra_options")
        if extra_options:
            for option in extra_options.split(","):
                option = option.strip()
                if not option:
                    continue
                option = option.split("=")
                if len(option) != 2:
                    continue
                name, value = option
                args += ["--" + name, value]

        firefox_args = ["--firefox.developer"]
        extra = self.extra_default_args(args=firefox_args)
        command = [self.browsertime_js] + extra + args
        self.info("Running browsertime with this command %s" % " ".join(command))
        self.node(command)
        metadata.set_result(result_dir)
        return metadata

    def teardown(self):
        for dir in self._created_dirs:
            if os.path.exists(dir):
                shutil.rmtree(dir)
