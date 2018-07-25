# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'Mozilla l10n compare locales tool'

from __future__ import absolute_import
from __future__ import print_function
import codecs
import os
import shutil
import re
from collections import defaultdict
import six
from six.moves import map
from six.moves import zip

from json import dumps

from compare_locales import parser
from compare_locales import paths, mozpath
from compare_locales.checks import getChecker


class Tree(object):
    def __init__(self, valuetype):
        self.branches = dict()
        self.valuetype = valuetype
        self.value = None

    def __getitem__(self, leaf):
        parts = []
        if isinstance(leaf, paths.File):
            parts = [] if not leaf.locale else [leaf.locale]
            if leaf.module:
                parts += leaf.module.split('/')
            parts += leaf.file.split('/')
        else:
            parts = leaf.split('/')
        return self.__get(parts)

    def __get(self, parts):
        common = None
        old = None
        new = tuple(parts)
        t = self
        for k, v in six.iteritems(self.branches):
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
        keys = sorted(self.branches.keys())
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
        if self.value is not None:
            return self.value
        return dict(('/'.join(key), self.branches[key].toJSON())
                    for key in self.branches.keys())

    def getStrRows(self):
        def tostr(t):
            if t[1] == 'key':
                return self.indent * t[0] + '/'.join(t[2])
            return self.indent * (t[0] + 1) + str(t[2])

        return [tostr(c) for c in self.getContent()]

    def __str__(self):
        return '\n'.join(self.getStrRows())


class AddRemove(object):
    def __init__(self):
        self.left = self.right = None

    def set_left(self, left):
        if not isinstance(left, list):
            left = list(l for l in left)
        self.left = left

    def set_right(self, right):
        if not isinstance(right, list):
            right = list(l for l in right)
        self.right = right

    def __iter__(self):
        # order_map stores index in left and then index in right
        order_map = dict((item, (i, -1)) for i, item in enumerate(self.left))
        left_items = set(order_map)
        # as we go through the right side, keep track of which left
        # item we had in right last, and for items not in left,
        # set the sortmap to (left_offset, right_index)
        left_offset = -1
        right_items = set()
        for i, item in enumerate(self.right):
            right_items.add(item)
            if item in order_map:
                left_offset = order_map[item][0]
            else:
                order_map[item] = (left_offset, i)
        for item in sorted(order_map, key=lambda item: order_map[item]):
            if item in left_items and item in right_items:
                yield ('equal', item)
            elif item in left_items:
                yield ('delete', item)
            else:
                yield ('add', item)


class Observer(object):

    def __init__(self, quiet=0, filter=None, file_stats=False):
        '''Create Observer
        For quiet=1, skip per-entity missing and obsolete strings,
        for quiet=2, skip missing and obsolete files. For quiet=3,
        skip warnings and errors.
        '''
        self.summary = defaultdict(lambda: defaultdict(int))
        self.details = Tree(list)
        self.quiet = quiet
        self.filter = filter
        self.file_stats = None
        if file_stats:
            self.file_stats = defaultdict(lambda: defaultdict(dict))

    # support pickling
    def __getstate__(self):
        state = dict(summary=self._dictify(self.summary), details=self.details)
        if self.file_stats is not None:
            state['file_stats'] = self._dictify(self.file_stats)
        return state

    def __setstate__(self, state):
        self.summary = defaultdict(lambda: defaultdict(int))
        if 'summary' in state:
            for loc, stats in six.iteritems(state['summary']):
                self.summary[loc].update(stats)
        self.file_stats = None
        if 'file_stats' in state:
            self.file_stats = defaultdict(lambda: defaultdict(dict))
            for k, d in six.iteritems(state['file_stats']):
                self.file_stats[k].update(d)
        self.details = state['details']
        self.filter = None

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
            self.summary[file.locale][category] += value
        if self.file_stats is None:
            return
        if 'missingInFiles' in stats:
            # keep track of how many strings are in a missing file
            # we got the {'missingFile': 'error'} from the notify pass
            self.details[file].append({'count': stats['missingInFiles']})
            # missingInFiles should just be "missing" in file stats
            self.file_stats[file.locale][file.localpath]['missing'] = \
                stats['missingInFiles']
            return  # there are no other stats for missing files
        self.file_stats[file.locale][file.localpath].update(stats)

    def notify(self, category, file, data):
        rv = 'error'
        if category in ['missingFile', 'obsoleteFile']:
            if self.filter is not None:
                rv = self.filter(file)
            if rv != "ignore" and self.quiet < 2:
                self.details[file].append({category: rv})
            return rv
        if category in ['missingEntity', 'obsoleteEntity']:
            if self.filter is not None:
                rv = self.filter(file, data)
            if rv == "ignore":
                return rv
            if self.quiet < 1:
                self.details[file].append({category: data})
            return rv
        if category in ('error', 'warning') and self.quiet < 3:
            self.details[file].append({category: data})
            self.summary[file.locale][category + 's'] += 1
        return rv

    def toExhibit(self):
        items = []
        for locale in sorted(six.iterkeys(self.summary)):
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
            total_w = sum([summary[k]
                           for k in ('changed_w', 'unchanged_w', 'missing_w')
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
            item.update((k, summary.get(k, 0))
                        for k in ('changed_w', 'unchanged_w', 'missing_w'))
            item['total_w'] = total_w
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
                 "missing_w", "changed_w", "unchanged_w",
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

        out = []
        for locale, summary in sorted(six.iteritems(self.summary)):
            if locale is not None:
                out.append(locale + ':')
            out += [
                k + ': ' + str(v) for k, v in sorted(six.iteritems(summary))]
            total = sum([summary[k]
                         for k in ['changed', 'unchanged', 'report', 'missing',
                                   'missingInFiles']
                         if k in summary])
            rate = 0
            if total:
                rate = (('changed' in summary and summary['changed'] * 100) or
                        0) / total
            out.append('%d%% of entries changed' % rate)
        return '\n'.join([tostr(c) for c in self.details.getContent()] + out)

    def __str__(self):
        return 'observer'


class ContentComparer:
    keyRE = re.compile('[kK]ey')
    nl = re.compile('\n', re.M)

    def __init__(self, observers, stat_observers=None):
        '''Create a ContentComparer.
        observer is usually a instance of Observer. The return values
        of the notify method are used to control the handling of missing
        entities.
        '''
        self.observers = observers
        if stat_observers is None:
            stat_observers = []
        self.stat_observers = stat_observers

    def create_merge_dir(self, merge_file):
        outdir = mozpath.dirname(merge_file)
        if not os.path.isdir(outdir):
            os.makedirs(outdir)

    def merge(self, ref_entities, ref_map, ref_file, l10n_file, merge_file,
              missing, skips, ctx, capabilities, encoding):
        '''Create localized file in merge dir

        `ref_entities` and `ref_map` are the parser result of the
        reference file
        `ref_file` and `l10n_file` are the File objects for the reference and
        the l10n file, resp.
        `merge_file` is the output path for the generated content. This is None
        if we're just comparing or validating.
        `missing` are the missing messages in l10n - potentially copied from
        reference
        `skips` are entries to be dropped from the localized file
        `ctx` is the parsing context
        `capabilities` are the capabilities for the merge algorithm
        `encoding` is the encoding to be used when serializing, usually utf-8
        '''

        if not merge_file:
            return

        if capabilities == parser.CAN_NONE:
            return

        self.create_merge_dir(merge_file)

        if capabilities & parser.CAN_COPY:
            # copy the l10n file if it's good, or the reference file if not
            if skips or missing:
                src = ref_file.fullpath
            else:
                src = l10n_file.fullpath
            shutil.copyfile(src, merge_file)
            print("copied reference to " + merge_file)
            return

        if not (capabilities & parser.CAN_SKIP):
            return

        # Start with None in case the merge file doesn't need to be created.
        f = None

        if skips:
            # skips come in ordered by key name, we need them in file order
            skips.sort(key=lambda s: s.span[0])

            # we need to skip a few erroneous blocks in the input, copy by hand
            f = codecs.open(merge_file, 'wb', encoding)
            offset = 0
            for skip in skips:
                chunk = skip.span
                f.write(ctx.contents[offset:chunk[0]])
                offset = chunk[1]
            f.write(ctx.contents[offset:])

        if f is None:
            # l10n file is a good starting point
            shutil.copyfile(l10n_file.fullpath, merge_file)

        if not (capabilities & parser.CAN_MERGE):
            if f:
                f.close()
            return

        if skips or missing:
            if f is None:
                f = codecs.open(merge_file, 'ab', encoding)
            trailing = (['\n'] +
                        [ref_entities[ref_map[key]].all for key in missing] +
                        [ref_entities[ref_map[skip.key]].all for skip in skips
                         if not isinstance(skip, parser.Junk)])

            def ensureNewline(s):
                if not s.endswith('\n'):
                    return s + '\n'
                return s

            print("adding to " + merge_file)
            f.write(''.join(map(ensureNewline, trailing)))

        if f is not None:
            f.close()

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
        rvs.discard('ignore')
        for obs in self.stat_observers:
            # non-filtering stat_observers, ignore results
            obs.notify(category, file, data)
        if 'error' in rvs:
            return 'error'
        assert len(rvs) == 1
        return rvs.pop()

    def updateStats(self, file, stats):
        """Check observer for the found data, and if it's
        not to ignore, notify stat_observers.
        """
        for observer in self.observers + self.stat_observers:
            observer.updateStats(file, stats)

    def remove(self, ref_file, l10n, merge_file):
        '''Obsolete l10n file.

        Copy to merge stage if we can.
        '''
        self.notify('obsoleteFile', l10n, None)
        self.merge(
            [], {}, ref_file, l10n, merge_file,
            [], [], None, parser.CAN_COPY, None
        )

    def compare(self, ref_file, l10n, merge_file, extra_tests=None):
        try:
            p = parser.getParser(ref_file.file)
        except UserWarning:
            # no comparison, XXX report?
            # At least, merge
            self.merge(
                [], {},
                ref_file, l10n, merge_file, [], [], None,
                parser.CAN_COPY, None)
            return
        try:
            p.readContents(ref_file.getContents())
        except Exception as e:
            self.notify('error', ref_file, str(e))
            return
        ref_entities, ref_map = p.parse()
        try:
            p.readContents(l10n.getContents())
            l10n_entities, l10n_map = p.parse()
            l10n_ctx = p.ctx
        except Exception as e:
            self.notify('error', l10n, str(e))
            return

        ar = AddRemove()
        ar.set_left(e.key for e in ref_entities)
        ar.set_right(e.key for e in l10n_entities)
        report = missing = obsolete = changed = unchanged = keys = 0
        missing_w = changed_w = unchanged_w = 0  # word stats
        missings = []
        skips = []
        checker = getChecker(l10n, extra_tests=extra_tests)
        if checker and checker.needs_reference:
            checker.set_reference(ref_entities)
        for msg in p.findDuplicates(ref_entities):
            self.notify('warning', l10n, msg)
        for msg in p.findDuplicates(l10n_entities):
            self.notify('error', l10n, msg)
        for action, entity_id in ar:
            if action == 'delete':
                # missing entity
                if isinstance(ref_entities[ref_map[entity_id]], parser.Junk):
                    self.notify('warning', l10n, 'Parser error in en-US')
                    continue
                _rv = self.notify('missingEntity', l10n, entity_id)
                if _rv == "ignore":
                    continue
                if _rv == "error":
                    # only add to missing entities for l10n-merge on error,
                    # not report
                    missings.append(entity_id)
                    missing += 1
                    refent = ref_entities[ref_map[entity_id]]
                    missing_w += refent.count_words()
                else:
                    # just report
                    report += 1
            elif action == 'add':
                # obsolete entity or junk
                if isinstance(l10n_entities[l10n_map[entity_id]],
                              parser.Junk):
                    junk = l10n_entities[l10n_map[entity_id]]
                    params = (junk.val,) + junk.position() + junk.position(-1)
                    self.notify('error', l10n,
                                'Unparsed content "%s" from line %d column %d'
                                ' to line %d column %d' % params)
                    if merge_file is not None:
                        skips.append(junk)
                elif self.notify('obsoleteEntity', l10n,
                                 entity_id) != 'ignore':
                    obsolete += 1
            else:
                # entity found in both ref and l10n, check for changed
                refent = ref_entities[ref_map[entity_id]]
                l10nent = l10n_entities[l10n_map[entity_id]]
                if self.keyRE.search(entity_id):
                    keys += 1
                else:
                    if refent.equals(l10nent):
                        self.doUnchanged(l10nent)
                        unchanged += 1
                        unchanged_w += refent.count_words()
                    else:
                        self.doChanged(ref_file, refent, l10nent)
                        changed += 1
                        changed_w += refent.count_words()
                        # run checks:
                if checker:
                    for tp, pos, msg, cat in checker.check(refent, l10nent):
                        line, col = l10nent.value_position(pos)
                        # skip error entities when merging
                        if tp == 'error' and merge_file is not None:
                            skips.append(l10nent)
                        self.notify(tp, l10n,
                                    u"%s at line %d, column %d for %s" %
                                    (msg, line, col, refent.key))
                pass

        if merge_file is not None:
            self.merge(
                ref_entities, ref_map, ref_file,
                l10n, merge_file, missings, skips, l10n_ctx,
                p.capabilities, p.encoding)

        stats = {}
        for cat, value in (
                ('missing', missing),
                ('missing_w', missing_w),
                ('report', report),
                ('obsolete', obsolete),
                ('changed', changed),
                ('changed_w', changed_w),
                ('unchanged', unchanged),
                ('unchanged_w', unchanged_w),
                ('keys', keys)):
            if value:
                stats[cat] = value
        self.updateStats(l10n, stats)
        pass

    def add(self, orig, missing, merge_file):
        ''' Add missing localized file.'''
        f = orig
        try:
            p = parser.getParser(f.file)
        except UserWarning:
            p = None

        # if we don't support this file, assume CAN_COPY to mimick
        # l10n dir as closely as possible
        caps = p.capabilities if p else parser.CAN_COPY
        if (caps & (parser.CAN_COPY | parser.CAN_MERGE)):
            # even if we can merge, pretend we can only copy
            self.merge(
                [], {}, orig, missing, merge_file,
                ['trigger copy'], [], None, parser.CAN_COPY, None
            )

        if self.notify('missingFile', missing, None) == "ignore":
            # filter said that we don't need this file, don't count it
            return

        if p is None:
            # We don't have a parser, cannot count missing strings
            return

        try:
            p.readContents(f.getContents())
            entities, map = p.parse()
        except Exception as ex:
            self.notify('error', f, str(ex))
            return
        # strip parse errors
        entities = [e for e in entities if not isinstance(e, parser.Junk)]
        self.updateStats(missing, {'missingInFiles': len(entities)})
        missing_w = 0
        for e in entities:
            missing_w += e.count_words()
        self.updateStats(missing, {'missing_w': missing_w})

    def doUnchanged(self, entity):
        # overload this if needed
        pass

    def doChanged(self, file, ref_entity, l10n_entity):
        # overload this if needed
        pass


def compareProjects(
            project_configs,
            stat_observer=None,
            file_stats=False,
            merge_stage=None,
            clobber_merge=False,
            quiet=0,
        ):
    locales = set()
    observers = []
    for project in project_configs:
        # disable filter if we're in validation mode
        if None in project.locales:
            filter = None
        else:
            filter = project.filter
        observers.append(
            Observer(
                quiet=quiet,
                filter=filter,
                file_stats=file_stats,
            ))
        locales.update(project.locales)
    if stat_observer is not None:
        stat_observers = [stat_observer]
    else:
        stat_observers = None
    comparer = ContentComparer(observers, stat_observers=stat_observers)
    for locale in sorted(locales):
        files = paths.ProjectFiles(locale, project_configs,
                                   mergebase=merge_stage)
        root = mozpath.commonprefix([m['l10n'].prefix for m in files.matchers])
        if merge_stage is not None:
            if clobber_merge:
                mergematchers = set(_m.get('merge') for _m in files.matchers)
                mergematchers.discard(None)
                for matcher in mergematchers:
                    clobberdir = matcher.prefix
                    if os.path.exists(clobberdir):
                        shutil.rmtree(clobberdir)
                        print("clobbered " + clobberdir)
        for l10npath, refpath, mergepath, extra_tests in files:
            # module and file path are needed for legacy filter.py support
            module = None
            fpath = mozpath.relpath(l10npath, root)
            for _m in files.matchers:
                if _m['l10n'].match(l10npath):
                    if _m['module']:
                        # legacy ini support, set module, and resolve
                        # local path against the matcher prefix,
                        # which includes the module
                        module = _m['module']
                        fpath = mozpath.relpath(l10npath, _m['l10n'].prefix)
                    break
            reffile = paths.File(refpath, fpath or refpath, module=module)
            if locale is None:
                # When validating the reference files, set locale
                # to a private subtag. This only shows in the output.
                locale = paths.REFERENCE_LOCALE
            l10n = paths.File(l10npath, fpath or l10npath,
                              module=module, locale=locale)
            if not os.path.exists(l10npath):
                comparer.add(reffile, l10n, mergepath)
                continue
            if not os.path.exists(refpath):
                comparer.remove(reffile, l10n, mergepath)
                continue
            comparer.compare(reffile, l10n, mergepath, extra_tests)
    return observers
