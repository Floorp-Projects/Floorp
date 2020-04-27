import tempfile
from mock import MagicMock
import contextlib
import shutil
import os

from mozperftest.metadata import Metadata
from mozperftest.environment import MachEnvironment


@contextlib.contextmanager
def temp_file(name="temp", content=None):
    tempdir = tempfile.mkdtemp()
    path = os.path.join(tempdir, name)
    if content is not None:
        with open(path, "w") as f:
            f.write(content)
    try:
        yield path
    finally:
        shutil.rmtree(tempdir)


@contextlib.contextmanager
def temp_dir():
    tempdir = tempfile.mkdtemp()
    try:
        yield tempdir
    finally:
        shutil.rmtree(tempdir)


def get_running_env(**kwargs):
    from mozbuild.base import MozbuildObject

    config = MozbuildObject.from_environment()
    mach_cmd = MagicMock()
    mach_cmd.get_binary_path = lambda: ""
    mach_cmd.topsrcdir = config.topsrcdir
    mach_cmd.topobjdir = config.topobjdir
    mach_cmd._mach_context = MagicMock()
    mach_cmd._mach_context.state_dir = tempfile.mkdtemp()

    mach_args = {
        "test_objects": None,
        "resolve_tests": True,
        "browsertime-clobber": False,
        "browsertime-install-url": None,
    }
    mach_args.update(kwargs)
    env = MachEnvironment(mach_cmd, "script", **mach_args)
    metadata = Metadata(mach_cmd, env, "script")
    return mach_cmd, metadata, env
