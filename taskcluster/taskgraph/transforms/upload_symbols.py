# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the upload-symbols task description template,
taskcluster/ci/upload-symbols/job-template.yml into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import join_symbol

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def fill_template(config, tasks):
    for task in tasks:
        dep = task['dependent-task']

        # Fill out the dynamic fields in the task description
        task['label'] = dep.label + '-upload-symbols'

        # Skip tasks where we don't have the full crashsymbols enabled
        if not dep.attributes.get('enable-full-crashsymbols'):
            logger.debug("Skipping upload symbols task for %s", task['label'])
            continue

        task['dependencies'] = {'build': dep.label}
        task['worker']['env']['GECKO_HEAD_REPOSITORY'] = config.params['head_repository']
        task['worker']['env']['GECKO_HEAD_REV'] = config.params['head_rev']
        task['worker']['env']['SYMBOL_SECRET'] = task['worker']['env']['SYMBOL_SECRET'].format(
            level=config.params['level'])

        build_platform = dep.attributes.get('build_platform')
        build_type = dep.attributes.get('build_type')
        attributes = task.setdefault('attributes', {})
        attributes['build_platform'] = build_platform
        attributes['build_type'] = build_type
        if dep.attributes.get('nightly'):
            attributes['nightly'] = True

        treeherder = task.get('treeherder', {})
        th = dep.task.get('extra')['treeherder']
        th_platform = dep.task['extra'].get('treeherder-platform',
                                            "{}/{}".format(th['machine']['platform'], build_type))
        th_symbol = th.get('symbol')
        th_groupsymbol = th.get('groupSymbol', '?')
        treeherder.setdefault('platform', th_platform)
        treeherder.setdefault('tier', th['tier'])
        treeherder.setdefault('kind', th['jobKind'])

        # Disambiguate the treeherder symbol.
        sym = 'Sym' + (th_symbol[1:] if th_symbol.startswith('B') else th_symbol)
        treeherder.setdefault(
            'symbol', join_symbol(th_groupsymbol, sym)
        )
        task['treeherder'] = treeherder

        # clear out the stuff that's not part of a task description
        del task['dependent-task']

        yield task
