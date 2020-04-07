#!/usr/bin/python
import json
import os.path

filename = 'count/not-implemented.json'
if not os.path.isfile(filename):
    with open(filename, 'w') as f:
        json.dump([], f, indent=4)  # initialize with an empty list

with open(filename, 'r+') as f:
    data = json.load(f)
    if len(data) == 0 or data[-1]["commit"] != os.environ['current_commit']:
        data.append({
            "commit": os.environ['current_commit'],
            "total_count": os.environ['total_count']
        })
        f.seek(0)
        json.dump(data, f, indent=4)
        f.truncate()
