Signing Transforms
==================

Signing kinds are passed a single dependent job (from its kind dependency) to act
on.

The transforms in ``taskcluster/gecko_taskgraph/transforms/signing.py`` implement
this common functionality.  They expect a "signing description", and produce a
task definition.  The schema for a signing description is defined at the top of
``signing.py``, with copious comments.

In particular you define a set of upstream artifact urls (that point at the
dependent task) and can optionally provide a dependent name (defaults to build)
for use in ``task-reference``/``artifact-reference``. You also need to provide
the signing formats to use.
