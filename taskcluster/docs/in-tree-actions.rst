Writing Treeherder Actions in-tree
==================================
This document shows how to define an action in-tree such that it shows up in
supported user interfaces like Treeherder. For details on interface between
in-tree logic and external user interfaces, see
:doc:`the specification for actions.json <action-spec>`.


Creating a Callback Action
--------------------------
A *callback action* is an action that calls back into in-tree logic. That is
you register the action with title, description, context, input schema and a
python callback. When the action is triggered in a user interface,
input matching the schema is collected, passed to a new task which then calls
your python callback, enabling it to do pretty much anything it wants to.

To create a new action you must create a file
``/taskcluster/actions/my-action.py``, that at minimum contains::

  from registry import register_callback_action

  @register_callback_action(
      title='Say Hello',
      symbol='hw',  # Show the callback task in treeherder as 'hw'
      description="Simple **proof-of-concept** callback action",
      order=10000,  # Order in which it should appear relative to other actions
  )
  def hello_world_action(parameters, input, task_group_id, task_id, task):
      # parameters is an instance of taskgraph.parameters.Parameters
      # it carries decision task parameters from the original decision task.
      # input, task_id, and task should all be None
      print "Hello was triggered from taskGroupId: " + taskGroupId

The example above defines an action that is available in the context-menu for
the entire task-group (result-set or push in Treeherder terminology). To create
an action that shows up in the context menu for a task we must specify the
``context`` parameter.


Setting the Action Context
--------------------------
The context parameter should be a list of tag-sets, such as
``context=[{"platform": "linux"}]``, which will make the task show up in the
context-menu for any task with ``task.tags.platform = 'linux'``. Below is
some examples of context parameters and the resulting conditions on
``task.tags`` (tags used below are just illustrative).

``context=[{"platform": "linux"}]``:
  Requires ``task.tags.platform = 'linux'``.
``context=[{"kind": "test", "platform": "linux"}]``:
  Requires ``task.tags.platform = 'linux'`` **and** ``task.tags.kind = 'test'``.
``context=[{"kind": "test"}, {"platform": "linux"}]``:
  Requires ``task.tags.platform = 'linux'`` **or** ``task.tags.kind = 'test'``.
``context=[{}]``:
  Requires nothing and the action will show up in the context menu for all tasks.
``context=[]``:
  Is the same as not setting the context parameter, which will make the action
  show up in the context menu for the task-group.
  (ie. the action is not specific to some task)

The example action below will be shown in the context-menu for tasks with
``task.tags.platform = 'linux'``::

  from registry import register_callback_action

  @register_callback_action(
      title='Retrigger',
      symbol='re-c',  # Show the callback task in treeherder as 're-c'
      description="Create a clone of the task",
      order=1,
      context=[{'platform': 'linux'}]
  )
  def retrigger_action(parameters, input, task_group_id, task_id, task):
      # input will be None
      print "Retriggering: {}".format(task_id)
      print "task definition: {}".format(task)

When the ``context`` parameter is set, the ``task_id`` and ``task`` parameters
will provided to the callback. In this case the ``task_id`` and ``task``
parameters will be the ``taskId`` and *task definition* of the task from whose
context-menu the action was triggered.

Typically, the ``context`` parameter is used for actions that operates on
tasks, such as retriggering, running a specific test case, creating a loaner,
bisection, etc. You can think of the context as a place the action should
appear, but it's also very much a form of input the action can use.


Specifying an Input Schema
--------------------------
In call examples so far the ``input`` parameter for the callback have been
``None``, to make an action that takes input you must specify an input schema.
This is done by passing a JSON schema as the ``schema`` parameter.

When designing a schema for the input it is important to exploit as many of the
JSON schema validation features as reasonably possible. Furthermore, it is
*strongly* encouraged that the ``title`` and ``description`` properties in
JSON schemas is used to provide a detailed explanation of what the input
value will do. Authors can reasonably expect JSON schema ``description``
properties to be rendered as markdown before being presented.

The example below illustrates how to specify an input schema. Notice that while
this example doesn't specify a ``context`` it is perfectly legal to specify
both ``input`` and ``context``::

  from registry import register_callback_action

  @register_callback_action(
      title='Run All Tasks',
      symbol='ra-c',  # Show the callback task in treeherder as 'ra-c'
      description="**Run all tasks** that have been _optimized_ away.",
      order=1,
      input={
          'title': 'Action Options',
          'description': 'Options for how you wish to run all tasks',
          'properties': {
              'priority': {
                  'title': 'priority'
                  'description': 'Priority that should be given to the tasks',
                  'type': 'string',
                  'enum': ['low', 'normal', 'high'],
                  'default': 'low',
              },
              'runTalos': {
                  'title': 'Run Talos'
                  'description': 'Do you wish to also include talos tasks?',
                  'type': 'boolean',
                  'default': 'false',
              }
          },
          'required': ['priority', 'runTalos'],
          'additionalProperties': False,
      },
  )
  def retrigger_action(parameters, input, task_group_id, task_id, task):
      print "Create all pruned tasks with priority: {}".format(input['priority'])
      if input['runTalos']:
          print "Also running talos jobs..."

When the ``schema`` parameter is given the callback will always be called with
an ``input`` parameter that satisfies the previously given JSON schema.
It is encouraged to set ``additionalProperties: false``, as well as specifying
all properties as ``required`` in the JSON schema. Furthermore, it's good
practice to provide ``default`` values for properties, user interface generators
will often take advantage of such properties.

Once you have specified input and context as applicable for your action you can
do pretty much anything you want from within your callback. Whether you want
to create one or more tasks or run a specific piece of code like a test.


Conditional Availability
------------------------
The decision parameters ``taskgraph.parameters.Parameters`` passed to
the callback is also available when the decision task generates the list of
actions to be displayed in the user interface. When registering an action
callback the ``availability`` parameter can be used to specify a lambda function
that given the decision parameters determines if the action should be available.
The feature is illustrated below::

  from registry import register_callback_action

  @register_callback_action(
      title='Say Hello',
      symbol='hw',  # Show the callback task in treeherder as 'hw'
      description="Simple **proof-of-concept** callback action",
      order=2,
      # Define an action that is only included if this is a push to try
      available=lambda parameters: parameters.get('project', None) == 'try',
  )
  def try_only_action(parameters, input, task_group_id, task_id, task):
      print "My try-only action"

Properties of ``parameters``  is documented in the
:doc:`parameters section <parameters>`. You can also checkout the
``parameters.yml`` artifact created by decisions tasks.


Skipping the Action Callback
----------------------------
It is possible to define an action that doesn't take a callback, instead you'll
then have to provide a task template. For details on how the task template
language works refer to :doc:`the specification for actions.json <action-spec>`,
the example below illustrates how to create such an action::

  from registry import register_task_action

  @register_task_action(
      title='Retrigger',
      description="Create a clone of the task",
      order=1,
      context=[{'platform': 'linux'}],
      input={
          'title': 'priority'
          'description': 'Priority that should be given to the tasks',
          'type': 'string',
          'enum': ['low', 'normal', 'high'],
          'default': 'low',
      },
  def task_template_builder(parameters):
      # The task template builder may return None to signal that the action
      # isn't available.
      if parameters.get('project', None) != 'try':
        return None
      return {
          'created': {'$fromNow': ''},
          'deadline': {'$fromNow': '1 hour'},
          'expires': {'$fromNow': '14 days'},
          'provisionerId': '...',
          'workerType': '...',
          'priority': '${input}',
          'payload': {
              'command': '...',
              'env': {
                  'TASK_DEFINITION': {'$json': {'eval': 'task'}}
              },
              ...
          },
          # It's now your responsibility to include treeherder routes, as well
          # additional metadata for treeherder in task.extra.treeherder.
          ...
      },
  )

This kind of actions is rarely useful due to the limited expressiveness of the
template language. For further details on the template see
:doc:`the specification for actions.json <action-spec>`. Obviously, this kind of
action can be slightly more responsive because it doesn't create an intermediate
task in-order to trigger a python callback inside the tree.
