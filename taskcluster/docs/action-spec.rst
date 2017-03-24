Action Specification
====================
This document specifies how actions exposed by the *decision task* are to be
presented and triggered from Treeherder, or similar user interfaces.

The *decision task* creates an artifact ``public/actions.json`` to be consumed
by a user interface such as Treeherder. The ``public/actions.json`` file
specifies actions that can be triggered such as:

 * Retrigger a task,
 * Retry specific test cases many times,
 * Obtain a loaner machine,
 * Back-fill missing tasks,
 * ...

Through the ``public/actions.json`` file it is possible expose actions defined
in-tree such that the actions can be conveniently triggered in Treeherder.
This has two purposes:

 1. Facilitate development of utility actions/tools in-tree, and,
 2. Strongly decouple build/test configuration from Treeherder.

For details on how define custom actions in-tree, refer to
:doc:`the in-tree actions section <action-details>`. This document merely
specifies how ``actions.json`` shall be interpreted.


Specification of Actions
------------------------
The *decision task* creates an artifact ``public/actions.json`` which contains
a list of actions to be presented in the user-interface.


Variables
---------
The ``public/actions.json`` artifact has a ``variables`` property that is a
mapping from variable names to JSON values to be used as constants.
These variables can be referenced from task templates, but beware that they
may overshadow builtin variables. This is mainly useful to deduplicate commonly
used values, in order to reduce template size. This feature does not
introduce further expressiveness.


MetaData
--------
Each action entry must define a ``title``, ``description`` and ``kind``,
furthermore, the list of actions should be sorted by the order in which actions
should appear in a menu.

The ``title`` is a human readable string intended to be used as label on the
button, link or menu entry that triggers the action. This should be short and
concise, ideally you'll want to avoid duplicates.

The ``description`` property contains a human readable string describing the
action, such as what it does, how it does it, what it is useful for. This string
is to be render as **markdown**, allowing for bullet points, links and other
simple formatting to explain what the action does.

The ``kind`` property specifies what kind of action the entry defines.
At present only one kind of action is supported, the ``task`` kind.
See section on *Action Kind: ``task``* below for details.


Action Context
--------------
Few actions are relevant in all contexts. For this reason each action specifies
a ``context`` property. This property specifies when an action is relevant.
Actions *relevant* for a task should be displayed in a context menu for the
given task. Similarly actions *not relevant* for a given task, should not be
display in the context menu for the given task.

As a special case we say that actions for which *no relevant* contexts can
exist, are *relevant* for the task-group. This could for example be an action
to create tasks that was optimized away.

The ``context`` property is specified as a list of *tag-sets*. A *tag-set* is a
set of key-value pairs. A task is said to *match* a *tag-set* if ``task.tags``
is a super-set of the *tag-set*. An action is said to be *relevant* for a given
task, if ``task.tags`` *match* one of the *tag-sets* given in the ``context``
property for the action.

Naturally, it follows that an action with an empty list of *tag-sets* in its
``context`` property cannot possibly be *relevant* for any task. Hence, by
previously declared special case such an action is *relevant* for the
task-group.

**Examples**::

    // Example task definitions (everything but tags eclipsed)
    TaskA = {..., tags: {kind: 'test', platform: 'linux'}}
    TaskB = {..., tags: {kind: 'test', platform: 'windows'}}
    TaskC = {..., tags: {kind: 'build', platform: 'linux'}}

    Action1 = {..., context: [{kind: 'test'}]}
    // Action1 is relevant to: TaskA, TaskB

    Action2 = {..., context: [{kind: 'test', platform: 'linux'}]}
    // Action2 is relevant to: TaskA

    Action3 = {..., context: [{platform: 'linux'}]}
    // Action3 is relevant to: TaskA, TaskC

    Action4 = {..., context: [{kind: 'test'}, {kind: 'build'}]}
    // Action4 is relevant to: TaskA, TaskB, TaskC

    Action5 = {..., context: [{}]}
    // Action5 is relevant to: TaskA, TaskB, TaskC (all tasks in fact)

    Action6 = {..., context: []}
    // Action6 is relevant to the task-group


Action Input
------------
An action can take JSON input, the input format accepted by an action is
specified using a `JSON schema <http://json-schema.org/>`_. This schema is
specified with by the action's ``schema`` property.

User interfaces for triggering actions, like Treeherder, are expected to provide
JSON input that satisfies this schema. These interfaces are also expected to
validate the input against the schema before attempting to trigger the action.

It is expected that such user interfaces will attempt to auto-generate HTML
forms from JSON schema specified. However, a user-interface implementor may also
decide to hand write an HTML form for a particularly common or complex JSON
schema. As long as the input generated from the form conforms to the schema
specified for the given action. To ensure that, implementers should do a deep
comparison between a schema for which a hand-written HTML form exists, and the
schema required by the action.

It is perfectly legal to reference external schemas using
constructs like ``{"$ref": "https://example.com/my-schema.json"}``, in this case
it however strongly recommended that the external resource is available over
HTTPS and allows CORS requests from any source.

In fact, user interface implementors should feel encouraged to publish schemas
for which they have hand written input forms, so that action developers can
use these when applicable.

When writing schemas it is strongly encouraged that the JSON schema
``description`` properties are used to provide detailed descriptions. It is
assumed that consumers will render these ``description`` properties as markdown.


Action Kind: ``task``
---------------------

An action with ``kind: 'task'`` is backed by an action task. That is, when the
action is triggered by the user, the usre interface creates a new task,
referred to as an *action task*.  The task created by the action may be useful
in its own right, or it may simplify trigger in-tree scripts that create new
tasks, similar to a decision task. This way in-tree scripts can be triggered to
execute complicated actions such as backfilling.

Actions of the ``'task'`` *kind* **must** have a ``task`` property. This
property specifies the task template to be parameterized and created in order
to trigger the action.

The template is parameterized with the following variables:

``taskGroupId``
  the ``taskGroupId`` of task-group this is triggerd from,
``taskId``
  the ``taskId`` of the selected task, ``null`` if no task is
  selected (this is the case if the action has ``context: []``),
``task``
  the task definition of the selected task, ``null`` if no task is
  selected (this is the case if the action has ``context: []``), and,
``input``
  the input matching the ``schema`` property, ``null`` if the action
  doesn't have a ``schema`` property.
``<key>``
  Any ``<key>`` defined in the ``variables`` property may also be referenced.

The template is an object that is parameterized by:

1. Replacing substrings ``'${variable}'`` in strings and object keys
   with the value of the given ``variable``.
2. Replacing objects on the form ``{$eval: 'variable'}`` with the
   value of of the given ``variable``.
3. Replacing objects on the form ``{$fromNow: 'timespan'}`` with a
   timestamp of ``timespan`` from now. Where ``timespan`` is on the
   form: ``([0-9]+ *d(ays?)?)? *([0-9]+ *h(ours?)?)? *([0-9]+ *m(in(utes?)?)?)?``
4. Replacing any object on the form ``{$json: value}`` with the
   value of ``JSON.stringify(result)`` where ``result`` is the result
   of recursive application of rules 1-4 on `value`.

.. warning::
  The template language is currently under active development and additional
  features will be added in the future. Once feature complete the template
  language will be frozen to avoid breaking backwards compatibility for user
  interface implementors. See `JSON-E <https://github.com/taskcluster/json-e>`_
  for details.

The following **example** demonstrates how a task template can specify
timestamps and dump input JSON into environment variables::

  {
    "workerType": "my-worker",
    "payload": {
      "created": {"$fromNow": ""},
      "deadline": {"$fromNow": "1 hour 15 minutes"},
      "expiration": {"$fromNow": "14 days"},
      "image": "my-docker-image",
      "env": {
        "TASKID_TRIGGERED_FOR": "${taskId}",
        "INPUT_JSON": {"$json": {"$eval": "input"}}
      },
      ...
    },
    ...
  }


Formal Specification
--------------------

.. literalinclude:: actions-schema.yml
   :language: YAML
