from registry import register_task_action


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
