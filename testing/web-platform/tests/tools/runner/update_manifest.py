import json
import os
import sys

here = os.path.abspath(os.path.split(__file__)[0])
root = os.path.abspath(os.path.join(here, "..", ".."))

sys.path.insert(0, os.path.abspath(os.path.join(here, "..", "scripts")))

import manifest

def main(request, response):
    path = os.path.join(root, "MANIFEST.json")
    manifest_file = manifest.load(path)
    manifest.update(root, "/", manifest_file)
    manifest.write(manifest_file, path)

    return [("Content-Type", "application/json")], json.dumps({"url": "/MANIFEST.json"})
