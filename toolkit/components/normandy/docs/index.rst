.. _components/normandy:

====================
Shield Recipe Client
====================

Normandy (aka the Shield Recipe Client) is a targeted change control
system, allowing small changes to be made within a release of Firefox,
such as studies.

It downloads recipes and actions from :ref:`Remote Settings <services/remotesettings>`
and then executes them.

.. note::

   Previously, the recipes were fetched from the `recipe server`_, but in `Bug 1513854`_
   the source was changed to *Remote Settings*. The cryptographic signatures are verified
   at the *Remote Settings* level (integrity) and at the *Normandy* level
   (authenticity of publisher).

.. _recipe server: https://github.com/mozilla/normandy/
.. _Bug 1513854: https://bugzilla.mozilla.org/show_bug.cgi?id=1513854

.. toctree::
   :maxdepth: 1

   data-collection
   execution-model
   suitabilities
   services
