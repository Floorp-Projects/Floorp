.. taskcluster_dockerimages:

=============
Docker Images
=============

TaskCluster Docker images are defined in the source directory under
``taskcluster/docker``. Each directory therein contains the name of an
image used as part of the task graph.

Organization
------------

Each folder describes a single docker image.  We have two types of images that can be defined:

1. Task Images (build-on-push)
2. Docker Images (prebuilt)

These images depend on one another, as described in the `FROM
<https://docs.docker.com/v1.8/reference/builder/#from>`_ line at the top of the
Dockerfile in each folder.

Images could either be an image intended for pushing to a docker registry, or
one that is meant either for local testing or being built as an artifact when
pushed to vcs.

Task Images (build-on-push)
:::::::::::::::::::::::::::

Images can be uploaded as a task artifact, :ref:`indexed <task-image-index-namespace>` under
a given namespace, and used in other tasks by referencing the task ID.

Important to note, these images do not require building and pushing to a docker registry, and are
built per push (if necessary) and uploaded as task artifacts.

The decision task that is run per push will :ref:`determine <context-directory-hashing>`
if the image needs to be built based on the hash of the context directory and if the image
exists under the namespace for a given branch.

As an additional convenience, and a precaution to loading images per branch, if an image
has been indexed with a given context hash for mozilla-central, any tasks requiring that image
will use that indexed task.  This is to ensure there are not multiple images built/used
that were built from the same context. In summary, if the image has been built for mozilla-central,
pushes to any branch will use that already built image.

To use within an in-tree task definition, the format is:

.. code-block:: yaml

    image:
        type: 'task-image'
        path: 'public/image.tar.zst'
        taskId: '<task_id_for_image_builder>'

.. _context-directory-hashing:

Context Directory Hashing
.........................

Decision tasks will calculate the sha256 hash of the contents of the image
directory and will determine if the image already exists for a given branch and hash
or if a new image must be built and indexed.

Note: this is the contents of *only* the context directory, not the
image contents.

The decision task will:

1. Recursively collect the paths of all files within the context directory
2. Sort the filenames alphabetically to ensure the hash is consistently calculated
3. Generate a sha256 hash of the contents of each file
4. All file hashes will then be combined with their path and used to update the
   hash of the context directory

This ensures that the hash is consistently calculated and path changes will result
in different hashes being generated.

.. _task-image-index-namespace:

Task Image Index Namespace
..........................

Images that are built on push and uploaded as an artifact of a task will be indexed under the
following namespaces.

* gecko.cache.level-{level}.docker.v2.{name}.hash.{digest}
* gecko.cache.level-{level}.docker.v2.{name}.latest
* gecko.cache.level-{level}.docker.v2.{name}.pushdate.{year}.{month}-{day}-{pushtime}

Not only can images be browsed by the pushdate and context hash, but the 'latest' namespace
is meant to view the latest built image.  This functions similarly to the 'latest' tag
for docker images that are pushed to a registry.

Docker Registry Images (prebuilt)
:::::::::::::::::::::::::::::::::

***Warning: Use of prebuilt images should only be used for base images (those that other images
will inherit from), or private images that must be stored in a private docker registry account.***

These are images that are intended to be pushed to a docker registry and used
by specifying the docker image name in task definitions.  They are generally
referred to by a ``<repo>@<repodigest>`` string:

Example:

.. code-block:: none

    image: taskcluster/decision:0.1.10@sha256:c5451ee6c655b3d97d4baa3b0e29a5115f23e0991d4f7f36d2a8f793076d6854

Each image has a repo digest and a version. The repo digest is stored in the
``HASH`` file in the image directory and used to refer to the image as above.
The version is in ``VERSION``.

The version file only serves to provide convenient names, such that old
versions are easy to discover in the registry (and ensuring old versions aren't
deleted by garbage-collection).

Each image directory also has a ``REGISTRY``, defaulting to the ``REGISTRY`` in
the ``taskcluster/docker`` directory, and specifying the image registry to
which the completed image should be uploaded.

Docker Hashes and Digests
.........................

There are several hashes involved in this process:

 * Image Hash -- the long version of the image ID; can be seen with
   ``docker images --no-trunc`` or in the ``Id`` field in ``docker inspect``.

 * Repo Digest -- hash of the image manifest; seen when running ``docker
   push`` or ``docker pull``.

 * Context Directory Hash -- see above (not a Docker concept at all)

The use of hashes allows older tasks which were designed to run on an older
version of the image to be executed in Taskcluster while new tasks use the new
version.  Furthermore, this mitigates attacks against the registry as docker
will verify the image hash when loading the image.

(Re)-Building images
--------------------

Generally, images can be pulled from the Docker registry rather than built
locally, however, for developing new images it's often helpful to hack on them
locally.

To build an image, invoke ``mach taskcluster-build-image`` with the name of the
folder (without a trailing slash):

.. code-block:: none

    ./mach taskcluster-build-image <image-name>

This is a wrapper around ``docker build -t $REGISTRY/$FOLDER:$VERSION``.

It's a good idea to bump the ``VERSION`` early in this process, to avoid
``docker push``-ing  over any old tags.

For task images, test your image locally or push to try. This is all that is
required.

Docker Registry Images
::::::::::::::::::::::

Landing docker registry images takes a little more care.

Once a new version of the image has been built and tested locally, push it to
the docker registry and make note of the resulting repo digest.  Put this value
in the ``HASH`` file, and update any references to the image in the code or
task definitions.

The change is now safe to use in Try pushes.

Special Dockerfile Syntax
-------------------------

Dockerfile syntax has been extended to allow *any* file from the
source checkout to be added to the image build *context*. (Traditionally
you can only ``ADD`` files from the same directory as the Dockerfile.)

Simply add the following syntax as a comment in a Dockerfile::

   # %include <path>

e.g.

   # %include mach
   # %include testing/mozharness

The argument to ``# %include`` is a relative path from the root level of
the source directory. It can be a file or a directory. If a file, only that
file will be added. If a directory, every file under that directory will be
added (even files that are untracked or ignored by version control).

Files added using ``# %include`` syntax are available inside the build
context under the ``topsrcdir/`` path.

Files are added as they exist on disk. e.g. executable flags should be
preserved. However, the file owner/group is changed to ``root`` and the
``mtime`` of the file is normalized.

Here is an example Dockerfile snippet::

   # %include mach
   ADD topsrcdir/mach /builds/worker/mach
