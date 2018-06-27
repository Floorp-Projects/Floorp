Loading Tasks
=============

The full task graph generation involves creating tasks for each kind.  Kinds
are ordered to satisfy ``kind-dependencies``, and then the ``loader`` specified
in ``kind.yml`` is used to load the tasks for that kind. It should point to
a Python function like::

    def loader(cls, kind, path, config, parameters, loaded_tasks):
        pass

The ``kind`` is the name of the kind; the configuration for that kind
named this class.

The ``path`` is the path to the configuration directory for the kind. This
can be used to load extra data, templates, etc.

The ``parameters`` give details on which to base the task generation. See
:doc:`parameters` for details.

At the time this method is called, all kinds on which this kind depends
(that is, specified in the ``kind-dependencies`` key in ``config``)
have already loaded their tasks, and those tasks are available in
the list ``loaded_tasks``.

The return value is a list of inputs to the transforms listed in the kind's
``transforms`` property. The specific format for the input depends on the first
transform - whatever it expects. The final transform should be
``taskgraph.transform.task:transforms``, which produces the output format the
task-graph generation infrastructure expects.

The ``transforms`` key in ``kind.yml`` is further documented in
:doc:`transforms`.  For more information on how all of this works, consult the
docstrings and comments in the source code itself.
