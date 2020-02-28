#!/usr/bin/python
import json
import os.path
from datetime import datetime

read_filename = 'count/fuzzbug.json'
write_count = 'badges/open-fuzzbug.json'

open_fuzzbugs = 0
with open(read_filename, 'r') as f:
    filedata = json.load(f)
    # the open fuzzbug count. Can be deleted
    open_fuzzbugs = len([x for x in filedata if x['closed_at'] == None])

# Write fuzzbug count
data = {
    "schemaVersion": 1,
    "label": "Open FuzzBugs",
    "message": str(open_fuzzbugs) if open_fuzzbugs > 0 else "None",
    "color": "green" if open_fuzzbugs == 0 else "yellow",
}

with open(write_count, 'w') as f:
    json.dump(data, f, indent=4)
