=====================
Firefox Health Report
=====================

This directory contains the implementation of the Firefox Health Report
(FHR).

Firefox Health Report is a background service that collects application
metrics and periodically submits them to a central server.

Implementation Notes
====================

The XPCOM service powering FHR is defined in HealthReportService.js. It
simply instantiates an instance of HealthReporter from healthreporter.jsm.

All the logic for enforcing the privacy policy and for scheduling data
submissions lives in policy.jsm.

Preferences
===========

Preferences controlling behavior of Firefox Health Report live in the
*healthreport.* branch.

Some important preferences are:

* **healthreport.serviceEnabled** - Controls whether the entire health report
  service runs. The overall service performs data collection, storing, and
  submission.

* **healthreport.policy.dataSubmissionEnabled** - Controls whether data
  submission is enabled. If this is *false*, data will still be collected
  and stored - it just won't ever be submitted to a remote server.

If the entire service is disabled, you lose data collection. This means that
data analysis won't be available because there is no data to analyze!

Other Notes
===========

There are many legal and privacy concerns with this code, especially
around the data that is submitted. Changes to submitted data should be
signed off by responsible parties.

