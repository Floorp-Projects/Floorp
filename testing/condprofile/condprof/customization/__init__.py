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
    if not os.path.exists(path):
        raise IOError("Can't find the customization file %r" % path)
    with open(path) as f:
        return json.loads(f.read())
