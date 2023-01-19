.. _mozilla_projects_nss_reference_building_and_installing_nss_installation_guide:

Installation guide
==================

.. container::

   The build system of NSS originated from Netscape's build system, which predated the "configure;
   make; make test; make install" sequence that we're familiar with now. Our makefiles also have an
   "install" target, but it has a different meaning: our "install" means installing the headers,
   libraries, and programs in the appropriate directories under mozilla/dist.

   So right now you need to manually install the headers, libraries, and programs in the directories
   you want. If you install the libraries in a directory other than /usr/lib, you usually need to
   set the LD_LIBRARY_PATH environment variable. You can avoid that by installing the libraries in a
   directory that is $ORIGIN/../lib, where $ORIGIN is the directory where the programs are
   installed. This is done here:
   `http://lxr.mozilla.org/security/sour...platlibs.mk#53 <http://lxr.mozilla.org/security/source/security/nss/cmd/platlibs.mk#53>`__

   .. code::

      53 ifeq ($(OS_ARCH), Linux)
      54 ifeq ($(USE_64), 1)
      55 EXTRA_SHARED_LIBS += -Wl,-rpath,'$$ORIGIN/../lib64:$$ORIGIN/../lib'
      56 else
      57 EXTRA_SHARED_LIBS += -Wl,-rpath,'$$ORIGIN/../lib'
      58 endif
      59 endif

   For example, if you install certutil in /foo/bar/nss/bin and the .so's in /foo/bar/nss/lib, then
   you only need to add /foo/bar/nss/bin to your PATH; you don't need to set LD_LIBRARY_PATH.

   The libraries you need to install are listed below.

   NSPR:

   -  libnspr4.so
   -  libplds4.so
   -  libplc4.so

   NSS: (Note the use of \* for libfreebl -- some platforms have multiple ones)

   -  libfreebl*3.so
   -  libfreebl*3.chk
   -  libsoftokn3.so
   -  libsoftokn3.chk
   -  libnss3.so
   -  libsmime3.so
   -  libssl3.so
   -  libnssckbi.so