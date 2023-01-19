.. _mozilla_projects_nss_reference_building_and_installing_nss_migration_to_hg:

Migration to HG
===============

.. container::

   | The NSPR, NSS and related projects have stopped using Mozilla'a CVS server, but have migrated
     to
   | Mozilla's HG (Mercurial) server.
   | Each project now lives in its own separate space, they can be found at:
   |    https://hg.mozilla.org/projects/nspr/
   |    https://hg.mozilla.org/projects/nss/
   |    https://hg.mozilla.org/projects/jss/
   |   https://hg.mozilla.org/projects/python-nss/

   | This migration has been used as an opportunity to change the layout of the
   | source directories.
   | For NSPR, "mozilla/nsprpub" has been removed from the directory
   | hierarchy, all files now live in the top directory of the NSPR
   | repository.
   | Likewise for NSS and JSS, "mozilla/security" has been removed and files
   | now live at the top level. In addition for NSS, we have merged the
   | contents of directories mozilla/dbm and mozilla/security/dbm into the
   | new directory lib/dbm.
   | Besides the new layout, the build system hasn't changed. Most parts of
   | the NSS build instructions remain valid, especially the instructions
   | about setting environment variables.
   | Updated instructions for building NSS with NSPR can be found at:
   | :ref:`mozilla_projects_nss_reference_building_and_installing_nss_build_instructions`
   | It's best to refer to the above document to learn about the various
   | environment variables that you might have to set to build on your
   | platform (this part hasn't changed).
   | However, below is a brief summary that shows how to checkout the
   | source code and build both NSPR and NSS:
   |   mkdir workarea
   |   cd workarea
   |   hg clone https://hg.mozilla.org/projects/nspr
   |   hg clone https://hg.mozilla.org/projects/nss
   |   cd nss
   |   # set USE_64=1 on 64 bit architectures
   |   # set BUILD_OPT=1 to get an optimized build
   |   make nss_build_all
   | Note that the JSS project has been given a private copy of the former
   | mozilla/security/coreconf directory, allowing it to remain stable,
   | and only update its build system as necessary.
   | Because of the changes described above, we have decided to use a new
   | series of (minor) version numbers. The first releases using the new code
   | layout will be NSPR 4.10 and NSS 3.15