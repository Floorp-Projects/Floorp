# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Do transforms specific to l10n kind
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def mh_config_replace_project(config, jobs):
    """ Replaces {project} in mh config entries with the current project """
    # XXXCallek This is a bad pattern but exists to satisfy ease-of-porting for buildbot
    for job in jobs:
        if not job['run'].get('using') == 'mozharness':
            # Nothing to do, not mozharness
            yield job
            continue
        job['run']['config'] = map(
            lambda x: x.format(project=config.params['project']),
            job['run']['config']
            )
        yield job


@transforms.add
def mh_options_replace_project(config, jobs):
    """ Replaces {project} in mh option entries with the current project """
    # XXXCallek This is a bad pattern but exists to satisfy ease-of-porting for buildbot
    for job in jobs:
        if not job['run'].get('using') == 'mozharness':
            # Nothing to do, not mozharness
            yield job
            continue
        job['run']['options'] = map(
            lambda x: x.format(project=config.params['project']),
            job['run']['options']
            )
        yield job
