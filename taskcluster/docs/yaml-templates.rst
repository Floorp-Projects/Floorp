Task Definition YAML Templates
==============================

A few kinds of tasks are described using templated YAML files.  These files
allow some limited forms of inheritance and template substitution as well as
the usual YAML features, as described below.

Please do not use these features in new kinds.  If you are tempted to use
variable substitution over a YAML file to define tasks, please instead
implement a new kind-specific transform to accopmlish your goal.  For example,
if the current push-id must be included as an argument in
``task.payload.command``, write a transform function that makes that assignment
while building a job description, rather than parameterizing that value in the
input to the transforms.

Inheritance
-----------

One YAML file can "inherit" from another by including a top-level ``$inherits``
key.  That key specifies the parent file in ``from``, and optionally a
collection of variables in ``variables``.  For example:

.. code-block:: yaml

    $inherits:
      from: 'tasks/builds/base_linux32.yml'
      variables:
        build_name: 'linux32'
        build_type: 'dbg'

Inheritance proceeds as follows: First, the child document has its template
substitutions performed and is parsed as YAML.  Then, the parent document is
parsed, with substitutions specified by ``variables`` added to the template
substitutions.  Finally, the child document is merged with the parent.

To merge two JSON objects (dictionaries), each value is merged individually.
Lists are merged by concatenating the lists from the parent and child
documents.  Atomic values (strings, numbers, etc.) are merged by preferring the
child document's value.

Substitution
------------

Each document is expanded using the PyStache template engine before it is
parsed as YAML.  The parameters for this expansion are specific to the task
kind.

Simple value substitution looks like ``{{variable}}``.  Function calls look
like ``{{#function}}argument{{/function}}``.
