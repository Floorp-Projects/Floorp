# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""module for tooltool operations"""
import os
import sys

from mozharness.base.errors import PythonErrorList
from mozharness.base.log import ERROR, FATAL

TooltoolErrorList = PythonErrorList + [{"substr": "ERROR - ", "level": ERROR}]


_here = os.path.abspath(os.path.dirname(__file__))
_external_tools_path = os.path.normpath(
    os.path.join(_here, "..", "..", "external_tools")
)
_mozbuild_action_path = os.path.normpath(
    os.path.join(
        _here, "..", "..", "..", "..", "python", "mozbuild", "mozbuild", "action"
    )
)
_mozbuild_action_abspath = os.path.join("python", "mozbuild", "mozbuild", "action")


class TooltoolMixin(object):
    """Mixin class for handling tooltool manifests.
    To use a tooltool server other than the Mozilla server, set
    TOOLTOOL_HOST in the environment.
    """

    def tooltool_fetch(self, manifest, output_dir=None, privileged=False, cache=None):
        """docstring for tooltool_fetch"""
        if cache is None:
            cache = os.environ.get("TOOLTOOL_CACHE")

        for d in (output_dir, cache):
            if d is not None and not os.path.exists(d):
                self.mkdir_p(d)
        if self.topsrcdir:
            cmd = [
                sys.executable,
                "-u",
                os.path.join(self.topsrcdir, "mach"),
                "artifact",
                "toolchain",
                "-v",
            ]
        else:
            # In CI, tooltool.py is expected in external_tools; in local runs,
            # tooltool.py is expected in mozbuild, but mozbuild might be hard
            # to find, expecially if topsrcdir could not be found...
            locations = [
                os.path.join(_external_tools_path, "tooltool.py"),
                os.path.join(_mozbuild_action_path, "tooltool.py"),
                os.path.join(
                    os.environ.get("GECKO_PATH", "."),
                    _mozbuild_action_abspath,
                    "tooltool.py",
                ),
            ]
            tooltool_path = [p for p in locations if os.path.exists(p)]
            if not tooltool_path:
                raise Exception("unable to find tooltool client file tooltool.py")
            cmd = [
                sys.executable,
                "-u",
                tooltool_path[0],
            ]

        if self.topsrcdir:
            cmd.extend(["--tooltool-manifest", manifest])
            cmd.extend(
                ["--artifact-manifest", os.path.join(self.topsrcdir, "toolchains.json")]
            )
        else:
            cmd.extend(["fetch", "-m", manifest, "-o"])

        if cache:
            cmd.extend(["--cache-dir" if self.topsrcdir else "-c", cache])

        timeout = self.config.get("tooltool_timeout", 10 * 60)

        self.retry(
            self.run_command,
            args=(cmd,),
            kwargs={
                "cwd": output_dir,
                "error_list": TooltoolErrorList,
                "privileged": privileged,
                "output_timeout": timeout,
            },
            good_statuses=(0,),
            error_message="Tooltool %s fetch failed!" % manifest,
            error_level=FATAL,
        )

    def create_tooltool_manifest(self, contents, path=None):
        """Currently just creates a manifest, given the contents.
        We may want a template and individual values in the future?
        """
        if path is None:
            dirs = self.query_abs_dirs()
            path = os.path.join(dirs["abs_work_dir"], "tooltool.tt")
        self.write_to_file(path, contents, error_level=FATAL)
        return path
