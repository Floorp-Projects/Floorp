import os
import sys

from .utils import invoke


def test_remove_legacy_cache_dir():
    """
    Check that legacy cache dir is removed at import time.
    """
    os.mkdir(os.path.expanduser("~/.pip-tools"))

    status, output = invoke([sys.executable, "-m", "piptools"])

    output = output.decode("utf-8")
    assert output.startswith("Removing old cache dir")
    assert status == 0
