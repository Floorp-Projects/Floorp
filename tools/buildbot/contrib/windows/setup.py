# setup.py
# A distutils setup script to create py2exe binaries for buildbot.
# Both a service and standard executable are created.
# Usage:
# % setup.py py2exe

import sys, os, tempfile, shutil
from os.path import dirname, join, abspath, exists, splitext

this_dir = abspath(dirname(__file__))
bb_root_dir = abspath(join(this_dir, "..", ".."))

from distutils.core import setup
import py2exe

includes = []

# We try and bundle *all* modules in the following packages:
for package in ["buildbot.changes", "buildbot.process", "buildbot.status"]:
    __import__(package)
    p = sys.modules[package]
    for fname in os.listdir(p.__path__[0]):
        base, ext = splitext(fname)
        if not fname.startswith("_") and ext == ".py":
            includes.append(p.__name__ + "." + base)

# Other misc modules dynamically imported, so missed by py2exe
includes.extend("""
            buildbot.scheduler
            buildbot.slave.bot
            buildbot.master
            twisted.internet.win32eventreactor
            twisted.web.resource""".split())

# Turn into "," sep py2exe requires
includes = ",".join(includes)

py2exe_options = {"bundle_files": 1,
                  "includes": includes,
                 }

# Each "target" executable we create
buildbot_target = {
    "script": join(bb_root_dir, "bin", "buildbot")
}
# Due to the way py2exe works, we need to rebuild the service code as a
# normal console process - this will be executed by the service itself.

service_target = {
    "modules": ["buildbot_service"],
    "cmdline_style": "custom",
}

# We use the py2exe "bundle" option, so servicemanager.pyd
# (which has the message resources) does not exist.  Take a copy
# of it with a "friendlier" name.  The service runtime arranges for this
# to be used.
import servicemanager

msg_file = join(tempfile.gettempdir(), "buildbot.msg")
shutil.copy(servicemanager.__file__, msg_file)

data_files = [
    ["", [msg_file]],
    ["", [join(bb_root_dir, "buildbot", "status", "classic.css")]],
    ["", [join(bb_root_dir, "buildbot", "buildbot.png")]],
]

try:
    setup(name="buildbot",
          # The buildbot script as a normal executable
          console=[buildbot_target],
          service=[service_target],
          options={'py2exe': py2exe_options},
          data_files = data_files,
          zipfile = "buildbot.library", # 'library.zip' invites trouble :)
    )
finally:
    os.unlink(msg_file)
