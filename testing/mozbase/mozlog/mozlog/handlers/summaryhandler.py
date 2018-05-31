# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from collections import (
    defaultdict,
    OrderedDict,
)

from ..reader import LogHandler
import six


class SummaryHandler(LogHandler):
    """Handler class for storing suite summary information.

    Can handle multiple suites in a single run. Summary
    information is stored on the self.summary instance variable.

    Per suite summary information can be obtained by calling 'get'
    or iterating over this class.
    """

    def __init__(self, **kwargs):
        super(SummaryHandler, self).__init__(**kwargs)

        self.summary = OrderedDict()
        self.current_suite = None

    @property
    def current(self):
        return self.summary.get(self.current_suite)

    def __getitem__(self, suite):
        """Return summary information for the given suite.

        The summary is of the form:

            {
              'counts': {
                '<check>': {
                  'count': int,
                  'expected': {
                    '<status>': int,
                  },
                  'unexpected': {
                    '<status>': int,
                  },
                },
              },
              'unexpected_logs': {
                '<test>': [<data>]
              }
            }

        Valid values for <check> are `test`, `subtest` and `assert`. Valid
        <status> keys are defined in the :py:mod:`mozlog.logtypes` module.  The
        <test> key is the id as logged by `test_start`. Finally the <data>
        field is the log data from any `test_end` or `test_status` log messages
        that have an unexpected result.
        """
        return self.summary[suite]

    def __iter__(self):
        """Iterate over summaries.

        Yields a tuple of (suite, summary). The summary returned is
        the same format as returned by 'get'.
        """
        for suite, data in six.iteritems(self.summary):
            yield suite, data

    @classmethod
    def aggregate(cls, key, counts, include_skip=True):
        """Helper method for aggregating count data by 'key' instead of by 'check'."""
        assert key in ('count', 'expected', 'unexpected')

        res = defaultdict(int)
        for check, val in counts.items():
            if key == 'count':
                res[check] += val[key]
                continue

            for status, num in val[key].items():
                if not include_skip and status == 'skip':
                    continue
                res[check] += num
        return res

    def suite_start(self, data):
        self.current_suite = data.get('name', 'suite {}'.format(len(self.summary) + 1))
        if self.current_suite not in self.summary:
            self.summary[self.current_suite] = {
                'counts': {
                    'test': {
                        'count': 0,
                        'expected': defaultdict(int),
                        'unexpected': defaultdict(int),
                    },
                    'subtest': {
                        'count': 0,
                        'expected': defaultdict(int),
                        'unexpected': defaultdict(int),
                    },
                    'assert': {
                        'count': 0,
                        'expected': defaultdict(int),
                        'unexpected': defaultdict(int),
                    }
                },
                'unexpected_logs': OrderedDict(),
            }

    def test_start(self, data):
        self.current['counts']['test']['count'] += 1

    def test_status(self, data):
        logs = self.current['unexpected_logs']
        count = self.current['counts']
        count['subtest']['count'] += 1

        if 'expected' in data:
            count['subtest']['unexpected'][data['status'].lower()] += 1
            if data['test'] not in logs:
                logs[data['test']] = []
            logs[data['test']].append(data)
        else:
            count['subtest']['expected'][data['status'].lower()] += 1

    def test_end(self, data):
        logs = self.current['unexpected_logs']
        count = self.current['counts']
        if 'expected' in data:
            count['test']['unexpected'][data['status'].lower()] += 1
            if data['test'] not in logs:
                logs[data['test']] = []
            logs[data['test']].append(data)
        else:
            count['test']['expected'][data['status'].lower()] += 1

    def assertion_count(self, data):
        count = self.current['counts']
        count['assert']['count'] += 1

        if data['min_expected'] <= data['count'] <= data['max_expected']:
            if data['count'] > 0:
                count['assert']['expected']['fail'] += 1
            else:
                count['assert']['expected']['pass'] += 1
        elif data['max_expected'] < data['count']:
            count['assert']['unexpected']['fail'] += 1
        else:
            count['assert']['unexpected']['pass'] += 1
