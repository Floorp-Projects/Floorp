======================
Data Reporting Service
======================

This directory contains files related to data collection and reporting
within Gecko applications.

The important files in this directory are:

DataReportingService.js
  An XPCOM service that coordinates collection and reporting of data.

policy.jsm
  A module containing the logic for coordinating and driving collection
  and upload of data.

sessions.jsm
  Records Gecko application session history. This is loaded as part of
  the XPCOM service because it needs to capture state from very early in
  the application lifecycle. Bug 841561 tracks implementing this in C++.


There is other code in the tree that collects and uploads data. The
original intent of this directory and XPCOM service was to serve as a
focal point for the coordination of all this activity so that it could
all be done consistently and properly. This vision may or may not be fully
realized.

