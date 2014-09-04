.. _cloudsync:

=====================
CloudSync
=====================

CloudSync is a service that provides access to tabs and bookmarks data
for third-party sync addons. Addons can read local bookmarks and tabs.
Bookmarks and tab data can be merged from remote devices.

Addons are responsible for maintaining an upstream representation, as
well as sending and receiving data over the network.

.. toctree::
   :maxdepth: 1

   architecture
   dataformat
   example
