#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import argparse
import os
import re

from mozharness.base.script import PostScriptAction
from mozharness.base.transfer import TransferMixin


class TryToolsMixin(TransferMixin):
    """Utility functions for an interface between try syntax and out test harnesses.
    Requires log and script mixins."""

    harness_extra_args = None
    try_test_paths = []
    known_try_arguments = {
        '--tag': {
            'action': 'append',
            'dest': 'tags',
            'default': None,
        },
    }

    def _extract_try_message(self):
        msg = self.buildbot_config['sourcestamp']['changes'][-1]['comments']
        if len(msg) == 1024:
            # This commit message was potentially truncated, get the full message
            # from hg.
            props = self.buildbot_config['properties']
            rev = props['revision']
            repo = props['repo_path']
            url = 'https://hg.mozilla.org/%s/json-pushes?changeset=%s&full=1' % (repo, rev)

            pushinfo = self.load_json_from_url(url)
            for k, v in pushinfo.items():
                if isinstance(v, dict) and 'changesets' in v:
                    msg = v['changesets'][-1]['desc']

        if not msg and 'try_syntax' in self.buildbot_config['properties']:
            # If we don't find try syntax in the usual place, check for it in an
            # alternate property available to tools using self-serve.
            msg = self.buildbot_config['properties']['try_syntax']

        return msg

    @PostScriptAction('download-and-extract')
    def _set_extra_try_arguments(self, action, success=None):
        """Finds a commit message and parses it for extra arguments to pass to the test
        harness command line and test paths used to filter manifests.

        Extracting arguments from a commit message taken directly from the try_parser.
        """
        if (not self.buildbot_config or 'properties' not in self.buildbot_config or
                self.buildbot_config['properties'].get('branch') != 'try'):
            return

        msg = self._extract_try_message()
        if not msg:
            return

        all_try_args = None
        for line in msg.splitlines():
            if 'try: ' in line:
                # Autoland adds quotes to try strings that will confuse our
                # args later on.
                if line.startswith('"') and line.endswith('"'):
                    line = line[1:-1]
                # Allow spaces inside of [filter expressions]
                try_message = line.strip().split('try: ', 1)
                all_try_args = re.findall(r'(?:\[.*?\]|\S)+', try_message[1])
                break

        if not all_try_args:
            self.warning('Try syntax not found in buildbot config, unable to append '
                         'arguments from try.')
            return


        parser = argparse.ArgumentParser(
            description=('Parse an additional subset of arguments passed to try syntax'
                         ' and forward them to the underlying test harness command.'))

        label_dict = {}
        def label_from_val(val):
            if val in label_dict:
                return label_dict[val]
            return '--%s' % val.replace('_', '-')

        for label, opts in self.known_try_arguments.iteritems():
            if 'action' in opts and opts['action'] not in ('append', 'store',
                                                           'store_true', 'store_false'):
                self.fatal('Try syntax does not support passing custom or store_const '
                           'arguments to the harness process.')
            if 'dest' in opts:
                label_dict[opts['dest']] = label

            parser.add_argument(label, **opts)

        parser.add_argument('--try-test-paths', nargs='*')
        (args, _) = parser.parse_known_args(all_try_args)
        self.try_test_paths = args.try_test_paths
        del args.try_test_paths

        out_args = []
        # This is a pretty hacky way to echo arguments down to the harness.
        # Hopefully this can be improved once we have a configuration system
        # in tree for harnesses that relies less on a command line.
        for (arg, value) in vars(args).iteritems():
            if value:
                label = label_from_val(arg)
                if isinstance(value, bool):
                    # A store_true or store_false argument.
                    out_args.append(label)
                elif isinstance(value, list):
                    out_args.extend(['%s=%s' % (label, el) for el in value])
                else:
                    out_args.append('%s=%s' % (label, value))

        self.harness_extra_args = out_args

    def _resolve_specified_manifests(self):
        if not self.try_test_paths:
            return None

        target_manifests = set(self.try_test_paths)

        def filter_ini_manifest(line):
            # Lines are formatted as [include:<path>], we care about <path>.
            parts = line.split(':')
            term = line
            if len(parts) == 2:
                term = parts[1]
            if term.endswith(']'):
                term = term[:-1]
            if (term in target_manifests or
                any(term.startswith(l) for l in target_manifests)):
                return True
            return False

        def filter_list_manifest(line):
            # Lines are usually formatted as "include <path>", we care about <path>.
            parts = line.split()
            term = line
            if len(parts) == 2:
                term = parts[1]
            # Reftest master manifests also include skip-if lines and relative
            # paths we aren't doing to resolve here, so unlike above this is just
            # a substring check.
            if (term in target_manifests or
                any(l in term for l in target_manifests)):
                return True
            return False

        # The master manifests we need to filter for target manifests.
        # TODO: All this needs to go in a config file somewhere and get sewn
        # into the job definition so its less likely to break as things are
        # modified. One straightforward way to achieve this would be with a key
        # in the tree manifest for the master manifest path, however the current
        # tree manifests don't distinguish between flavors of mochitests to this
        # isn't straightforward.
        master_manifests = [
            ('mochitest/chrome/chrome.ini', filter_ini_manifest),
            ('mochitest/browser/browser-chrome.ini', filter_ini_manifest),
            ('mochitest/tests/mochitest.ini', filter_ini_manifest),
            ('xpcshell/tests/all-test-dirs.list', filter_ini_manifest),
            ('xpcshell/tests/xpcshell.ini', filter_ini_manifest),
            ('reftest/tests/layout/reftests/reftest.list', filter_list_manifest),
            ('reftest/tests/testing/crashtest/crashtests.list', filter_list_manifest),
        ]

        dirs = self.query_abs_dirs()
        tests_dir = dirs.get('abs_test_install_dir',
                             os.path.join(dirs['abs_work_dir'], 'tests'))
        master_manifests = [(os.path.join(tests_dir, name), filter_fn) for (name, filter_fn) in
                            master_manifests]
        for m, filter_fn in master_manifests:
            if not os.path.isfile(m):
                continue

            self.info("Filtering master manifest at: %s" % m)
            lines = self.read_from_file(m).splitlines()

            out_lines = [line for line in lines if filter_fn(line)]

            self.rmtree(m)
            self.write_to_file(m, '\n'.join(out_lines))


    def append_harness_extra_args(self, cmd):
        """Append arguments derived from try syntax to a command."""
        # TODO: Detect and reject incompatible arguments
        extra_args = self.harness_extra_args[:] if self.harness_extra_args else []
        if self.try_test_paths:
            self._resolve_specified_manifests()
            self.info('TinderboxPrint: Tests will be run from the following '
                      'manifests: %s.' % ','.join(self.try_test_paths))
            extra_args.extend(['--this-chunk=1', '--total-chunks=1'])

        if not extra_args:
            return cmd

        out_cmd = cmd[:]
        out_cmd.extend(extra_args)
        self.info('TinderboxPrint: The following arguments were forwarded from mozharness '
                  'to the test command:\nTinderboxPrint: \t%s' %
                  extra_args)

        return out_cmd
