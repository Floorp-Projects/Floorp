from __future__ import absolute_import, unicode_literals

import inspect
import os
import re
import subprocess
import sys
import textwrap

import six

import virtualenv


def bootstrap():
    print("startup")
    import subprocess
    import os

    # noinspection PyUnusedLocal
    def extend_parser(opt_parse_parser):
        print("extend parser with count")
        opt_parse_parser.add_option("-c", action="count", dest="count", default=0, help="Count number of times passed")

    # noinspection PyUnusedLocal
    def adjust_options(options, args):
        print("adjust options")
        options.count += 1

    # noinspection PyUnusedLocal
    def after_install(options, home_dir):
        print("after install {} with options {}".format(home_dir, options.count))
        # noinspection PyUnresolvedReferences
        _, _, _, bin_dir = path_locations(home_dir)  # noqa: F821
        # noinspection PyUnresolvedReferences
        print(
            "exe at {}".format(
                subprocess.check_output(
                    [os.path.join(bin_dir, EXPECTED_EXE), "-c", "import sys; print(sys.executable)"],  # noqa: F821
                    universal_newlines=True,
                )
            )
        )


def test_bootstrap(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)

    extra_code = inspect.getsource(bootstrap)
    extra_code = textwrap.dedent(extra_code[extra_code.index("\n") + 1 :])

    output = virtualenv.create_bootstrap_script(extra_code)
    assert extra_code in output
    if six.PY2:
        output = output.decode()
    write_at = tmp_path / "blog-bootstrap.py"
    write_at.write_text(output)

    try:
        monkeypatch.chdir(tmp_path)
        cmd = [
            sys.executable,
            str(write_at),
            "--no-download",
            "--no-pip",
            "--no-wheel",
            "--no-setuptools",
            "-ccc",
            "-qqq",
            "env",
        ]
        raw = subprocess.check_output(cmd, universal_newlines=True, stderr=subprocess.STDOUT)
        out = re.sub(r"pydev debugger: process \d+ is connecting\n\n", "", raw, re.M).strip().split("\n")

        _, _, _, bin_dir = virtualenv.path_locations(str(tmp_path / "env"))
        exe = os.path.realpath(
            os.path.join(bin_dir, "{}{}".format(virtualenv.EXPECTED_EXE, ".exe" if virtualenv.IS_WIN else ""))
        )
        assert out == [
            "startup",
            "extend parser with count",
            "adjust options",
            "after install env with options 4",
            "exe at {}".format(exe),
        ]
    except subprocess.CalledProcessError as exception:
        assert not exception.returncode, exception.output
