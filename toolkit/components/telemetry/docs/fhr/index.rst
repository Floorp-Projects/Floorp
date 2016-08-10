================================
Firefox Health Report (Obsolete)
================================

**Firefox Health Report (FHR) is obsolete and no longer ships with Firefox.
This documentation will live here for a few more cycles.**

Firefox Health Report is a background service that collects application
metrics and periodically submits them to a central server. The core
parts of the service are implemented in this directory. However, the
actual XPCOM service is implemented in the
``data_reporting_service`.

The core types can actually be instantiated multiple times and used to
power multiple data submission services within a single Gecko
application. In other words, everything in this directory is effectively
a reusable library. However, the terminology and some of the features
are very specific to what the Firefox Health Report feature requires.

.. toctree::
   :maxdepth: 1

   architecture
   dataformat
   identifiers

Legal and Privacy Concerns
==========================

Because Firefox Health Report collects and submits data to remote
servers and is an opt-out feature, there are legal and privacy
concerns over what data may be collected and submitted. **Additions or
changes to submitted data should be signed off by responsible
parties.**
