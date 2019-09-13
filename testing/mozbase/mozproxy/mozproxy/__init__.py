# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import sys
import os


def path_join(*args):
    path = os.path.join(*args)
    return os.path.abspath(path)


mozproxy_src_dir = os.path.dirname(os.path.realpath(__file__))
mozproxy_dir = path_join(mozproxy_src_dir, "..")
mozbase_dir = path_join(mozproxy_dir, "..")

# needed so unit tests can find their imports
if os.environ.get("SCRIPTSPATH", None) is not None:
    # in production it is env SCRIPTS_PATH
    mozharness_dir = os.environ["SCRIPTSPATH"]
else:
    # locally it's in source tree
    mozharness_dir = path_join(mozbase_dir, "..", "mozharness")


def get_playback(config):
    """ Returns an instance of the right Playback class
    """
    sys.path.insert(0, mozharness_dir)
    sys.path.insert(0, mozproxy_dir)
    sys.path.insert(0, mozproxy_src_dir)

    from .server import get_backend
    from .utils import LOG

    tool_name = config.get("playback_tool", None)
    if tool_name is None:
        LOG.critical("playback_tool name not found in config")
        return None
    try:
        return get_backend(tool_name, config)
    except KeyError:
        LOG.critical("specified playback tool is unsupported: %s" % tool_name)
        return None
