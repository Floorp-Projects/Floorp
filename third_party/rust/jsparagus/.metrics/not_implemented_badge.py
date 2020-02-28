#!/usr/bin/python
import json
import os.path

filename = 'badges/not-implemented.json'
total_count = os.environ['total_count']
data = {
    "schemaVersion": 1,
    "label": "NotImplemented",
    "message": total_count,
    "color": "green" if total_count == "0" else "yellow",
}
with open(filename, 'w') as f:
    json.dump(data, f, indent=4)
