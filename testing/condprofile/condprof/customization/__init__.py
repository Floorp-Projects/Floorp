from __future__ import absolute_import
import os
import json

HERE = os.path.dirname(__file__)


def get_customizations():
    for f in os.listdir(HERE):
        if not f.endswith("json"):
            continue
        yield os.path.join(HERE, f)


def get_customization(path):
    if not path.endswith(".json"):
        path += ".json"
    if not os.path.exists(path):
        # trying relative
        rpath = os.path.join(HERE, path)
        if not os.path.exists(rpath):
            raise IOError("Can't find the customization file %r" % path)
        path = rpath
    with open(path) as f:
        return json.loads(f.read())
