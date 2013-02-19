=====================
Firefox Health Report
=====================

This directory contains the implementation of the Firefox Health Report
(FHR).

Firefox Health Report is a background service that collects application
metrics and periodically submits them to a central server.

Implementation Notes
====================

healthreporter.jsm contains the main interface for FHR, the
*HealthReporter* type. An instance of this is created by the data
reporting service. See /services/datareporting/.

providers.jsm contains numerous Metrics.Provider and Metrics.Measurement
used for collecting application metrics. If you are looking for the FHR
probes, this is where they are.

Preferences
===========

Preferences controlling behavior of Firefox Health Report live in the
*datareporting.healthreport.* branch.

Some important preferences are:

* **service.enabled** - Controls whether the entire health report
  service runs. The overall service performs data collection, storing, and
  submission. This is the primary kill switch for Firefox Health Report
  outside of the build system variable. i.e. if you are using an
  official Firefox build and wish to disable FHR, this is what you
  should change.

* **uploadEnabled** - Whether uploading of data is enabled. This
  is the preference the checkbox in the UI reflects. If this is
  disabled, FHR still collects data - it just doesn't upload it.

If the entire service is disabled, you lose data collection. This means that
data analysis won't be available because there is no data to analyze!

Registering Providers
=====================

Firefox Health Report providers are registered via the category manager.
See HealthReportComponents.manifest for providers defined in this
directory.

Essentially, the category manager receives the name of a JS type and the
URI of a JSM to import that exports this symbol. At run-time, the
providers registered in the category manager are instantiated. This
allows for a loose coupling of providers which in turns makes managing
the code behind the providers much simpler.

Other Notes
===========

There are many legal and privacy concerns with this code, especially
around the data that is submitted. Changes to submitted data should be
signed off by responsible parties.

