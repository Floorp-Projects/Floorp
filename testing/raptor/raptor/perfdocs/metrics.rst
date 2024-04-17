################
Metrics Gathered
################

.. contents::
   :depth: 2
   :local:

**WARNING: This page is still being actively developed.**

This document contains information about the metrics gathered in Browsertime tests, as well as detailed information of how they are gathered.

Pageload Tests
--------------

For browsertime pageload tests, there is a limited set of metrics that we collect (which can easily be expanded). Currently we divide these into two sets of metrics: (i) visual metrics, and (ii) technical metrics. These are gathered through two types of tests called warm and cold pageload tests. We have combined these two into a single "chimera" mode which you'll find in the Treeherder tasks.

Below, you can find the process of how we run Warm, Cold, and Chimera pageload tests.

Warm Pageload
==============

In this pageload test type, we open the browser, then repeatedly navigate to the same page without restarting the browser in between cycles.

* A new, or conditioned, browser profile is created
* The browser is started up
* Post-startup browser settle pause of 30 seconds or 1 second if using a conditioned profile
* A new tab is opened
* The test URL is loaded; measurements taken
* The tab is reloaded ``X`` more times (for ``X`` replicates); measurements taken each time

NOTES:
- The measurements from the first page-load are not included in overall results metrics b/c of first load noise; however they are listed in the JSON artifacts
- The bytecode cache gets populated on the first test cycle, and subsequent iterations will already have the cache built to reduce noise.

Cold Pageload
==============

In this pageload test type, we open the browser, navigate to the page, then restart the browser before performing the next cycle.

* A new, or conditioned, browser profile is created
* The browser is started up
* Post-startup browser settle pause of 30 seconds or 1 second if using a conditioned profile
* A new tab is opened
* The test URL is loaded; measurements taken
* The browser is shut down
* Entire process is repeated for the remaining browser cycles

NOTE: The measurements from all browser cycles are used to calculate overall results

Chimera Pageload
================

A new mode for pageload testing is called chimera mode. It combines the warm and cold variants into a single test. This test mode is used in our Taskcluster tasks.

* A new, or conditioned, browser profile is created
* The browser is started up
* Post-startup browser settle pause of 30 seconds or 1 second if using a conditioned profile
* A new tab is opened
* The test URL is loaded; measurements taken for ``Cold pageload``
* Navigate to a secondary URL (to preserve the content process)
* The test URL is loaded again; measurements taken for ``Warm pageload``
* The desktop browser is shut down
* Entire process is repeated for the remaining browser cycles

NOTE: The bytecode cache mentioned in Warm pageloads still applies here.

Technical Metrics
=================

These are metrics that are obtained from the browser. This includes metrics like First Paint, DOM Content Flushed, etc..

Visual Metrics
==============

When you run Raptor Browsertime with ``--browsertime-visualmetrics``, it will record a video of the page being loaded and then process this video to build the metrics. The video is either produced using FFMPEG (with ``--browsertime-no-ffwindowrecorder``) or the Firefox Window Recorder (default).


Benchmarks
----------

Benchmarks gather their own custom metrics unlike the pageload tests above. Please ping the owners of those benchmarks to determine what they mean and how they are produced, or reach out to the Performance Test and Tooling team in #perftest on Element.

Metric Definitions
------------------

The following documents all available metrics that current alert in Raptor Browsertime tests.

{metrics_documentation}
