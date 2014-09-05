import json
import os
import sys

here = os.path.abspath(os.path.split(__file__)[0])
root = os.path.abspath(os.path.join(here, "..", ".."))

sys.path.insert(0, os.path.abspath(os.path.join(here, "..", "scripts")))

import manifest

def main(request, response):
    manifest_path = os.path.join(root, "MANIFEST.json")
    manifest.update_manifest(root, **{"rebuild": False,
                                      "local_changes": True,
                                      "path": manifest_path})

    return [("Content-Type", "application/json")], json.dumps({"url": "/MANIFEST.json"})
