# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

import sys
import logging

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import MachCommandBase
from mozilla_version.gecko import GeckoVersion


@CommandProvider
class MachCommands(MachCommandBase):

    @Command('release', category="release",
             description="Task that are part of the release process.")
    def release(self):
        """
        The release subcommands all relate to the release process.
        """

    @SubCommand('release', 'buglist',
                description="Generate list of bugs since the last release.")
    @CommandArgument('--version',
                     required=True,
                     type=GeckoVersion.parse,
                     help="The version being built.")
    @CommandArgument('--product',
                     required=True,
                     help="The product being built.")
    @CommandArgument('--repo',
                     help="The repo being built.")
    @CommandArgument('--revision',
                     required=True,
                     help="The revision being built.")
    def buglist(self, version, product, revision, repo):
        self.setup_logging()
        from mozrelease.buglist_creator import create_bugs_url
        print(create_bugs_url(
            product=product,
            current_version=version,
            current_revision=revision,
            repo=repo,
        ))

    @SubCommand('release', 'send-buglist-email',
                description="Send an email with the bugs since the last release.")
    @CommandArgument('--address',
                     required=True,
                     action='append',
                     dest='addresses',
                     help="The email address to send the bug list to "
                          "(may be specified more than once.")
    @CommandArgument('--version',
                     type=GeckoVersion.parse,
                     required=True,
                     help="The version being built.")
    @CommandArgument('--product',
                     required=True,
                     help="The product being built.")
    @CommandArgument('--repo',
                     required=True,
                     help="The repo being built.")
    @CommandArgument('--revision',
                     required=True,
                     help="The revision being built.")
    @CommandArgument('--build-number',
                     required=True,
                     help="The build number")
    @CommandArgument('--task-group-id',
                     help="The task group of the build.")
    def buglist_email(self, **options):
        self.setup_logging()
        from mozrelease.buglist_creator import email_release_drivers
        email_release_drivers(**options)

    def setup_logging(self, quiet=False, verbose=True):
        """
        Set up Python logging for all loggers, sending results to stderr (so
        that command output can be redirected easily) and adding the typical
        mach timestamp.
        """
        # remove the old terminal handler
        old = self.log_manager.replace_terminal_handler(None)

        # re-add it, with level and fh set appropriately
        if not quiet:
            level = logging.DEBUG if verbose else logging.INFO
            self.log_manager.add_terminal_logging(
                fh=sys.stderr, level=level,
                write_interval=old.formatter.write_interval,
                write_times=old.formatter.write_times)

        # all of the taskgraph logging is unstructured logging
        self.log_manager.enable_unstructured()
