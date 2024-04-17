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

First Paint
===========
Denotes the first time the browser performs a paint that has content in it.


  * **Aliases**: First Contentful Composite, First Contentful Paint, fcp
  * **Tests using it**:
     * **Custom**: `browsertime <raptor.html#browsertime-c>`__, `connect <raptor.html#connect-c>`__, `process-switch <raptor.html#process-switch-c>`__, `throttled <raptor.html#throttled-c>`__, `welcome <raptor.html#welcome-c>`__
     * **Desktop**: `amazon <raptor.html#amazon-d>`__, `bing-search <raptor.html#bing-search-d>`__, `buzzfeed <raptor.html#buzzfeed-d>`__, `cnn <raptor.html#cnn-d>`__, `ebay <raptor.html#ebay-d>`__, `espn <raptor.html#espn-d>`__, `expedia <raptor.html#expedia-d>`__, `facebook <raptor.html#facebook-d>`__, `fandom <raptor.html#fandom-d>`__, `google-docs <raptor.html#google-docs-d>`__, `google-mail <raptor.html#google-mail-d>`__, `google-search <raptor.html#google-search-d>`__, `google-slides <raptor.html#google-slides-d>`__, `imdb <raptor.html#imdb-d>`__, `imgur <raptor.html#imgur-d>`__, `instagram <raptor.html#instagram-d>`__, `linkedin <raptor.html#linkedin-d>`__, `microsoft <raptor.html#microsoft-d>`__, `netflix <raptor.html#netflix-d>`__, `nytimes <raptor.html#nytimes-d>`__, `office <raptor.html#office-d>`__, `outlook <raptor.html#outlook-d>`__, `paypal <raptor.html#paypal-d>`__, `pinterest <raptor.html#pinterest-d>`__, `reddit <raptor.html#reddit-d>`__, `tumblr <raptor.html#tumblr-d>`__, `twitch <raptor.html#twitch-d>`__, `twitter <raptor.html#twitter-d>`__, `wikia <raptor.html#wikia-d>`__, `wikipedia <raptor.html#wikipedia-d>`__, `yahoo-mail <raptor.html#yahoo-mail-d>`__, `youtube <raptor.html#youtube-d>`__
     * **Interactive**: `cnn-nav <raptor.html#cnn-nav-i>`__, `facebook-nav <raptor.html#facebook-nav-i>`__, `reddit-billgates-ama <raptor.html#reddit-billgates-ama-i>`__, `reddit-billgates-post-1 <raptor.html#reddit-billgates-post-1-i>`__, `reddit-billgates-post-2 <raptor.html#reddit-billgates-post-2-i>`__
     * **Live**: `booking-sf <raptor.html#booking-sf-l>`__, `discord <raptor.html#discord-l>`__, `fashionbeans <raptor.html#fashionbeans-l>`__, `google-accounts <raptor.html#google-accounts-l>`__, `imdb-firefox <raptor.html#imdb-firefox-l>`__, `medium-article <raptor.html#medium-article-l>`__, `people-article <raptor.html#people-article-l>`__, `reddit-thread <raptor.html#reddit-thread-l>`__, `rumble-fox <raptor.html#rumble-fox-l>`__, `stackoverflow-question <raptor.html#stackoverflow-question-l>`__, `urbandictionary-define <raptor.html#urbandictionary-define-l>`__, `wikia-marvel <raptor.html#wikia-marvel-l>`__
     * **Mobile**: `allrecipes <raptor.html#allrecipes-m>`__, `amazon <raptor.html#amazon-m>`__, `amazon-search <raptor.html#amazon-search-m>`__, `bild-de <raptor.html#bild-de-m>`__, `bing <raptor.html#bing-m>`__, `bing-search-restaurants <raptor.html#bing-search-restaurants-m>`__, `booking <raptor.html#booking-m>`__, `cnn <raptor.html#cnn-m>`__, `cnn-ampstories <raptor.html#cnn-ampstories-m>`__, `dailymail <raptor.html#dailymail-m>`__, `ebay-kleinanzeigen <raptor.html#ebay-kleinanzeigen-m>`__, `ebay-kleinanzeigen-search <raptor.html#ebay-kleinanzeigen-search-m>`__, `espn <raptor.html#espn-m>`__, `facebook <raptor.html#facebook-m>`__, `facebook-cristiano <raptor.html#facebook-cristiano-m>`__, `google <raptor.html#google-m>`__, `google-maps <raptor.html#google-maps-m>`__, `google-search-restaurants <raptor.html#google-search-restaurants-m>`__, `imdb <raptor.html#imdb-m>`__, `instagram <raptor.html#instagram-m>`__, `microsoft-support <raptor.html#microsoft-support-m>`__, `reddit <raptor.html#reddit-m>`__, `sina <raptor.html#sina-m>`__, `stackoverflow <raptor.html#stackoverflow-m>`__, `wikipedia <raptor.html#wikipedia-m>`__, `youtube <raptor.html#youtube-m>`__, `youtube-watch <raptor.html#youtube-watch-m>`__
     * **Unittests**: `test-page-1 <raptor.html#test-page-1-u>`__, `test-page-2 <raptor.html#test-page-2-u>`__, `test-page-3 <raptor.html#test-page-3-u>`__, `test-page-4 <raptor.html#test-page-4-u>`__


In Progress
===========
Many metrics have not been documented yet, this is a placeholder.

  * **Aliases**: 
  * **Tests using it**:
     * **Benchmarks**: `youtube-playback <raptor.html#youtube-playback-b>`__, `youtube-playback-hfr <raptor.html#youtube-playback-hfr-b>`__
     * **Custom**: `browsertime <raptor.html#browsertime-c>`__, `connect <raptor.html#connect-c>`__, `process-switch <raptor.html#process-switch-c>`__, `throttled <raptor.html#throttled-c>`__, `welcome <raptor.html#welcome-c>`__
     * **Desktop**: `amazon <raptor.html#amazon-d>`__, `bing-search <raptor.html#bing-search-d>`__, `buzzfeed <raptor.html#buzzfeed-d>`__, `cnn <raptor.html#cnn-d>`__, `ebay <raptor.html#ebay-d>`__, `espn <raptor.html#espn-d>`__, `expedia <raptor.html#expedia-d>`__, `facebook <raptor.html#facebook-d>`__, `fandom <raptor.html#fandom-d>`__, `google-docs <raptor.html#google-docs-d>`__, `google-mail <raptor.html#google-mail-d>`__, `google-search <raptor.html#google-search-d>`__, `google-slides <raptor.html#google-slides-d>`__, `imdb <raptor.html#imdb-d>`__, `imgur <raptor.html#imgur-d>`__, `instagram <raptor.html#instagram-d>`__, `linkedin <raptor.html#linkedin-d>`__, `microsoft <raptor.html#microsoft-d>`__, `netflix <raptor.html#netflix-d>`__, `nytimes <raptor.html#nytimes-d>`__, `office <raptor.html#office-d>`__, `outlook <raptor.html#outlook-d>`__, `paypal <raptor.html#paypal-d>`__, `pinterest <raptor.html#pinterest-d>`__, `reddit <raptor.html#reddit-d>`__, `tumblr <raptor.html#tumblr-d>`__, `twitch <raptor.html#twitch-d>`__, `twitter <raptor.html#twitter-d>`__, `wikia <raptor.html#wikia-d>`__, `wikipedia <raptor.html#wikipedia-d>`__, `yahoo-mail <raptor.html#yahoo-mail-d>`__, `youtube <raptor.html#youtube-d>`__
     * **Interactive**: `cnn-nav <raptor.html#cnn-nav-i>`__, `facebook-nav <raptor.html#facebook-nav-i>`__, `reddit-billgates-ama <raptor.html#reddit-billgates-ama-i>`__, `reddit-billgates-post-1 <raptor.html#reddit-billgates-post-1-i>`__, `reddit-billgates-post-2 <raptor.html#reddit-billgates-post-2-i>`__
     * **Live**: `booking-sf <raptor.html#booking-sf-l>`__, `discord <raptor.html#discord-l>`__, `fashionbeans <raptor.html#fashionbeans-l>`__, `google-accounts <raptor.html#google-accounts-l>`__, `imdb-firefox <raptor.html#imdb-firefox-l>`__, `medium-article <raptor.html#medium-article-l>`__, `people-article <raptor.html#people-article-l>`__, `reddit-thread <raptor.html#reddit-thread-l>`__, `rumble-fox <raptor.html#rumble-fox-l>`__, `stackoverflow-question <raptor.html#stackoverflow-question-l>`__, `urbandictionary-define <raptor.html#urbandictionary-define-l>`__, `wikia-marvel <raptor.html#wikia-marvel-l>`__
     * **Mobile**: `allrecipes <raptor.html#allrecipes-m>`__, `amazon <raptor.html#amazon-m>`__, `amazon-search <raptor.html#amazon-search-m>`__, `bild-de <raptor.html#bild-de-m>`__, `bing <raptor.html#bing-m>`__, `bing-search-restaurants <raptor.html#bing-search-restaurants-m>`__, `booking <raptor.html#booking-m>`__, `cnn <raptor.html#cnn-m>`__, `cnn-ampstories <raptor.html#cnn-ampstories-m>`__, `dailymail <raptor.html#dailymail-m>`__, `ebay-kleinanzeigen <raptor.html#ebay-kleinanzeigen-m>`__, `ebay-kleinanzeigen-search <raptor.html#ebay-kleinanzeigen-search-m>`__, `espn <raptor.html#espn-m>`__, `facebook <raptor.html#facebook-m>`__, `facebook-cristiano <raptor.html#facebook-cristiano-m>`__, `google <raptor.html#google-m>`__, `google-maps <raptor.html#google-maps-m>`__, `google-search-restaurants <raptor.html#google-search-restaurants-m>`__, `imdb <raptor.html#imdb-m>`__, `instagram <raptor.html#instagram-m>`__, `microsoft-support <raptor.html#microsoft-support-m>`__, `reddit <raptor.html#reddit-m>`__, `sina <raptor.html#sina-m>`__, `stackoverflow <raptor.html#stackoverflow-m>`__, `wikipedia <raptor.html#wikipedia-m>`__, `youtube <raptor.html#youtube-m>`__, `youtube-watch <raptor.html#youtube-watch-m>`__
     * **Unittests**: `test-page-1 <raptor.html#test-page-1-u>`__, `test-page-2 <raptor.html#test-page-2-u>`__, `test-page-3 <raptor.html#test-page-3-u>`__, `test-page-4 <raptor.html#test-page-4-u>`__


Youtube Playback Metrics
========================
Metrics starting with VP9/H264 give the number of frames dropped, and painted.


  * **Aliases**: H264, VP9
  * **Tests using it**:
     * **Benchmarks**: `youtube-playback <raptor.html#youtube-playback-b>`__, `youtube-playback-hfr <raptor.html#youtube-playback-hfr-b>`__


