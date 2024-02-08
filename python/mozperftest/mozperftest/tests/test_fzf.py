#!/usr/bin/env python
import json
from pathlib import Path
from unittest import mock

import mozunit

from mozperftest.fzf.fzf import select
from mozperftest.fzf.preview import main
from mozperftest.tests.support import EXAMPLE_TEST, temp_file
from mozperftest.utils import silence


class Fzf:
    def __init__(self, cmd, *args, **kw):
        self.cmd = cmd

    def communicate(self, *args):
        return "query\n" + args[0], "stderr"


def fzf_executable(*args, path: str = None):
    return None if len(args) == 2 else "fzf"


@mock.patch("subprocess.Popen", new=Fzf)
@mock.patch("mozperftest.fzf.fzf.which", new=fzf_executable)
def test_select(*mocked):
    test_objects = [{"path": EXAMPLE_TEST}]
    selection = select(test_objects)
    assert len(selection) == 1


@mock.patch("subprocess.Popen", new=Fzf)
@mock.patch("mozperftest.fzf.fzf.which", new=fzf_executable)
def test_find_fzf_executable(*mocked):
    test_objects = [{"path": EXAMPLE_TEST}]
    selection = select(test_objects)
    assert len(selection) == 1


def test_preview():
    content = Path(EXAMPLE_TEST)
    line = f"[bt][sometag] {content.name} in {content.parent}"
    test_objects = [{"path": str(content)}]
    cache = Path(Path.home(), ".mozbuild", ".perftestfuzzy")
    with cache.open("w") as f:
        f.write(json.dumps(test_objects))

    with temp_file(content=str(line)) as tasklist, silence() as out:
        main(args=["-t", tasklist])

    stdout, __ = out
    stdout.seek(0)
    assert ":owner: Performance Testing Team" in stdout.read()


if __name__ == "__main__":
    mozunit.main()
