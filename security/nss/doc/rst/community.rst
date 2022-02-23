.. _mozilla_projects_nss_community:

Community
---------

Network Security Services (NSS) is maintained by a group of engineers and researchers,
mainly RedHat and Mozilla.

.. warning::

   While the NSS team focuses mainly on supporting platforms and features needded by
   Firefox and RHEL, we are happy to take contributions.

Contributors can reach out the the core team and follow NSS related news through the
following mailing list, Google group and Element/Matrix channel:

.. note::

   Mailing list: `https://groups.google.com/a/mozilla.org/g/dev-tech-crypto <https://groups.google.com/a/mozilla.org/g/dev-tech-crypto>`__

   Matrix/Element: `https://app.element.io/#/room/#nss:mozilla.org <https://app.element.io/#/room/#nss:mozilla.org>`__

..
   -  View Mozilla Security forums...

   -  `Mailing list <https://lists.mozilla.org/listinfo/dev-security>`__
   -  `Newsgroup <http://groups.google.com/group/mozilla.dev.security>`__
   -  `RSS feed <http://groups.google.com/group/mozilla.dev.security/feeds>`__

.. _how_to_contribute:

`How to Contribute <#how_to_contribute>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Start by opening a **Bugzilla** account at `bugzilla.mozilla.org <https://bugzilla.mozilla.org/>`__ if you don't have one.

   ``NSS :: Libraries`` is the component for issues you'd like to work on.
   We maintain a list of `NSS bugs marked with a keyword "good-first-bug" <https://bugzilla.mozilla.org/buglist.cgi?keywords=good-first-bug%2C%20&keywords_type=allwords&classification=Components&query_format=advanced&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&component=Libraries&product=NSS>`__.

.. _creating_your_patch:

`Creating your Patch <#creating_your_patch>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   See our section on :ref:`mozilla_projects_nss_nss_sources_building_testing` to get started
   making your patch. When you're satisfied with it, you'll need code review.

.. _code_review:

`Code Review <#code_review>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   `http://phabricator.services.mozilla.com/ <https://phabricator.services.mozilla.com>`__ is our
   code review tool, which uses your Bugzilla account.

   Use our `Phabricator user instructions <https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html>`__ to upload patches for review.
   Some items that will be evaluated during code review are `listed in checklist form on
   Github. <https://github.com/mozilla/nss-tools/blob/master/nss-code-review-checklist.yaml>`__

   After passing review, your patch can be landed by a member of the NSS team. Note that we don't land code that isn't both reviewed and tested.

.. warning::

   Please reach out to the team before engaging in a lot of work to make ensure we are willing to accept your contributions.
