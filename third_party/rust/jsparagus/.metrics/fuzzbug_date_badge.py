#!/usr/bin/python
import json
from datetime import datetime

read_filename = 'count/fuzzbug.json'
write_since = 'badges/since-last-fuzzbug.json'

days_since = None
with open(read_filename, 'r') as f:
    filedata = json.load(f)
    count = len(filedata)
    # the last time we saw a fuzzbug regardless of status
    if count > 0:
        dt_format = "%Y-%m-%dT%H:%M:%SZ"
        fuzzbug_opened = filedata[0]["created_at"]
        fuzzbug_date = datetime.strptime(fuzzbug_opened, dt_format)
        today = datetime.today()
        days_since = (today - fuzzbug_date).days


# Write days since last fuzzbug

def get_color(days):
    if days_since is None or days_since > 100:
        return "green"
    elif days_since > 10:
        return "yellow"
    else:
        return "red"


data = {
    "schemaVersion": 1,
    "label": "Days since last FuzzBug",
    "message": str(days_since) if days_since is not None else "Forever",
    "color": get_color(days_since),
}

with open(write_since, 'w') as f:
    json.dump(data, f, indent=4)
