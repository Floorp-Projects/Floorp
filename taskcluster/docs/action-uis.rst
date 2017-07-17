User Interface Considerations
=============================

The actions system decouples in-tree changes from user interface changes by
taking advantage of graceful degradation. User interfaces, when presented with
an unfamiliar action, fall back to a usable default behavior, and can later be
upgraded to handle that action with a more refined approach.

Default Behavior
----------------

Every user interface should support the following:

 * Displaying a list of actions relevant to each task, and displaying
   task-group tasks for the associated task-group.

 * Providing an opportuntity for the user to enter input for an action.  This
   might be in JSON or YAML, or using a form auto-generated from the action's
   JSON-schema.  If the action has no schema, this step should be skipped.
   The user's input should be validated against the schema.

 * For ``action.kind = 'task'``, rendering the template using the JSON-e
   library, using the variables described in :doc:`action-spec`.

 * Calling ``Queue.createTask`` with the resulting task, using the user's
   Taskcluster credentials.  See the next section for some important
   security-related concerns.

Creating Tasks
--------------

When executing an action, a UI must ensure that the user is authorized to
perform the action, and that the user is not being "tricked" into executing
an unexpected action.

To accomplish the first, the UI should create tasks with the user's Taskcluster
credentials. Do not use credentials configured as part of the service itself!

To accomplish the second, use the decision tasks's ``scopes`` property as the
`authorizedScopes
<https://docs.taskcluster.net/manual/design/apis/hawk/authorized-scopes>`_ for
the ``Queue.createTask`` call.  This prevents action tasks from doing anything
the original decision task couldn't do.

Specialized Behavior
--------------------

The default behavior is too clumsy for day-to-day use for common actions.  User
interfaces may want to provide a more natural interface that still takes advantage
of the actions system.

Specialized Input
.................

A user interface may provide specialized input forms for specific schemas.  The
input generated from the form must conform to the schema.

To ensure that the schema has not changed, implementers should do a deep
comparison between a schema for which a hand-written form exists, and the
schema required by the action. If the two differ, the default behavior should
be used instead.

Specialized Triggering
......................

A user interface may want to trigger a specific action using a dedicated UI
element.  For example, an "start interactive session" button might be placed
next to each failing test in a list of tests.

User interfaces should look for the desired action by name. The UI should check
that there is exactly one matching action available for the given task or
task-graph. If multiple actions match, the UI should treat that as an error
(helping to avoid actions being surreptitiously replaced by similarly-named,
malicious actions).

Having discovered the task, the user interface has a choice in how to provide
its input. It can use the "specialized input" approach outlined above, providing
a customized form if the action's schema is recognized and gracefully degrading
if not.

But if the user interface is generating the input internally, it may instead
validate that generated input against the action's schema as given, proceeding
if validation succeeds.  In this alternative, there is no need to do a deep
comparison of the schema.  This approach allows in-tree changes that introduce
backward-compatible changes to the schema, without breaking support in user
interfaces.  Of course, if the changes are not backward-compatibile, breakage
will ensue.
