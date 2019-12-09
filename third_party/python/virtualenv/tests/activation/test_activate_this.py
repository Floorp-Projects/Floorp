"""Activate this does not mangles with the shell itself to provision the python, but instead mangles
with the caller interpreter, effectively making so that the virtualenv activation constraints are met once
it's loaded.

While initially may feel like a import is all we need, import is executed only once not at every activation. To
work around this we'll use Python 2 execfile, and Python 3 exec(read()).

Virtual env activation constraints that we guarantee:
- the virtualenv site-package will be visible from the activator Python
- virtualenv packages take priority over python 2
- virtualenv bin PATH pre-pended
- VIRTUAL_ENV env var will be set.
- if the user tries to import we'll raise

"""
from __future__ import absolute_import, unicode_literals

import os
import re
import subprocess
import sys
import textwrap


def test_activate_this(clean_python, tmp_path, monkeypatch):
    # to test this, we'll try to use the activation env from this Python
    monkeypatch.delenv(str("VIRTUAL_ENV"), raising=False)
    monkeypatch.delenv(str("PYTHONPATH"), raising=False)
    paths = [str(tmp_path), str(tmp_path / "other")]
    start_path = os.pathsep.join(paths)
    monkeypatch.setenv(str("PATH"), start_path)
    activator = tmp_path.__class__(clean_python[1]) / "activate_this.py"
    assert activator.exists()

    activator_at = str(activator)
    script = textwrap.dedent(
        """
    import os
    import sys
    print(os.environ.get("VIRTUAL_ENV"))
    print(os.environ.get("PATH"))
    try:
        import pydoc_test
        raise RuntimeError("this should not happen")
    except ImportError:
        pass
    print(os.pathsep.join(sys.path))
    file_at = {!r}
    exec(open(file_at).read(), {{'__file__': file_at}})
    print(os.environ.get("VIRTUAL_ENV"))
    print(os.environ.get("PATH"))
    print(os.pathsep.join(sys.path))
    import pydoc_test
    print(pydoc_test.__file__)
    """.format(
            str(activator_at)
        )
    )
    script_path = tmp_path / "test.py"
    script_path.write_text(script)
    try:
        raw = subprocess.check_output(
            [sys.executable, str(script_path)], stderr=subprocess.STDOUT, universal_newlines=True
        )

        out = re.sub(r"pydev debugger: process \d+ is connecting\n\n", "", raw, re.M).strip().split("\n")

        assert out[0] == "None"
        assert out[1] == start_path
        prev_sys_path = out[2].split(os.path.pathsep)

        assert out[3] == clean_python[0]  # virtualenv set as the activated env

        # PATH updated with activated
        assert out[4].endswith(start_path)
        assert out[4][: -len(start_path)].split(os.pathsep) == [clean_python[1], ""]

        # sys path contains the site package at its start
        new_sys_path = out[5].split(os.path.pathsep)
        assert new_sys_path[-len(prev_sys_path) :] == prev_sys_path
        extra_start = new_sys_path[0 : -len(prev_sys_path)]
        assert len(extra_start) == 1
        assert extra_start[0].startswith(clean_python[0])
        assert tmp_path.__class__(extra_start[0]).exists()

        # manage to import from activate site package
        assert os.path.realpath(out[6]) == os.path.realpath(str(clean_python[2]))
    except subprocess.CalledProcessError as exception:
        assert not exception.returncode, exception.output


def test_activate_this_no_file(clean_python, tmp_path, monkeypatch):
    activator = tmp_path.__class__(clean_python[1]) / "activate_this.py"
    assert activator.exists()
    try:
        subprocess.check_output(
            [sys.executable, "-c", "exec(open({!r}).read())".format(str(activator))],
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
        raise RuntimeError("this should not happen")
    except subprocess.CalledProcessError as exception:
        out = re.sub(r"pydev debugger: process \d+ is connecting\n\n", "", exception.output, re.M).strip()
        assert "You must use exec(open(this_file).read(), {'__file__': this_file}))" in out, out
