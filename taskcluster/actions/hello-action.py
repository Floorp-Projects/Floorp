# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .registry import register_callback_action


@register_callback_action(
    name='hello',
    title='Say Hello',
    symbol='hw',
    description="""
    Simple **proof-of-concept** action that prints a hello action.
    """,
    order=10000,  # Put this at the very bottom/end of any menu (default)
    context=[{}],  # Applies to any task
    available=lambda parameters: True,  # available regardless decision parameters (default)
    schema={
        'type': 'string',
        'maxLength': 255,
        'default': 'World',
        'title': 'Target Name',
        'description': """
A name wish to say hello to...
This should normally be **your name**.

But you can also use the default value `'World'`.
        """.strip(),
    },
)
def hello_world_action(parameters, input, task_group_id, task_id, task):
    print("This message was triggered from context-menu of taskId: {}".format(task_id))
    print("")
    print("Hello {}".format(input))
    print("--- Action is now executed")
