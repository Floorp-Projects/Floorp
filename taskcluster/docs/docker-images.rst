.. taskcluster_dockerimages:

=============
Docker Images
=============

TaskCluster Docker images are defined in the source directory under
``taskcluster/docker``. Each directory therein contains the name of an
image used as part of the task graph.

Adding Extra Files to Images
============================

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
   ADD topsrcdir/mach /home/worker/mach
