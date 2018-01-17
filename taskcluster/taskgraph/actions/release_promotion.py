# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import os

from .registry import register_callback_action

from .util import (find_decision_task, find_existing_tasks_from_previous_kinds,
                   find_hg_revision_pushlog_id)
from taskgraph.util.taskcluster import get_artifact
from taskgraph.util.partials import populate_release_history
from taskgraph.taskgraph import TaskGraph
from taskgraph.decision import taskgraph_decision
from taskgraph.parameters import Parameters
from taskgraph.util.attributes import RELEASE_PROMOTION_PROJECTS

RELEASE_PROMOTION_CONFIG = {
    'promote_fennec': {
        'target_tasks_method': 'promote_fennec',
    },
    'ship_fennec': {
        'target_tasks_method': 'ship_fennec',
    },
    'promote_firefox': {
        'target_tasks_method': 'promote_firefox',
    },
    'push_firefox': {
        'target_tasks_method': 'push_firefox',
    },
    'ship_firefox': {
        'target_tasks_method': 'ship_firefox',
    },
    'promote_devedition': {
        'target_tasks_method': 'promote_devedition',
    },
    'push_devedition': {
        'target_tasks_method': 'push_devedition',
    },
    'ship_devedition': {
        'target_tasks_method': 'ship_devedition',
    },
}

VERSION_BUMP_FLAVORS = (
    'ship_fennec',
    'ship_firefox',
    'ship_devedition',
)

UPTAKE_MONITORING_PLATFORMS_FLAVORS = (
    'push_firefox',
    'push_devedition',
)

PARTIAL_UPDATES_FLAVORS = UPTAKE_MONITORING_PLATFORMS_FLAVORS + (
    'promote_firefox',
    'promote_devedition',
)

DESKTOP_RELEASE_TYPE_FLAVORS = (
    'promote_firefox',
    'push_firefox',
    'ship_firefox',
    'promote_devedition',
    'push_devedition',
    'ship_devedition',
)


VALID_DESKTOP_RELEASE_TYPES = (
    'beta',
    'devedition',
    'esr',
    'release',
    'rc',
)


def is_release_promotion_available(parameters):
    return parameters['project'] in RELEASE_PROMOTION_PROJECTS


@register_callback_action(
    name='release-promotion',
    title='Release Promotion',
    symbol='${input.release_promotion_flavor}',
    description="Promote a release.",
    order=10000,
    context=[],
    available=is_release_promotion_available,
    schema={
        'type': 'object',
        'properties': {
            'build_number': {
                'type': 'integer',
                'default': 1,
                'minimum': 1,
                'title': 'The release build number',
                'description': ('The release build number. Starts at 1 per '
                                'release version, and increments on rebuild.'),
            },
            'do_not_optimize': {
                'type': 'array',
                'description': ('Optional: a list of labels to avoid optimizing out '
                                'of the graph (to force a rerun of, say, '
                                'funsize docker-image tasks).'),
                'items': {
                    'type': 'string',
                },
            },
            'revision': {
                'type': 'string',
                'title': 'Optional: revision to promote',
                'description': ('Optional: the revision to promote. If specified, '
                                'and if neither `pushlog_id` nor `previous_graph_kinds` '
                                'is specified, find the `pushlog_id using the '
                                'revision.'),
            },
            'release_promotion_flavor': {
                'type': 'string',
                'description': 'The flavor of release promotion to perform.',
                'enum': sorted(RELEASE_PROMOTION_CONFIG.keys()),
            },
            'target_tasks_method': {
                'type': 'string',
                'title': 'target task method',
                'description': ('Optional: the target task method to use to generate '
                                'the new graph.'),
            },
            'rebuild_kinds': {
                'type': 'array',
                'description': ('Optional: an array of kinds to ignore from the previous '
                                'graph(s).'),
                'items': {
                    'type': 'string',
                },
            },
            'previous_graph_ids': {
                'type': 'array',
                'description': ('Optional: an array of taskIds of decision or action '
                                'tasks from the previous graph(s) to use to populate '
                                'our `previous_graph_kinds`.'),
                'items': {
                    'type': 'string',
                },
            },
            'next_version': {
                'type': 'string',
                'description': 'Next version.',
                'default': '',
            },

            # Example:
            #   'partial_updates': {
            #       '38.0': {
            #           'buildNumber': 1,
            #           'locales': ['de', 'en-GB', 'ru', 'uk', 'zh-TW']
            #       },
            #       '37.0': {
            #           'buildNumber': 2,
            #           'locales': ['de', 'en-GB', 'ru', 'uk']
            #       }
            #   }
            'partial_updates': {
                'type': 'object',
                'description': 'Partial updates.',
                'default': {},
                'additionalProperties': {
                    'type': 'object',
                    'properties': {
                        'buildNumber': {
                            'type': 'number',
                        },
                        'locales': {
                            'type': 'array',
                            'items':  {
                                'type': 'string',
                            },
                        },
                    },
                    'required': [
                        'buildNumber',
                        'locales',
                    ],
                    'additionalProperties': False,
                }
            },

            'uptake_monitoring_platforms': {
                'type': 'array',
                'items': {
                    'type': 'string',
                    'enum': [
                        'macosx',
                        'win32',
                        'win64',
                        'linux',
                        'linux64',
                    ],
                },
                'default': [],
            },

            'desktop_release_type': {
                'type': 'string',
                'default': '',
            },
        },
        "required": ['release_promotion_flavor', 'build_number'],
    }
)
def release_promotion_action(parameters, input, task_group_id, task_id, task):
    release_promotion_flavor = input['release_promotion_flavor']
    release_history = {}
    desktop_release_type = None

    next_version = str(input.get('next_version') or '')
    if release_promotion_flavor in VERSION_BUMP_FLAVORS:
        # We force str() the input, hence the 'None'
        if next_version in ['', 'None']:
            raise Exception(
                "`next_version` property needs to be provided for %s "
                "targets." % ', '.join(VERSION_BUMP_FLAVORS)
            )

    if release_promotion_flavor in DESKTOP_RELEASE_TYPE_FLAVORS:
        desktop_release_type = input.get('desktop_release_type', None)
        if desktop_release_type not in VALID_DESKTOP_RELEASE_TYPES:
            raise Exception("`desktop_release_type` must be one of: %s" %
                            ", ".join(VALID_DESKTOP_RELEASE_TYPES))

        if release_promotion_flavor in PARTIAL_UPDATES_FLAVORS:
            partial_updates = json.dumps(input.get('partial_updates', {}))
            if partial_updates == "{}":
                raise Exception(
                    "`partial_updates` property needs to be provided for %s "
                    "targets." % ', '.join(PARTIAL_UPDATES_FLAVORS)
                )
            balrog_prefix = 'Firefox'
            if desktop_release_type == 'devedition':
                balrog_prefix = 'Devedition'
            os.environ['PARTIAL_UPDATES'] = partial_updates
            release_history = populate_release_history(
                balrog_prefix, parameters['project'],
                partial_updates=input['partial_updates']
            )

        if release_promotion_flavor in UPTAKE_MONITORING_PLATFORMS_FLAVORS:
            uptake_monitoring_platforms = json.dumps(input.get('uptake_monitoring_platforms', []))
            if partial_updates == "[]":
                raise Exception(
                    "`uptake_monitoring_platforms` property needs to be provided for %s "
                    "targets." % ', '.join(UPTAKE_MONITORING_PLATFORMS_FLAVORS)
                )
            os.environ['UPTAKE_MONITORING_PLATFORMS'] = uptake_monitoring_platforms

    promotion_config = RELEASE_PROMOTION_CONFIG[release_promotion_flavor]

    target_tasks_method = input.get(
        'target_tasks_method',
        promotion_config['target_tasks_method'].format(project=parameters['project'])
    )
    rebuild_kinds = input.get(
        'rebuild_kinds', promotion_config.get('rebuild_kinds', [])
    )
    do_not_optimize = input.get(
        'do_not_optimize', promotion_config.get('do_not_optimize', [])
    )

    # make parameters read-write
    parameters = dict(parameters)
    # Build previous_graph_ids from ``previous_graph_ids``, ``pushlog_id``,
    # or ``revision``.
    previous_graph_ids = input.get('previous_graph_ids')
    if not previous_graph_ids:
        revision = input.get('revision')
        parameters['pushlog_id'] = parameters['pushlog_id'] or \
            find_hg_revision_pushlog_id(parameters, revision)
        previous_graph_ids = [find_decision_task(parameters)]

    # Download parameters from the first decision task
    parameters = get_artifact(previous_graph_ids[0], "public/parameters.yml")
    # Download and combine full task graphs from each of the previous_graph_ids.
    # Sometimes previous relpro action tasks will add tasks, like partials,
    # that didn't exist in the first full_task_graph, so combining them is
    # important. The rightmost graph should take precedence in the case of
    # conflicts.
    combined_full_task_graph = {}
    for graph_id in previous_graph_ids:
        full_task_graph = get_artifact(graph_id, "public/full-task-graph.json")
        combined_full_task_graph.update(full_task_graph)
    _, combined_full_task_graph = TaskGraph.from_json(combined_full_task_graph)
    parameters['existing_tasks'] = find_existing_tasks_from_previous_kinds(
        combined_full_task_graph, previous_graph_ids, rebuild_kinds
    )
    parameters['do_not_optimize'] = do_not_optimize
    parameters['target_tasks_method'] = target_tasks_method
    parameters['build_number'] = int(input['build_number'])
    parameters['next_version'] = next_version
    parameters['release_history'] = release_history
    parameters['desktop_release_type'] = desktop_release_type

    # make parameters read-only
    parameters = Parameters(**parameters)

    taskgraph_decision({}, parameters=parameters)
