Test Transforms
===============

Test descriptions specify how to run a unittest or talos run.  They aim to
describe this abstractly, although in many cases the unique nature of
invocation on different platforms leaves a lot of specific behavior in the test
description, divided by ``by-test-platform``.

Test descriptions are validated to conform to the schema in
``taskcluster/gecko_taskgraph/transforms/test/__init__.py``.  This schema is
extensively documented and is a the primary reference for anyone modifying
tests.

The output of ``tests.py`` is a task description.  Test dependencies are
produced in the form of a dictionary mapping dependency name to task label.
