#!/usr/bin/env python
import mozunit
from unittest import mock
from pathlib import Path
import json

from mozperftest.tests.support import EXAMPLE_TEST, temp_file
from mozperftest.fzf.fzf import select
from mozperftest.fzf.preview import main
from mozperftest.utils import silence


class Fzf:
    def __init__(self, cmd, *args, **kw):
        self.cmd = cmd

    def communicate(self, *args):
        return "query\n" + args[0], "stderr"


@mock.patch("subprocess.Popen", new=Fzf)
def test_select(*mocked):
    test_objects = [{"path": str(EXAMPLE_TEST)}]
    selection = select(test_objects)
    assert len(selection) == 1


def test_preview():
    content = str(EXAMPLE_TEST)
    test_objects = [{"path": content}]
    cache = Path(Path.home(), ".mozbuild", ".perftestfuzzy")
    with cache.open("w") as f:
        f.write(json.dumps(test_objects))

    with temp_file(content=content) as tasklist, silence() as out:
        main(args=["-t", tasklist])

    out[0].seek(0)
    assert "Owner: Performance Testing Team" in out[0].read()


if __name__ == "__main__":
    mozunit.main()
