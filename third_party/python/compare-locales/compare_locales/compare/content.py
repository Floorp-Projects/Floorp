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

from compare_locales import parser
from compare_locales import mozpath
from compare_locales.checks import getChecker
from compare_locales.keyedtuple import KeyedTuple

from .observer import ObserverList
from .utils import AddRemove


class ContentComparer:
    keyRE = re.compile('[kK]ey')
    nl = re.compile('\n', re.M)

    def __init__(self, quiet=0):
        '''Create a ContentComparer.
        observer is usually a instance of Observer. The return values
        of the notify method are used to control the handling of missing
        entities.
        '''
        self.observers = ObserverList(quiet=quiet)

    def create_merge_dir(self, merge_file):
        outdir = mozpath.dirname(merge_file)
        if not os.path.isdir(outdir):
            os.makedirs(outdir)

    def merge(self, ref_entities, ref_file, l10n_file, merge_file,
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
                        [ref_entities[key].all for key in missing] +
                        [ref_entities[skip.key].all for skip in skips
                         if not isinstance(skip, parser.Junk)])

            def ensureNewline(s):
                if not s.endswith('\n'):
                    return s + '\n'
                return s

            print("adding to " + merge_file)
            f.write(''.join(map(ensureNewline, trailing)))

        if f is not None:
            f.close()

    def remove(self, ref_file, l10n, merge_file):
        '''Obsolete l10n file.

        Copy to merge stage if we can.
        '''
        self.observers.notify('obsoleteFile', l10n, None)
        self.merge(
            KeyedTuple([]), ref_file, l10n, merge_file,
            [], [], None, parser.CAN_COPY, None
        )

    def compare(self, ref_file, l10n, merge_file, extra_tests=None):
        try:
            p = parser.getParser(ref_file.file)
        except UserWarning:
            # no comparison, XXX report?
            # At least, merge
            self.merge(
                KeyedTuple([]), ref_file, l10n, merge_file, [], [], None,
                parser.CAN_COPY, None)
            return
        try:
            p.readFile(ref_file)
        except Exception as e:
            self.observers.notify('error', ref_file, str(e))
            return
        ref_entities = p.parse()
        try:
            p.readFile(l10n)
            l10n_entities = p.parse()
            l10n_ctx = p.ctx
        except Exception as e:
            self.observers.notify('error', l10n, str(e))
            return

        ar = AddRemove()
        ar.set_left(ref_entities.keys())
        ar.set_right(l10n_entities.keys())
        report = missing = obsolete = changed = unchanged = keys = 0
        missing_w = changed_w = unchanged_w = 0  # word stats
        missings = []
        skips = []
        checker = getChecker(l10n, extra_tests=extra_tests)
        if checker and checker.needs_reference:
            checker.set_reference(ref_entities)
        for msg in p.findDuplicates(ref_entities):
            self.observers.notify('warning', l10n, msg)
        for msg in p.findDuplicates(l10n_entities):
            self.observers.notify('error', l10n, msg)
        for action, entity_id in ar:
            if action == 'delete':
                # missing entity
                if isinstance(ref_entities[entity_id], parser.Junk):
                    self.observers.notify(
                        'warning', l10n, 'Parser error in en-US'
                    )
                    continue
                _rv = self.observers.notify('missingEntity', l10n, entity_id)
                if _rv == "ignore":
                    continue
                if _rv == "error":
                    # only add to missing entities for l10n-merge on error,
                    # not report
                    missings.append(entity_id)
                    missing += 1
                    refent = ref_entities[entity_id]
                    missing_w += refent.count_words()
                else:
                    # just report
                    report += 1
            elif action == 'add':
                # obsolete entity or junk
                if isinstance(l10n_entities[entity_id],
                              parser.Junk):
                    junk = l10n_entities[entity_id]
                    self.observers.notify(
                        'error', l10n,
                        junk.error_message()
                    )
                    if merge_file is not None:
                        skips.append(junk)
                elif (
                    self.observers.notify('obsoleteEntity', l10n, entity_id)
                    != 'ignore'
                ):
                    obsolete += 1
            else:
                # entity found in both ref and l10n, check for changed
                refent = ref_entities[entity_id]
                l10nent = l10n_entities[entity_id]
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
                        self.observers.notify(
                            tp, l10n,
                            u"%s at line %d, column %d for %s" %
                            (msg, line, col, refent.key)
                        )
                pass

        if merge_file is not None:
            self.merge(
                ref_entities, ref_file,
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
        self.observers.updateStats(l10n, stats)
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
                KeyedTuple([]), orig, missing, merge_file,
                ['trigger copy'], [], None, parser.CAN_COPY, None
            )

        if self.observers.notify('missingFile', missing, None) == "ignore":
            # filter said that we don't need this file, don't count it
            return

        if p is None:
            # We don't have a parser, cannot count missing strings
            return

        try:
            p.readFile(f)
            entities = p.parse()
        except Exception as ex:
            self.observers.notify('error', f, str(ex))
            return
        # strip parse errors
        entities = [e for e in entities if not isinstance(e, parser.Junk)]
        self.observers.updateStats(missing, {'missingInFiles': len(entities)})
        missing_w = 0
        for e in entities:
            missing_w += e.count_words()
        self.observers.updateStats(missing, {'missing_w': missing_w})

    def doUnchanged(self, entity):
        # overload this if needed
        pass

    def doChanged(self, file, ref_entity, l10n_entity):
        # overload this if needed
        pass
