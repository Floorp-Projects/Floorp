# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .registry import register_task_action


@register_task_action(
    title='Retrigger',
    name='retrigger',
    description='Create a clone of the task',
    order=1,
    context=[{}],
)
def retrigger_task_builder(parameters):

    new_expires = '30 days'

    return {
        '$merge': [
            {'$eval': 'task'},
            {'created': {'$fromNow': ''}},
            {'deadline': {'$fromNow': '1 day'}},
            {'expires': {'$fromNow': new_expires}},
            {'payload': {
                '$merge': [
                    {'$eval': 'task.payload'},
                    {
                        '$if': '"artifacts" in task.payload',
                        'then': {
                            'artifacts': {
                                '$if': 'typeof(task.payload.artifacts) == "object"',
                                'then': {
                                    '$map': {'$eval': 'task.payload.artifacts'},
                                    'each(artifact)': {
                                        '${artifact.key}': {
                                            '$merge': [
                                                {'$eval': 'artifact.val'},
                                                {'expires': {'$fromNow': new_expires}},
                                            ],
                                        },
                                    },
                                },
                                'else': {
                                    '$map': {'$eval': 'task.payload.artifacts'},
                                    'each(artifact)': {
                                        '$merge': [
                                            {'$eval': 'artifact'},
                                            {'expires': {'$fromNow': new_expires}},
                                        ],
                                    },
                                },
                            },
                        },
                        'else': {},
                    }
                ]
            }}
        ]
    }
