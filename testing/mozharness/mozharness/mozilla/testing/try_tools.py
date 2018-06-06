#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import argparse
import os
import re
from collections import defaultdict

from mozharness.base.script import PostScriptAction
from mozharness.base.transfer import TransferMixin

try_config_options = [
    [["--try-message"],
     {"action": "store",
      "dest": "try_message",
      "default": None,
      "help": "try syntax string to select tests to run",
      }],
]

test_flavors = {
    'browser-chrome': {},
    'chrome': {},
    'devtools-chrome': {},
    'mochitest': {},
    'xpcshell': {},
    'reftest': {
        "path": lambda x: os.path.join("tests", "reftest", "tests", x)
    },
    'crashtest': {
        "path": lambda x: os.path.join("tests", "reftest", "tests", x)
    },
    'web-platform-tests': {
        "path": lambda x: os.path.join("tests", x.split("testing" + os.path.sep)[1])
    },
    'web-platform-tests-reftests': {
        "path": lambda x: os.path.join("tests", x.split("testing" + os.path.sep)[1])
    },
    'web-platform-tests-wdspec': {
        "path": lambda x: os.path.join("tests", x.split("testing" + os.path.sep)[1])
    },
}


class TryToolsMixin(TransferMixin):
    """Utility functions for an interface between try syntax and out test harnesses.
    Requires log and script mixins."""

    harness_extra_args = None
    try_test_paths = {}
    known_try_arguments = {
        '--tag': ({
            'action': 'append',
            'dest': 'tags',
            'default': None,
        }, (
            'browser-chrome',
            'chrome',
            'devtools-chrome',
            'marionette',
            'mochitest',
            'web-plaftform-tests',
            'xpcshell',
        )),
        '--setenv': ({
            'action': 'append',
            'dest': 'setenv',
            'default': [],
            'metavar': 'NAME=VALUE',
        }, (
            'browser-chrome',
            'chrome',
            'crashtest',
            'devtools-chrome',
            'mochitest',
            'reftest',
        )),
    }

    def _extract_try_message(self):
        msg = None
        if "try_message" in self.config and self.config["try_message"]:
            msg = self.config["try_message"]
        elif 'TRY_COMMIT_MSG' in os.environ:
            msg = os.environ['TRY_COMMIT_MSG']

        if not msg:
            self.warning('Try message not found.')
        return msg

    def _extract_try_args(self, msg):
        """ Returns a list of args from a try message, for parsing """
        if not msg:
            return None
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
            self.warning('Try syntax not found in: %s.' % msg)
        return all_try_args

    def try_message_has_flag(self, flag, message=None):
        """
        Returns True if --`flag` is present in message.
        """
        parser = argparse.ArgumentParser()
        parser.add_argument('--' + flag, action='store_true')
        message = message or self._extract_try_message()
        if not message:
            return False
        msg_list = self._extract_try_args(message)
        args, _ = parser.parse_known_args(msg_list)
        return getattr(args, flag, False)

    def _is_try(self):
        repo_path = None
        get_branch = self.config.get('branch', repo_path)
        if get_branch is not None:
            on_try = ('try' in get_branch or 'Try' in get_branch)
        elif os.environ is not None:
            on_try = ('TRY_COMMIT_MSG' in os.environ)
        else:
            on_try = False
        return on_try

    @PostScriptAction('download-and-extract')
    def set_extra_try_arguments(self, action, success=None):
        """Finds a commit message and parses it for extra arguments to pass to the test
        harness command line and test paths used to filter manifests.

        Extracting arguments from a commit message taken directly from the try_parser.
        """
        if not self._is_try():
            return

        msg = self._extract_try_message()
        if not msg:
            return

        all_try_args = self._extract_try_args(msg)
        if not all_try_args:
            return

        parser = argparse.ArgumentParser(
            description=('Parse an additional subset of arguments passed to try syntax'
                         ' and forward them to the underlying test harness command.'))

        label_dict = {}

        def label_from_val(val):
            if val in label_dict:
                return label_dict[val]
            return '--%s' % val.replace('_', '-')

        for label, (opts, _) in self.known_try_arguments.iteritems():
            if 'action' in opts and opts['action'] not in ('append', 'store',
                                                           'store_true', 'store_false'):
                self.fatal('Try syntax does not support passing custom or store_const '
                           'arguments to the harness process.')
            if 'dest' in opts:
                label_dict[opts['dest']] = label

            parser.add_argument(label, **opts)

        parser.add_argument('--try-test-paths', nargs='*')
        (args, _) = parser.parse_known_args(all_try_args)
        self.try_test_paths = self._group_test_paths(args.try_test_paths)
        del args.try_test_paths

        out_args = defaultdict(list)
        # This is a pretty hacky way to echo arguments down to the harness.
        # Hopefully this can be improved once we have a configuration system
        # in tree for harnesses that relies less on a command line.
        for arg, value in vars(args).iteritems():
            if value:
                label = label_from_val(arg)
                _, flavors = self.known_try_arguments[label]

                for f in flavors:
                    if isinstance(value, bool):
                        # A store_true or store_false argument.
                        out_args[f].append(label)
                    elif isinstance(value, list):
                        out_args[f].extend(['%s=%s' % (label, el) for el in value])
                    else:
                        out_args[f].append('%s=%s' % (label, value))

        self.harness_extra_args = dict(out_args)

    def _group_test_paths(self, args):
        rv = defaultdict(list)

        if args is None:
            return rv

        for item in args:
            suite, path = item.split(":", 1)
            rv[suite].append(path)
        return rv

    def try_args(self, flavor):
        """Get arguments, test_list derived from try syntax to apply to a command"""
        args = []
        if self.harness_extra_args:
            args = self.harness_extra_args.get(flavor, [])[:]

        if self.try_test_paths.get(flavor):
            self.info('TinderboxPrint: Tests will be run from the following '
                      'files: %s.' % ','.join(self.try_test_paths[flavor]))
            args.extend(['--this-chunk=1', '--total-chunks=1'])

            path_func = test_flavors[flavor].get("path", lambda x: x)
            tests = [path_func(os.path.normpath(item)) for item in self.try_test_paths[flavor]]
        else:
            tests = []

        if args or tests:
            self.info('TinderboxPrint: The following arguments were forwarded from mozharness '
                      'to the test command:\nTinderboxPrint: \t%s -- %s' %
                      (" ".join(args), " ".join(tests)))

        return args, tests
