# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import stat

try:
    from shutil import which
except ImportError:
    from shutil_which import which

from mozbuild.util import mkdir
import mozpack.path as mozpath

from mozbuild.action.tooltool import unpack_file
from mozbuild.artifact_cache import ArtifactCache

from mozperftest.utils import host_platform


AUTOMATION = "MOZ_AUTOMATION" in os.environ


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


def system_prerequisites(state_path, artifact_cache_path, log, info):
    """Install browsertime and visualmetrics.py prerequisites.
    """
    if not AUTOMATION and host_platform().startswith("linux"):
        # On Linux ImageMagick needs to be installed manually, and `mach bootstrap` doesn't
        # do that (yet).  Provide some guidance.
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
    artifact_cache = ArtifactCache(artifact_cache_path, log=log, skip_cache=False)

    fetches = host_fetches[host_platform()]
    for tool, fetch in sorted(fetches.items()):
        archive = artifact_cache.fetch(fetch["url"])
        # TODO: assert type, verify sha256 (and size?).

        if fetch.get("unpack", True):
            cwd = os.getcwd()
            try:
                mkdir(state_path)
                os.chdir(state_path)
                info("Unpacking temporary location {path}", path=archive)

                if "win64" in host_platform() and "imagemagick" in tool.lower():
                    # Windows archive does not contain a subfolder
                    # so we make one for it here
                    mkdir(fetch.get("path"))
                    os.chdir(os.path.join(state_path, fetch.get("path")))
                    unpack_file(archive)
                    os.chdir(state_path)
                else:
                    unpack_file(archive)

                # Make sure the expected path exists after extraction
                path = os.path.join(state_path, fetch.get("path"))
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
                        "Could not set executable bit in %s, error: %s" % (path, str(e))
                    )
            finally:
                os.chdir(cwd)


def append_system_env(env, state_path, append_path=True):
    fetches = host_fetches[host_platform()]

    # Ensure that bare `ffmpeg` and ImageMagick commands
    # {`convert`,`compare`,`mogrify`} are found.  The `visualmetrics.py`
    # script doesn't take these as configuration, so we do this (for now).
    # We should update the script itself to accept this configuration.
    path = env.get("PATH", "").split(os.pathsep)
    path_to_ffmpeg = mozpath.join(state_path, fetches["ffmpeg"]["path"])

    path_to_imagemagick = None
    if "ImageMagick" in fetches:
        path_to_imagemagick = mozpath.join(state_path, fetches["ImageMagick"]["path"])

    if path_to_imagemagick:
        # ImageMagick ships ffmpeg (on Windows, at least) so we
        # want to ensure that our ffmpeg goes first, just in case.
        path.insert(
            0,
            state_path
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
