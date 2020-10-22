# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import bisect
import gzip
import json
import math
import requests

from datetime import datetime, timedelta
from collections import Counter

import mozpack.path as mozpath

PUSHLOG_CHUNK_SIZE = 500

URL = 'https://hg.mozilla.org/mozilla-central/json-pushes?'


def unix_epoch(date):
    return (date - datetime(1970, 1, 1)).total_seconds()


def unix_from_date(n, today):
    return unix_epoch(today - timedelta(days=n))


def get_lastpid(session):
    return session.get(URL+'&version=2').json()['lastpushid']


def get_pushlog_chunk(session, start, end):
    # returns pushes sorted by date
    res = session.get(URL+'version=1&startID={0}&\
        endID={1}&full=1'.format(start, end)).json()
    return sorted(res.items(), key=lambda x: x[1]['date'])


def collect_data(session, date):
    if date < 1206031764:  # first push
        raise Exception("No pushes exist before March 20, 2008.")
    lastpushid = get_lastpid(session)
    data = []
    start_id = lastpushid - PUSHLOG_CHUNK_SIZE
    end_id = lastpushid + 1
    while True:
        res = get_pushlog_chunk(session, start_id, end_id)
        starting_date = res[0][1]['date']  # date of oldest push in chunk
        dates = [x[1]['date'] for x in res]
        if starting_date < date:
            i = bisect.bisect_left(dates, date)
            data.append(res[i:])
            return data
        else:
            data.append(res)
            end_id = start_id + 1
            start_id = start_id - PUSHLOG_CHUNK_SIZE


def get_data(epoch):
    session = requests.Session()
    data = collect_data(session, epoch)
    return {k: v for sublist in data for (k, v) in sublist}


class Pushlog(object):

    def __init__(self, days):
        info = get_data(unix_from_date(days, datetime.today()))
        self.pushlog = info
        self.pids = self.get_pids()
        self.pushes = self.make_pushes()
        self.files = [l for p in self.pushes for l in set(p.files)]
        self.file_set = set(self.files)
        self.file_count = Counter(self.files)

    def make_pushes(self):
        pids = self.pids
        all_pushes = self.pushlog
        return [Push(pid, all_pushes[str(pid)]) for pid in pids]

    def get_pids(self):
        keys = self.pushlog.keys()
        keys.sort()
        return keys


class Push(object):

    def __init__(self, pid, p_dict):
        self.id = pid
        self.date = p_dict['date']
        self.files = [f for x in p_dict['changesets'] for f in x['files']]


class Report(object):

    def __init__(self, days, path=None, cost_dict=None):
        obj = Pushlog(days)
        self.file_set = obj.file_set
        self.file_count = obj.file_count
        self.name = str(days) + 'day_report'
        self.cost_dict = self.get_cost_dict(path, cost_dict)

    def get_cost_dict(self, path, cost_dict):
        if path is not None:
            with gzip.open(path) as file:
                return json.loads(file.read())
        else:
            if cost_dict is not None:
                return cost_dict
            else:
                raise Exception

    def organize_data(self):
        costs = self.cost_dict
        counts = self.file_count
        res = []
        for f in self.file_set:
            cost = costs.get(f)
            count = counts.get(f)
            if cost is not None:
                res.append((f, cost, count, round(cost*count, 3)))
        return res

    def get_sorted_report(self, format):
        res = self.organize_data()
        res.sort(key=(lambda x: x[3]), reverse=True)

        def ms_to_mins_secs(ms):
            secs = ms / 1000.0
            mins = secs / 60
            secs = secs % 60
            return '%d:%02d' % (math.trunc(mins), int(round(secs)))

        if format in ('html', 'pretty'):
            res = [(f, ms_to_mins_secs(cost), count, ms_to_mins_secs(total)) for
                   (f, cost, count, total) in res]

        return res

    def cut(self, size, lst):
        if len(lst) <= size:
            return lst
        else:
            return lst[:size]

    def generate_output(self, format, limit, dst):
        import tablib
        data = tablib.Dataset(headers=['FILE', 'TIME', 'CHANGES', 'TOTAL'])
        res = self.get_sorted_report(format)
        if limit is not None:
            res = self.cut(limit, res)
        for x in res:
            data.append(x)
        if format == 'pretty':
            print(data)
        else:
            file_name = self.name + '.' + format
            content = None
            data.export(format)
            if format == 'csv':
                content = data.csv
            elif format == 'json':
                content = data.json
            else:
                content = data.html
            file_path = mozpath.join(dst, file_name)
            with open(file_path, 'wb') as f:
                f.write(content)
            print("Created report: %s" % file_path)
