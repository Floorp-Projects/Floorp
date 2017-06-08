# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'Mozilla l10n compare locales tool'

import codecs
import os
import os.path
import shutil
import re
from difflib import SequenceMatcher
from collections import defaultdict

try:
    from json import dumps
except:
    from simplejson import dumps

from compare_locales import parser
from compare_locales import paths
from compare_locales.checks import getChecker


class Tree(object):
    def __init__(self, valuetype):
        self.branches = dict()
        self.valuetype = valuetype
        self.value = None

    def __getitem__(self, leaf):
        parts = []
        if isinstance(leaf, paths.File):
            parts = [p for p in [leaf.locale, leaf.module] if p] + \
                leaf.file.split('/')
        else:
            parts = leaf.split('/')
        return self.__get(parts)

    def __get(self, parts):
        common = None
        old = None
        new = tuple(parts)
        t = self
        for k, v in self.branches.iteritems():
            for i, part in enumerate(zip(k, parts)):
                if part[0] != part[1]:
                    i -= 1
                    break
            if i < 0:
                continue
            i += 1
            common = tuple(k[:i])
            old = tuple(k[i:])
            new = tuple(parts[i:])
            break
        if old:
            self.branches.pop(k)
            t = Tree(self.valuetype)
            t.branches[old] = v
            self.branches[common] = t
        elif common:
            t = self.branches[common]
        if new:
            if common:
                return t.__get(new)
            t2 = t
            t = Tree(self.valuetype)
            t2.branches[new] = t
        if t.value is None:
            t.value = t.valuetype()
        return t.value

    indent = '  '

    def getContent(self, depth=0):
        '''
        Returns iterator of (depth, flag, key_or_value) tuples.
        If flag is 'value', key_or_value is a value object, otherwise
        (flag is 'key') it's a key string.
        '''
        keys = self.branches.keys()
        keys.sort()
        if self.value is not None:
            yield (depth, 'value', self.value)
        for key in keys:
            yield (depth, 'key', key)
            for child in self.branches[key].getContent(depth + 1):
                yield child

    def toJSON(self):
        '''
        Returns this Tree as a JSON-able tree of hashes.
        Only the values need to take care that they're JSON-able.
        '''
        json = {}
        keys = self.branches.keys()
        keys.sort()
        if self.value is not None:
            json['value'] = self.value
        children = [('/'.join(key), self.branches[key].toJSON())
                    for key in keys]
        if children:
            json['children'] = children
        return json

    def getStrRows(self):
        def tostr(t):
            if t[1] == 'key':
                return self.indent * t[0] + '/'.join(t[2])
            return self.indent * (t[0] + 1) + str(t[2])

        return map(tostr, self.getContent())

    def __str__(self):
        return '\n'.join(self.getStrRows())


class AddRemove(SequenceMatcher):
    def __init__(self):
        SequenceMatcher.__init__(self, None, None, None)

    def set_left(self, left):
        if not isinstance(left, list):
            left = [l for l in left]
        self.set_seq1(left)

    def set_right(self, right):
        if not isinstance(right, list):
            right = [l for l in right]
        self.set_seq2(right)

    def __iter__(self):
        for tag, i1, i2, j1, j2 in self.get_opcodes():
            if tag == 'equal':
                for pair in zip(self.a[i1:i2], self.b[j1:j2]):
                    yield ('equal', pair)
            elif tag == 'delete':
                for item in self.a[i1:i2]:
                    yield ('delete', item)
            elif tag == 'insert':
                for item in self.b[j1:j2]:
                    yield ('add', item)
            else:
                # tag == 'replace'
                for item in self.a[i1:i2]:
                    yield ('delete', item)
                for item in self.b[j1:j2]:
                    yield ('add', item)


class DirectoryCompare(SequenceMatcher):
    def __init__(self, reference):
        SequenceMatcher.__init__(self, None, [i for i in reference],
                                 [])
        self.watcher = None

    def setWatcher(self, watcher):
        self.watcher = watcher

    def compareWith(self, other):
        if not self.watcher:
            return
        self.set_seq2([i for i in other])
        for tag, i1, i2, j1, j2 in self.get_opcodes():
            if tag == 'equal':
                for i, j in zip(xrange(i1, i2), xrange(j1, j2)):
                    self.watcher.compare(self.a[i], self.b[j])
            elif tag == 'delete':
                for i in xrange(i1, i2):
                    self.watcher.add(self.a[i], other.cloneFile(self.a[i]))
            elif tag == 'insert':
                for j in xrange(j1, j2):
                    self.watcher.remove(self.b[j])
            else:
                for j in xrange(j1, j2):
                    self.watcher.remove(self.b[j])
                for i in xrange(i1, i2):
                    self.watcher.add(self.a[i], other.cloneFile(self.a[i]))


class Observer(object):
    stat_cats = ['missing', 'obsolete', 'missingInFiles', 'report',
                 'changed', 'unchanged', 'keys']

    def __init__(self):
        class intdict(defaultdict):
            def __init__(self):
                defaultdict.__init__(self, int)

        self.summary = defaultdict(intdict)
        self.details = Tree(dict)
        self.filter = None

    # support pickling
    def __getstate__(self):
        return dict(summary=self.getSummary(), details=self.details)

    def __setstate__(self, state):
        class intdict(defaultdict):
            def __init__(self):
                defaultdict.__init__(self, int)

        self.summary = defaultdict(intdict)
        if 'summary' in state:
            for loc, stats in state['summary'].iteritems():
                self.summary[loc].update(stats)
        self.details = state['details']
        self.filter = None

    def getSummary(self):
        plaindict = {}
        for k, v in self.summary.iteritems():
            plaindict[k] = dict(v)
        return plaindict

    def toJSON(self):
        return dict(summary=self.getSummary(), details=self.details.toJSON())

    def notify(self, category, file, data):
        rv = "error"
        if category in self.stat_cats:
            # these get called post reporting just for stats
            # return "error" to forward them to other other_observers
            self.summary[file.locale][category] += data
            # keep track of how many strings are in a missing file
            # we got the {'missingFile': 'error'} from the first pass
            if category == 'missingInFiles':
                self.details[file]['strings'] = data
            return "error"
        if category in ['missingFile', 'obsoleteFile']:
            if self.filter is not None:
                rv = self.filter(file)
            if rv != "ignore":
                self.details[file][category] = rv
            return rv
        if category in ['missingEntity', 'obsoleteEntity']:
            if self.filter is not None:
                rv = self.filter(file, data)
            if rv == "ignore":
                return rv
            v = self.details[file]
            try:
                v[category].append(data)
            except KeyError:
                v[category] = [data]
            return rv
        if category == 'error':
            try:
                self.details[file][category].append(data)
            except KeyError:
                self.details[file][category] = [data]
            self.summary[file.locale]['errors'] += 1
        elif category == 'warning':
            try:
                self.details[file][category].append(data)
            except KeyError:
                self.details[file][category] = [data]
            self.summary[file.locale]['warnings'] += 1
        return rv

    def toExhibit(self):
        items = []
        for locale in sorted(self.summary.iterkeys()):
            summary = self.summary[locale]
            if locale is not None:
                item = {'id': 'xxx/' + locale,
                        'label': locale,
                        'locale': locale}
            else:
                item = {'id': 'xxx',
                        'label': 'xxx',
                        'locale': 'xxx'}
            item['type'] = 'Build'
            total = sum([summary[k]
                         for k in ('changed', 'unchanged', 'report', 'missing',
                                   'missingInFiles')
                         if k in summary])
            rate = (('changed' in summary and summary['changed'] * 100) or
                    0) / total
            item.update((k, summary.get(k, 0))
                        for k in ('changed', 'unchanged'))
            item.update((k, summary[k])
                        for k in ('report', 'errors', 'warnings')
                        if k in summary)
            item['missing'] = summary.get('missing', 0) + \
                summary.get('missingInFiles', 0)
            item['completion'] = rate
            item['total'] = total
            result = 'success'
            if item.get('warnings', 0):
                result = 'warning'
            if item.get('errors', 0) or item.get('missing', 0):
                result = 'failure'
            item['result'] = result
            items.append(item)
        data = {
            "properties": dict.fromkeys(
                ("completion", "errors", "warnings", "missing", "report",
                 "unchanged", "changed", "obsolete"),
                {"valueType": "number"}),
            "types": {
                "Build": {"pluralLabel": "Builds"}
            }}
        data['items'] = items
        return dumps(data, indent=2)

    def serialize(self, type="text"):
        if type == "exhibit":
            return self.toExhibit()
        if type == "json":
            return dumps(self.toJSON())

        def tostr(t):
            if t[1] == 'key':
                return '  ' * t[0] + '/'.join(t[2])
            o = []
            indent = '  ' * (t[0] + 1)
            if 'error' in t[2]:
                o += [indent + 'ERROR: ' + e for e in t[2]['error']]
            if 'warning' in t[2]:
                o += [indent + 'WARNING: ' + e for e in t[2]['warning']]
            if 'missingEntity' in t[2] or 'obsoleteEntity' in t[2]:
                missingEntities = ('missingEntity' in t[2] and
                                   t[2]['missingEntity']) or []
                obsoleteEntities = ('obsoleteEntity' in t[2] and
                                    t[2]['obsoleteEntity']) or []
                entities = missingEntities + obsoleteEntities
                entities.sort()
                for entity in entities:
                    op = '+'
                    if entity in obsoleteEntities:
                        op = '-'
                    o.append(indent + op + entity)
            elif 'missingFile' in t[2]:
                o.append(indent + '// add and localize this file')
            elif 'obsoleteFile' in t[2]:
                o.append(indent + '// remove this file')
            return '\n'.join(o)

        out = []
        for locale, summary in sorted(self.summary.iteritems()):
            if locale is not None:
                out.append(locale + ':')
            out += [k + ': ' + str(v) for k, v in sorted(summary.iteritems())]
            total = sum([summary[k]
                         for k in ['changed', 'unchanged', 'report', 'missing',
                                   'missingInFiles']
                         if k in summary])
            rate = 0
            if total:
                rate = (('changed' in summary and summary['changed'] * 100) or
                        0) / total
            out.append('%d%% of entries changed' % rate)
        return '\n'.join(map(tostr, self.details.getContent()) + out)

    def __str__(self):
        return 'observer'


class ContentComparer:
    keyRE = re.compile('[kK]ey')
    nl = re.compile('\n', re.M)

    def __init__(self):
        '''Create a ContentComparer.
        observer is usually a instance of Observer. The return values
        of the notify method are used to control the handling of missing
        entities.
        '''
        self.reference = dict()
        self.observer = Observer()
        self.other_observers = []
        self.merge_stage = None

    def add_observer(self, obs):
        '''Add a non-filtering observer.
        Results from the notify calls are ignored.
        '''
        self.other_observers.append(obs)

    def set_merge_stage(self, merge_stage):
        self.merge_stage = merge_stage

    def merge(self, ref_entities, ref_map, ref_file, l10n_file, missing,
              skips, ctx, canMerge, encoding):
        outfile = os.path.join(self.merge_stage, l10n_file.module,
                               l10n_file.file)
        outdir = os.path.dirname(outfile)
        if not os.path.isdir(outdir):
            os.makedirs(outdir)
        if not canMerge:
            shutil.copyfile(ref_file.fullpath, outfile)
            print "copied reference to " + outfile
            return
        if skips:
            # skips come in ordered by key name, we need them in file order
            skips.sort(key=lambda s: s.span[0])
        trailing = (['\n'] +
                    [ref_entities[ref_map[key]].all for key in missing] +
                    [ref_entities[ref_map[skip.key]].all for skip in skips
                     if not isinstance(skip, parser.Junk)])
        if skips:
            # we need to skip a few errornous blocks in the input, copy by hand
            f = codecs.open(outfile, 'wb', encoding)
            offset = 0
            for skip in skips:
                chunk = skip.span
                f.write(ctx.contents[offset:chunk[0]])
                offset = chunk[1]
            f.write(ctx.contents[offset:])
        else:
            shutil.copyfile(l10n_file.fullpath, outfile)
            f = codecs.open(outfile, 'ab', encoding)
        print "adding to " + outfile

        def ensureNewline(s):
            if not s.endswith('\n'):
                return s + '\n'
            return s

        f.write(''.join(map(ensureNewline, trailing)))
        f.close()

    def notify(self, category, file, data):
        """Check observer for the found data, and if it's
        not to ignore, notify other_observers.
        """
        rv = self.observer.notify(category, file, data)
        if rv == 'ignore':
            return rv
        for obs in self.other_observers:
            # non-filtering other_observers, ignore results
            obs.notify(category, file, data)
        return rv

    def remove(self, obsolete):
        self.notify('obsoleteFile', obsolete, None)
        pass

    def compare(self, ref_file, l10n):
        try:
            p = parser.getParser(ref_file.file)
        except UserWarning:
            # no comparison, XXX report?
            return
        if ref_file not in self.reference:
            # we didn't parse this before
            try:
                p.readContents(ref_file.getContents())
            except Exception, e:
                self.notify('error', ref_file, str(e))
                return
            self.reference[ref_file] = p.parse()
        ref = self.reference[ref_file]
        ref_list = ref[1].keys()
        ref_list.sort()
        try:
            p.readContents(l10n.getContents())
            l10n_entities, l10n_map = p.parse()
            l10n_ctx = p.ctx
        except Exception, e:
            self.notify('error', l10n, str(e))
            return

        l10n_list = l10n_map.keys()
        l10n_list.sort()
        ar = AddRemove()
        ar.set_left(ref_list)
        ar.set_right(l10n_list)
        report = missing = obsolete = changed = unchanged = keys = 0
        missings = []
        skips = []
        checker = getChecker(l10n, reference=ref[0])
        for action, item_or_pair in ar:
            if action == 'delete':
                # missing entity
                _rv = self.notify('missingEntity', l10n, item_or_pair)
                if _rv == "ignore":
                    continue
                if _rv == "error":
                    # only add to missing entities for l10n-merge on error,
                    # not report
                    missings.append(item_or_pair)
                    missing += 1
                else:
                    # just report
                    report += 1
            elif action == 'add':
                # obsolete entity or junk
                if isinstance(l10n_entities[l10n_map[item_or_pair]],
                              parser.Junk):
                    junk = l10n_entities[l10n_map[item_or_pair]]
                    params = (junk.val,) + junk.position() + junk.position(-1)
                    self.notify('error', l10n,
                                'Unparsed content "%s" from line %d colum %d'
                                ' to line %d column %d' % params)
                    if self.merge_stage is not None:
                        skips.append(junk)
                elif self.notify('obsoleteEntity', l10n,
                                 item_or_pair) != 'ignore':
                    obsolete += 1
            else:
                # entity found in both ref and l10n, check for changed
                entity = item_or_pair[0]
                refent = ref[0][ref[1][entity]]
                l10nent = l10n_entities[l10n_map[entity]]
                if self.keyRE.search(entity):
                    keys += 1
                else:
                    if refent.val == l10nent.val:
                        self.doUnchanged(l10nent)
                        unchanged += 1
                    else:
                        self.doChanged(ref_file, refent, l10nent)
                        changed += 1
                        # run checks:
                if checker:
                    for tp, pos, msg, cat in checker.check(refent, l10nent):
                        # compute real src position, if first line,
                        # col needs adjustment
                        if isinstance(pos, tuple):
                            _l, col = l10nent.value_position()
                            # line, column
                            if pos[0] == 1:
                                col = col + pos[1]
                            else:
                                col = pos[1]
                                _l += pos[0] - 1
                        else:
                            _l, col = l10nent.value_position(pos)
                        # skip error entities when merging
                        if tp == 'error' and self.merge_stage is not None:
                            skips.append(l10nent)
                        self.notify(tp, l10n,
                                    u"%s at line %d, column %d for %s" %
                                    (msg, _l, col, refent.key))
                pass
        if missing:
            self.notify('missing', l10n, missing)
        if self.merge_stage is not None and (missings or skips):
            self.merge(
                ref[0], ref[1], ref_file,
                l10n, missings, skips, l10n_ctx,
                p.canMerge, p.encoding)
        if report:
            self.notify('report', l10n, report)
        if obsolete:
            self.notify('obsolete', l10n, obsolete)
        if changed:
            self.notify('changed', l10n, changed)
        if unchanged:
            self.notify('unchanged', l10n, unchanged)
        if keys:
            self.notify('keys', l10n, keys)
        pass

    def add(self, orig, missing):
        if self.notify('missingFile', missing, None) == "ignore":
            # filter said that we don't need this file, don't count it
            return
        f = orig
        try:
            p = parser.getParser(f.file)
        except UserWarning:
            return
        try:
            p.readContents(f.getContents())
            entities, map = p.parse()
        except Exception, e:
            self.notify('error', f, str(e))
            return
        self.notify('missingInFiles', missing, len(map))

    def doUnchanged(self, entity):
        # overload this if needed
        pass

    def doChanged(self, file, ref_entity, l10n_entity):
        # overload this if needed
        pass


def compareApp(app, other_observer=None, merge_stage=None, clobber=False):
    '''Compare locales set in app.

    Optional arguments are:
    - other_observer. A object implementing
        notify(category, _file, data)
      The return values of that callback are ignored.
    - merge_stage. A directory to be used for staging the output of
      l10n-merge.
    - clobber. Clobber the module subdirectories of the merge dir as we go.
      Use wisely, as it might cause data loss.
    '''
    comparer = ContentComparer()
    if other_observer is not None:
        comparer.add_observer(other_observer)
    comparer.observer.filter = app.filter
    for module, reference, locales in app:
        dir_comp = DirectoryCompare(reference)
        dir_comp.setWatcher(comparer)
        for _, localization in locales:
            if merge_stage is not None:
                locale_merge = merge_stage.format(ab_CD=localization.locale)
                comparer.set_merge_stage(locale_merge)
                if clobber:
                    # if clobber, remove the stage for the module if it exists
                    clobberdir = os.path.join(locale_merge, module)
                    if os.path.exists(clobberdir):
                        shutil.rmtree(clobberdir)
                        print "clobbered " + clobberdir
            dir_comp.compareWith(localization)
    return comparer.observer


def compareDirs(reference, locale, other_observer=None, merge_stage=None):
    '''Compare reference and locale dir.

    Optional arguments are:
    - other_observer. A object implementing
        notify(category, _file, data)
      The return values of that callback are ignored.
    '''
    comparer = ContentComparer()
    if other_observer is not None:
        comparer.add_observer(other_observer)
    comparer.set_merge_stage(merge_stage)
    dir_comp = DirectoryCompare(paths.EnumerateDir(reference))
    dir_comp.setWatcher(comparer)
    dir_comp.compareWith(paths.EnumerateDir(locale))
    return comparer.observer
