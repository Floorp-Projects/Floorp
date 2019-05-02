# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'Mozilla l10n compare locales tool'

from __future__ import absolute_import
from __future__ import print_function
from collections import defaultdict
import six

from .utils import Tree


class Observer(object):

    def __init__(self, quiet=0, filter=None):
        '''Create Observer
        For quiet=1, skip per-entity missing and obsolete strings,
        for quiet=2, skip missing and obsolete files. For quiet=3,
        skip warnings and errors.
        '''
        self.summary = defaultdict(lambda: defaultdict(int))
        self.details = Tree(list)
        self.quiet = quiet
        self.filter = filter
        self.error = False

    def _dictify(self, d):
        plaindict = {}
        for k, v in six.iteritems(d):
            plaindict[k] = dict(v)
        return plaindict

    def toJSON(self):
        # Don't export file stats, even if we collected them.
        # Those are not part of the data we use toJSON for.
        return {
            'summary': self._dictify(self.summary),
            'details': self.details.toJSON()
        }

    def updateStats(self, file, stats):
        # in multi-project scenarios, this file might not be ours,
        # check that.
        # Pass in a dummy entity key '' to avoid getting in to
        # generic file filters. If we have stats for those,
        # we want to aggregate the counts
        if (self.filter is not None and
                self.filter(file, entity='') == 'ignore'):
            return
        for category, value in six.iteritems(stats):
            if category == 'errors':
                # updateStats isn't called with `errors`, but make sure
                # we handle this if that changes
                self.error = True
            self.summary[file.locale][category] += value

    def notify(self, category, file, data):
        rv = 'error'
        if category in ['missingFile', 'obsoleteFile']:
            if self.filter is not None:
                rv = self.filter(file)
            if rv == "ignore" or self.quiet >= 2:
                return rv
            if self.quiet == 0 or category == 'missingFile':
                self.details[file].append({category: rv})
            return rv
        if self.filter is not None:
            rv = self.filter(file, data)
            if rv == "ignore":
                return rv
        if category in ['missingEntity', 'obsoleteEntity']:
            if (
                (category == 'missingEntity' and self.quiet < 2)
                or (category == 'obsoleteEntity' and self.quiet < 1)
            ):
                self.details[file].append({category: data})
            return rv
        if category == 'error':
            # Set error independently of quiet
            self.error = True
        if category in ('error', 'warning'):
            if (
                (category == 'error' and self.quiet < 4)
                or (category == 'warning' and self.quiet < 3)
            ):
                self.details[file].append({category: data})
            self.summary[file.locale][category + 's'] += 1
        return rv


class ObserverList(Observer):
    def __init__(self, quiet=0):
        super(ObserverList, self).__init__(quiet=quiet)
        self.observers = []

    def __iter__(self):
        return iter(self.observers)

    def append(self, observer):
        self.observers.append(observer)

    def notify(self, category, file, data):
        """Check observer for the found data, and if it's
        not to ignore, notify stat_observers.
        """
        rvs = set(
            observer.notify(category, file, data)
            for observer in self.observers
            )
        if all(rv == 'ignore' for rv in rvs):
            return 'ignore'
        # our return value doesn't count
        super(ObserverList, self).notify(category, file, data)
        rvs.discard('ignore')
        if 'error' in rvs:
            return 'error'
        assert len(rvs) == 1
        return rvs.pop()

    def updateStats(self, file, stats):
        """Check observer for the found data, and if it's
        not to ignore, notify stat_observers.
        """
        for observer in self.observers:
            observer.updateStats(file, stats)
        super(ObserverList, self).updateStats(file, stats)

    def serializeDetails(self):

        def tostr(t):
            if t[1] == 'key':
                return '  ' * t[0] + '/'.join(t[2])
            o = []
            indent = '  ' * (t[0] + 1)
            for item in t[2]:
                if 'error' in item:
                    o += [indent + 'ERROR: ' + item['error']]
                elif 'warning' in item:
                    o += [indent + 'WARNING: ' + item['warning']]
                elif 'missingEntity' in item:
                    o += [indent + '+' + item['missingEntity']]
                elif 'obsoleteEntity' in item:
                    o += [indent + '-' + item['obsoleteEntity']]
                elif 'missingFile' in item:
                    o.append(indent + '// add and localize this file')
                elif 'obsoleteFile' in item:
                    o.append(indent + '// remove this file')
            return '\n'.join(o)

        return '\n'.join(tostr(c) for c in self.details.getContent())

    def serializeSummaries(self):
        summaries = {
            loc: []
            for loc in self.summary.keys()
        }
        for observer in self.observers:
            for loc, lst in summaries.items():
                # Not all locales are on all projects,
                # default to empty summary
                lst.append(observer.summary.get(loc, {}))
        if len(self.observers) > 1:
            # add ourselves if there's more than one project
            for loc, lst in summaries.items():
                lst.append(self.summary.get(loc, {}))
        # normalize missing and missingInFiles -> missing
        for summarylist in summaries.values():
            for summary in summarylist:
                if 'missingInFiles' in summary:
                    summary['missing'] = (
                        summary.get('missing', 0)
                        + summary.pop('missingInFiles')
                    )
        keys = (
            'errors',
            'warnings',
            'missing', 'missing_w',
            'obsolete', 'obsolete_w',
            'changed', 'changed_w',
            'unchanged', 'unchanged_w',
            'keys',
        )
        leads = [
            '{:12}'.format(k) for k in keys
        ]
        out = []
        for locale, summaries in sorted(six.iteritems(summaries)):
            if locale:
                out.append(locale + ':')
            segment = [''] * len(keys)
            for summary in summaries:
                for row, key in enumerate(keys):
                    segment[row] += ' {:6}'.format(summary.get(key, ''))

            out += [
                lead + row
                for lead, row in zip(leads, segment)
                if row.strip()
            ]

            total = sum([summaries[-1].get(k, 0)
                         for k in ['changed', 'unchanged', 'report', 'missing',
                                   'missingInFiles']
                         ])
            rate = 0
            if total:
                rate = (('changed' in summary and summary['changed'] * 100) or
                        0) / total
            out.append('%d%% of entries changed' % rate)
        return '\n'.join(out)

    def __str__(self):
        return 'observer'
