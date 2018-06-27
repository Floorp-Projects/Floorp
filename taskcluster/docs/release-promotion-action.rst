Release Promotion Action
========================

The release promotion action is how Releng triggers `release promotion`_
taskgraphs. The one action covers all release promotion needs: different
*flavors* allow for us to trigger the different :ref:`release promotion phases`
for each product. The input schema and release promotion flavors are defined in
the `release promotion action`_.

.. _snowman model:

The snowman model
-----------------

The `release promotion action`_ allows us to chain multiple taskgraphs (aka graphs, aka task groups) together.
Essentially, we're using `optimization`_ logic to replace task labels in the
current taskgraph with task IDs from the previous taskgraph(s).

This is the ``snowman`` model. If you request the body of
the snowman and point at the base, we only create the middle section of the snowman.
If you request the body of the snowman and don't point it at the base, we build the
first base and body of the snowman from scratch.

For example, let's generate a task ``t2`` that depends on ``t1``. Let's call our new taskgraph ``G``::

    G
    |
    t1
    |
    t2

Task ``t2`` will wait on task ``t1`` to finish, and downloads some artifacts from task ``t1``.

Now let's specify task group ``G1`` and ``G2`` as previous task group IDs. If task ``t1`` is in one of them, ``t2`` will depend on that task, rather than spawning a new ``t1`` in task group ``G``::

    G1        G2        G
    |         |         |
    t1        t1        |
                \______ |
                       \|
                        t2
    
    or
    
    G1        G2        G
    |         |         |
    t1        t0        |
      \________________ |
                       \|
                        t2

For a more real-world example::

         G
         |
       build
         |
      signing
         |
    l10n-repack
         |
    l10n-signing

If we point the ``promote`` task group G at the on-push build task group ``G1``, the l10n-repack job will depend on the previously finished build and build-signing tasks::

         G1            G
         |             |
       build           |
         |             |
      signing          |
             \_________|
                       |
                  l10n-repack
                       |
                  l10n-signing

We can also explicitly exclude certain tasks from being optimized out.
We currently do this by specifying ``rebuild_kinds`` in the action; these
are `kinds`_ that we want to explicitly rebuild in the current task group,
even if they existed in previous task groups. We also allow for specifying a list of
``do_not_optimize`` labels, which would be more verbose and specific than
specifying kinds to rebuild.

Release promotion action mechanics
----------------------------------

There are a number of inputs defined in the `release promotion action`_. Among these are the ``previous_graph_ids``, which is an ordered list of taskGroupIds of the task groups that we want to build our task group, off of. In the :ref:`snowman model`, these define the already-built portions of the snowman.

The action downloads the ``parameters.yml`` from the initial ``previous_graph_id``, which matches the decision- or action- taskId. (See :ref:`taskid vs taskgroupid`.) This is most likely the decision task of the revision to promote, which is generally the same revision the release promotion action is run against.

.. note:: If the parameters have been changed since the build happened, *and* we explicitly want the new parameters for the release promotion action task, the first ``previous_graph_id`` should be the new revision's decision task. Then the build and other previous action task group IDs can follow, so we're still replacing the task labels with the task IDs from the original revision.

The action then downloads the various ``label-to-taskid.json`` artifacts from each previous task group, and builds an ``existing_tasks`` parameter of which labels to replace with which task IDs. Each successive update to this dictionary overwrites existing keys with new task IDs, so the rightmost task group with a given label takes precedence. Any labels that match the ``do_not_optimize`` list or that belong to tasks in the ``rebuild_kinds`` list are excluded from the ``existing_tasks`` parameter.

Once all that happens, and we've gotten our configuration from the original parameters and our action config and inputs, we run the decision task function with our custom parameters. The `optimization`_ phase replaces any ``existing_tasks`` with the task IDs we've built from the previous task groups.

Release Promotion Flavors
-------------------------

For the most part, release promotion flavors match the pattern ``phase_product``,
e.g. ``promote_fennec``, ``push_devedition``, or ``ship_firefox``.

We've added ``_rc`` suffix flavors, to deal with special RC behavior around rolling out updates using a different rate or channel.

We are planning on adding ``_partners`` suffix flavors, to allow for creating partner repacks off-cycle.

The various flavors are defined in the `release promotion action`_.

Triggering the release promotion action via Treeherder
------------------------------------------------------

Currently, we're able to trigger this action via `Treeherder`_; we sometimes use this method for testing purposes. This is powerful, because we can modify the inputs directly, but is less production friendly, because it requires us to enter the inputs manually. At some point we may disable the ability to trigger the action via Treeherder.

This requires being signed in with the right scopes. On `Release Promotion Projects`_, there's a dropdown in the top right of a given revision. Choose ``Custom Push Action``, then ``Release Promotion``. The inputs are specifiable as raw yaml on the left hand column.

Triggering the release promotion action via releaserunner3
----------------------------------------------------------

`Releaserunner3`_ is our current method of triggering the release promotion action from Ship It in production. Examples of how to run this are in the `releasewarrior docs`_.

To deal with the above ``previous_graph_ids`` logic, we allow for a ``decision_task_id`` in `trigger_action.py`_. As of 2018-03-14, this script assumes we want to download ``parameters.yml`` from the same decision task that we get ``actions.json`` from. At some point, we'd like the `trigger_action.py`_ call to happen automatically once we push a button on Ship It.

The action task that's generated from ``actions.json`` matches the `.taskcluster.yml`_ template. This is important; Chain of Trust (v2) requires that the task definition be reproducible from `.taskcluster.yml`_.

.. _taskid vs taskgroupid:

Release promotion action taskId and taskGroupId
-----------------------------------------------

The ``taskGroupId`` of a release promotion action task will be the same as the ``taskId`` of the decision task.

The ``taskGroupId`` of a release promotion *task group* will be the same as the ``taskId`` of the release promotion action task.

So:

* for a given push, the decision taskId ``D`` will create the taskGroupId ``D``
* we create a release promotion action task with the taskId ``A``. The ``A`` task will be part of the ``D`` task group, but will spawn a task group with the taskGroupId ``A``.

Another way of looking at it:

* If you're looking at a task ``t1`` in the action taskGroup, ``t1``'s taskGroupId is the action task's taskId. (In the above example, this would be ``A``.)
* Then if you look at the action task's taskGroupId, that's the original decision task's taskId. (In the above example, this would be ``D``.)

Testing and developing the release promotion action
---------------------------------------------------

To test the release promotion, action, we can use ``./mach taskgraph test-action-callback`` to debug.

The full command for a ``promote_fennec`` test might look like::

    ./mach taskgraph test-action-callback \
        --task-group-id LR-xH1ViTTi2jrI-N1Mf2A \
        --input /src/gecko/params/promote_fennec.yml \
        -p /src/gecko/params/maple-promote-fennec.yml \
        release_promotion_action > ../promote.json

The input file (in the above example, that would be ``/src/gecko/params/promote_fennec.yml``), contains the action inputs. The input schema is defined in the `release promotion action`_. Previous example inputs are embedded in previous promotion task group action task definitions (``task.extra.action.input``).

The ``parameters.yml`` file is downloadable from a previous decision or action task.

.. _release promotion: release-promotion.html
.. _optimization: optimization.html
.. _kinds: kinds.html
.. _release promotion action: https://searchfox.org/mozilla-central/source/taskcluster/taskgraph/actions/release_promotion.py
.. _Treeherder: https://treeherder.mozilla.org
.. _Release Promotion Projects: https://searchfox.org/mozilla-central/search?q=RELEASE_PROMOTION_PROJECTS&path=taskcluster/taskgraph/util/attributes.py
.. _Releaserunner3: https://hg.mozilla.org/build/tools/file/tip/buildfarm/release
.. _releasewarrior docs: https://github.com/mozilla-releng/releasewarrior-2.0/blob/master/docs/release-promotion/desktop/howto.md#how
.. _trigger_action.py: https://dxr.mozilla.org/build-central/source/tools/buildfarm/release/trigger_action.py#118
.. _.taskcluster.yml: https://searchfox.org/mozilla-central/source/.taskcluster.yml
