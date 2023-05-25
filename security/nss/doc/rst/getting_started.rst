.. _mozilla_projects_nss_getting_started:

.. warning::
   This NSS documentation was just imported from our legacy MDN repository. It currently is very deprecated and likely incorrect or broken in many places.

Getting Started
===============

.. _how_to_get_involved_with_nss:

`How to get involved with NSS <#how_to_get_involved_with_nss>`__
----------------------------------------------------------------

.. container::

   | Network Security Services (NSS) is a base library for cryptographic algorithms and secure
     network protocols used by Mozilla software.
   | Would you like to get involved and help us to improve the core security of Mozilla Firefox and
     other applications that make use of NSS? We are looking forward to your contributions!

   .. note::

      We have a large list of tasks waiting for attention, and we are happy to assist you in
      identifying areas that match your interest or skills. You can find us on `chat.mozilla.org`
      in channel `#nss <https://chat.mozilla.org/#/room/#nss:mozilla.org>`__ or you could ask your 
      questions on the `mozilla.dev.tech.crypto <https://groups.google.com/a/mozilla.org/g/dev-tech-crypto>`__ newsgroup.


   The NSS library and its supporting command line tools are written in the C programming language.
   Its build system and the automated tests are based on makefiles and bash scripts.

   Over time, many documents have been produced that describe various aspects of NSS. You can start
   with:

   -  the current `primary NSS documentation page <https://developer.mozilla.org/en-US/docs/NSS>`__
      from which we link to other documentation.
   -  a `General Overview <https://developer.mozilla.org/en-US/docs/Overview_of_NSS>`__ of the
      applications that use NSS and the features it provides.
   -  a high level :ref:`mozilla_projects_nss_an_overview_of_nss_internals`.
   -  learn about getting the :ref:`mozilla_projects_nss_nss_sources_building_testing`
   -  `Old documentation <https://www-archive.mozilla.org/projects/security/pki/nss/>`__ that is on
      the archived mozilla.org website.

   .. _nss_sample_code:

   `NSS Sample Code <#nss_sample_code>`__
   --------------------------------------

   .. container::

      A good place to start learning how to write NSS applications are the command line tools that are
      maintained by the NSS developers. You can find them in subdirectory mozilla/security/nss/cmd

      Or have a look at some basic :ref:`mozilla_projects_nss_nss_sample_code`.

      A new set of samples is currently under development and review, see `Create new NSS
      samples <https://bugzilla.mozilla.org/show_bug.cgi?id=490238>`__.

      You are welcome to download the samples via: hg clone https://hg.mozilla.org/projects/nss; cd
      nss; hg update SAMPLES_BRANCH
