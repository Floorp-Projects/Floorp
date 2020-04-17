import tempfile
from mock import MagicMock
import contextlib
import shutil
import os

from mozperftest.metadata import Metadata
from mozperftest.environment import MachEnvironment


@contextlib.contextmanager
def temp_file(name="temp"):
    tempdir = tempfile.mkdtemp()
    path = os.path.join(tempdir, name)
    try:
        yield path
    finally:
        shutil.rmtree(tempdir)


def get_running_env():
    from mozbuild.base import MozbuildObject

    config = MozbuildObject.from_environment()
    mach_cmd = MagicMock()
    mach_cmd.get_binary_path = lambda: ""
    mach_cmd.topsrcdir = config.topsrcdir
    mach_cmd.topobjdir = config.topobjdir
    mach_cmd._mach_context = MagicMock()
    mach_cmd._mach_context.state_dir = tempfile.mkdtemp()

    env = MachEnvironment(mach_cmd, "script", test_objects=None, resolve_tests=True)
    metadata = Metadata(mach_cmd, env, "script")
    return mach_cmd, metadata, env
