.. _mozilla_projects_nss_reference_building_and_installing_nss_sample_manual_installation:

Sample manual installation
==========================

.. container::

   |
   | The NSS build system does not include a target to install header files and shared libraries in
     the system directories, so this needs to be done manually.

   After building NSS with *"gmake nss_build_all"*, the resulting build can be found in the NSS
   source tree as follows:

   -  NSS header files: *mozilla/dist/public/nss*
   -  NSPR header files: *mozilla/dist/*\ **<OBJ-DIR>**\ */include*
   -  NSPR/NSS shared libs: *mozilla/dist/*\ **<OBJ-DIR>**\ */lib*
   -  NSS binary executables: *mozilla/dist/*\ **<OBJ-DIR>**\ */bin*.

   where **<OBJ-DIR>** would vary according to the type of build and the platform. For example,
   **<OBJ-DIR>** for a debug build of NSS on the x86 platform with a Linux kernel version 2.6 with
   glibc would be: Linux2.6_x86_glibc_PTH_DBG.OBJ

   From these directories, you can copy the files to any system (or other) directory. If the
   destination directories are not what's standard for the system (e.g. /usr/include, /usr/lib and
   /usr/bin for a Linux system), you need to edit the corresponding environment variables or
   compiler/linker arguments.