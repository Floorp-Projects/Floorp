import tempfile
from mock import MagicMock
from mozperftest.metadata import Metadata


def get_running_env():
    from mozbuild.base import MozbuildObject

    config = MozbuildObject.from_environment()
    mach_cmd = MagicMock()
    mach_cmd.get_binary_path = lambda: ""
    mach_cmd.topsrcdir = config.topsrcdir
    mach_cmd.topobjdir = config.topobjdir
    mach_cmd._mach_context = MagicMock()
    mach_cmd._mach_context.state_dir = tempfile.mkdtemp()
    metadata = Metadata(mach_cmd, "script")
    return mach_cmd, metadata
