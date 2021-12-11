.. _mozilla_projects_nss_getting_started_with_nss:

Getting Started With NSS
========================

.. _how_to_get_involved_with_nss:

`How to get involved with NSS <#how_to_get_involved_with_nss>`__
----------------------------------------------------------------

.. container::

   | Network Security Services (NSS) is a base library for cryptographic algorithms and secure
     network protocols used by Mozilla software.
   | Would you like to get involved and help us to improve the core security of Mozilla Firefox and
     other applications that make use of NSS? We are looking forward to your contributions!
   | We have a large list of tasks waiting for attention, and we are happy to assist you in
     identifying areas that match your interest or skills. You can find us on `Mozilla
     IRC <https://developer.mozilla.org/en-US/docs/Mozilla/QA/Getting_Started_with_IRC>`__ in
     channel `#nss <irc://irc.mozilla.org/#nss>`__ or you could ask your questions on the
     `mozilla.dev.tech.crypto <https://lists.mozilla.org/listinfo/dev-tech-crypto/>`__ newsgroup.

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

   (Unfortunately the NSS project doesn't have a technical writer at this time, so our documentation
   is not as organized as we would like it to be. You could contribute by organizing it in a better
   way.)

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

.. _how_to_contribute:

`How to Contribute <#how_to_contribute>`__
------------------------------------------

.. container::

   ... (this section is still under construction, but there are many contribution opportunities)

   Start by opening a bugzilla account at `bugzilla.mozilla.org <https://bugzilla.mozilla.org/>`__
   if you don't have one.

   NSS :: Libraries component for issues you'd like to work on. We maintain a list of `NSS bugs
   marked with a keyword "good-first-bug" that you can
   view <https://bugzilla.mozilla.org/buglist.cgi?keywords=good-first-bug%2C%20&keywords_type=allwords&classification=Components&query_format=advanced&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&component=Libraries&product=NSS>`__.

.. _creating_your_patch:

`Creating your Patch <#creating_your_patch>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Seee our section on :ref:`mozilla_projects_nss_nss_sources_building_testing` to get started
   making your patch. When you're satisfied with it, you'll need code review.

.. _code_review:

`Code Review <#code_review>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   `http://phabricator.services.mozilla.com/ <https://phabricator.services.mozilla.com>`__ is our
   code review tool, which uses your Bugzilla account. Use our `Phabricator user instructions to
   upload patches for
   review <https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html>`__.

   Some items that will be evaluated during code review are `listed in checklist form on
   Github. <https://github.com/mozilla/nss-tools/blob/master/nss-code-review-checklist.yaml>`__

   After passing review, your patch can be landed by a member of the NSS team. You can find us on
   `Mozilla IRC <https://developer.mozilla.org/en-US/docs/Mozilla/QA/Getting_Started_with_IRC>`__ in
   channel `#nss <irc://irc.mozilla.org/#nss>`__.

   Note that we don't land code that isn't both reviewed and tested. Code only works when it has
   tests, and tests only work when they're part of the automation.