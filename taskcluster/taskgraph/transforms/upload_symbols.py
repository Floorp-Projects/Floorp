# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the upload-symbols task description template,
taskcluster/ci/upload-symbols/job-template.yml into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import RELEASE_PROJECTS, copy_attributes_from_dependent_job
from taskgraph.util.treeherder import join_symbol

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def check_nightlies(config, tasks):
    """Ensure that we upload symbols for all nightly builds, so that crash-stats can
    resolve any reports sent to it. Try may enable full symbols but not upload them.

    Putting this check here (instead of the transforms for the build kind) lets us
    leverage the any not-for-build-platforms set in the update-symbols kind."""
    for task in tasks:
        dep = task['primary-dependency']
        if config.params['project'] in RELEASE_PROJECTS and \
                dep.attributes.get('nightly', dep.attributes.get('shippable')) and \
                not dep.attributes.get('enable-full-crashsymbols'):
            raise Exception('Nightly job %s should have enable-full-crashsymbols attribute '
                            'set to true to enable symbol upload to crash-stats' % dep.label)
        yield task


@transforms.add
def fill_template(config, tasks):
    for task in tasks:
        dep = task['primary-dependency']

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

        attributes = copy_attributes_from_dependent_job(dep)
        attributes.update(task.get('attributes', {}))
        task['attributes'] = attributes

        build_type = attributes.get('build_type')

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

        if dep.attributes.get('nightly'):
            # For nightly builds, we want to run these tasks if the build is run.
            task['run-on-projects'] = dep.attributes.get('run_on_projects')
        elif dep.attributes.get('shippable'):
            # For shippable builds, we want to run these tasks if the build is run.
            # XXX Better to run this on promote phase instead?
            task['run-on-projects'] = dep.attributes.get('run_on_projects')
        else:
            # For other builds, these can be requested to upload to the try symbol sever.
            task['run-on-projects'] = ['try']

        # clear out the stuff that's not part of a task description
        del task['primary-dependency']

        yield task
