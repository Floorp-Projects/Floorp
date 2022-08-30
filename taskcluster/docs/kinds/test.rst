Test Kind
=========

The ``test`` kind defines both desktop and mobile tests for builds. Each YAML
file referenced in ``kind.yml`` defines the full set of tests for the
associated suite.

The process of generating tests goes like this, based on a set of YAML files
named in ``kind.yml``:

 * For each build task, determine the related test platforms based on the build
   platform. For example, a Windows 2010 build might be tested on Windows 7
   and Windows 10. Each test platform specifies "test sets" indicating which
   tests to run. This is configured in the file named
   ``test-platforms.yml``.

 * Each test set is expanded to a list of tests to run.  This is configured in
   the file named by ``test-sets.yml``. A platform may specify several test
   sets, in which case the union of those sets is used.

 * Each named test is looked up in the file named by ``tests.yml`` to find a
   test description. This test description indicates what the test does, how
   it is reported to treeherder, and how to perform the test, all in a
   platform-independent fashion.

 * Each test description is converted into one or more tasks. This is
   performed by a sequence of transforms defined in the ``transforms`` key in
   ``kind.yml``.  See :ref:`transforms` for more information.

 * The resulting tasks become a part of the task graph.

.. important::

    This process generates *all* test jobs, regardless of tree or try syntax.
    It is up to a later stages of the task-graph generation (the target set and
    optimization) to select the tests that will actually be performed.

Variants
--------

Sometimes we want to run the same tests under a different Firefox context,
usually this means with a pref set. The concept of ``variants`` was invented to
handle this use case. A variant is a stanza of configuration that can be merged
into each test definition. Variants are defined in the `variants.yml`_ file.
See this file for an up to date list of active variants and the pref(s) they
set.

Each variant must conform to the
:py:data:`~gecko_taskgraph.transforms.test.variant_description_schema`:

* **description** (required) - A description explaining what the variant is for.
* **component** (required) - The name of the component that owns the variant. It
  should be formatted as ``PRODUCT``::``COMPONENT``.
* **expiration** (required) - The date when the variant will be expired (maximum
  6 months).
* **suffix** (required) - A suffix to apply to the task label and treeherder symbol.
* **when** - A `json-e`_ expression that must evaluate to ``true`` for the variant
  to be applied. The ``task`` definition is passed in as context.
* **replace** - A dictionary that will overwrite keys in the task definition.
* **merge** - A dictionary that will be merged into the task definition using
  the :py:func:`~gecko_taskgraph.util.templates.merge` function.

.. note::

  Exceptions can be requested to have a variant without expiration (using
  "never") if this is a shipped mode we support. Teams should contact the CI
  team to discuss this before submitting a patch if they think their variant
  qualifies.  All exceptions will require director approval.


Defining Variants
~~~~~~~~~~~~~~~~~

Variants can be defined in the test YAML files using the ``variants`` key. E.g:

.. code-block:: yaml

   example-suite:
       variants:
           - foo
           - bar

This will split the task into three. The original task, the task with the
config from the variant named 'foo' merged in and the task with the config from
the variant named 'bar' merged in.


Composite Variants
~~~~~~~~~~~~~~~~~~

Sometimes we want to run tasks with multiple variants enabled at once. This can
be achieved with "composite variants". Composite variants are simply two or
more variant names joined with the ``+`` sign. Using the previous example, if
we wanted to run both the ``foo`` and ``bar`` variants together, we could do:

.. code-block:: yaml

   example-suite:
       variants:
           - foo+bar

This will first merge or replace the config of ``foo`` into the task, followed
by the config of ``bar``. Care should be taken if both variants are replacing
the same keys. The last variant's configuration will be the one that gets used.


Expired Variants
~~~~~~~~~~~~~~~~
Ideally, when a variant is not needed anymore, it should be dropped (even if it
has not expired). If you need to extend the expiration date, you can submit a
patch to modify the expiration date in the `variants.yml`_ file. Variants will
not be scheduled to run after the expiration date.

If an expired variant is not dropped, the triage owner of the component will be
notified. If the expired variant persists for an extended period, the autonag
bot will escalate to notify the manager and director of the triage owner. Once
at that point, we will submit a patch to remove the variant from Taskcluster and
manifest conditions pending the triage owner / manager to review.

Please subscribe to alerts from `firefox-ci <https://groups.google.com/a/mozilla.com/g/firefox-ci>`
group in order to be aware of changes to the CI, scheduling, or the policy.

.. _variants.yml: https://searchfox.org/mozilla-central/source/taskcluster/ci/test/variants.yml
.. _json-e: https://json-e.js.org/


Setting
-------

A test ``setting`` is the set of conditions under which a test is running.
Aside from the chunk number, a ``setting`` uniquely distinguishes a task from
another that is running the same set of tests. There are three types of inputs
that make up a ``setting``:

1. Platform - Bits of information that describe the underlying platform the
   test is running on. This includes things like the operating system and
   version, CPU architecture, etc.

2. Build - Bits of information that describe the build being tested. This
   includes things like the build type and which build attributes (like
   ``asan``, ``ccov``, etc) are enabled.

3. Runtime - Bits of information that describe the configured state of Firefox.
   This includes things like prefs and environment variables. Note that tasks
   should only set runtime configuration via the variants system (see
   `Variants`_).

Test ``settings`` are available in the ``task.extra.test-setting`` object in
all test tasks. They are defined by the
:py:func:`~gecko_taskgraph.transforms.test.set_test_setting` transform
function.

The full schema is defined in the
:py:data:`~gecko_taskgraph.transforms.test.test_setting_description_schema`.

Setting Hash
~~~~~~~~~~~~

In addition to the three top-level objects, there is also a ``_hash`` key which
contains a hash of the rest of the setting object. This is a convenient way for
consumers to group or compare tasks that run under the same setting.
