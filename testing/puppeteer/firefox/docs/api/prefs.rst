.. py:currentmodule:: firefox_puppeteer.api.prefs

Preferences
===========

The Preferences class is a wrapper around the nsIPrefBranch_ interface in
Firefox and allows you to interact with the preferences system. It only
includes the most commonly used methods of that interface, whereby it also
enhances the logic in terms of e.g. restoring the original value of modified
preferences.

.. _nsIPrefBranch: https://developer.mozilla.org/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIPrefBranch

Preferences
-----------

.. autoclass:: Preferences
   :members:
