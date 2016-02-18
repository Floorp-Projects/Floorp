.. -*- Mode: rst; fill-column: 100; -*-

==========================
 Shipping Default Domains
==========================

Firefox for Mobile (Android and iOS) ships sets of default content in order to improve the
first-run experience. There are two primary places where default sets of domains are used: URLBar
domain auto-completion, and Top Sites suggested thumbnails.

The source of these domains is typically the Alexa top sites lists, global and by-country. Before
shipping the sets of domains, the lists are sanitized.

Domain Auto-completion
======================

As you type in the URLBar, Firefox will scan your history and auto-complete previously visited
domains that match what you have entered. This can make navigating to web sites faster because it
can avoid significant amounts of typing. During your first few uses, Firefox does not have any
history and you are forced to type full URLs. Shipping a set of top domains provides a fallback.

The top domains list can be localized, but Firefox will fallback to using en-US as the default for all
locales that do not provide a specific set. The list can have several hundred domains, but due to
size concerns, is usually capped to five hundred or less.

Sanitizing Methods
------------------

After getting a source list, e.g. Alexa top global sites, we apply some simple guidelines to the
list of domains:


* Remove any sites in the Alexa adult site list.
* Remove any locale-specific domain duplicates. We assume primary URLs (.com) will redirect to the
  correct locale (.co.jp) at run-time.
* Remove any explicit adult content* domains.
* Remove any sites that use explicit or adult advertising*.
* Remove any URL shorteners and redirecters.
* Remove any content/CDN domains. Some sites use separate domains to store images and other static content.
* Remove any sites primarily used for advertising or management of advertising.
* Remove any sites that fail to load in mobile browsers.
* Remove any time/date specific sites that may have appeared on the list due to seasonal spikes.

Suggested Sites
===============

Suggested sites are default thumbnails, displayed on the Top Sites home panel. A suggested site
consists of a title, thumbnail image, background color and URL. Multiple images are usually
required to handle the variety of device DPIs.

Suggested sites can be localized, but Firefox will fallback to using en-US as the default for all
locales that do not provide a specific set. The list is usually small, with perhaps fewer than ten
sites.

Sanitizing Methods
------------------

After getting a source list, e.g. Alexa top global sites, we apply some simple guidelines to the
list of domains:

* Remove pure search engines. We handle search engines differently and don't consider them to be
  suggested sites.
* Remove any locale-specific domain duplicates. We assume primary URLs (.com) will redirect to the
  correct locale (.co.jp) at run-time.
* Remove any explicit adult content domains.
* Remove any sites that use explicit or adult advertising.
* Remove any URL shorteners and redirecters.
* Remove any content/CDN domains. Some sites use separate domains to store images and other static
  content.

Guidelines for Adult Content
============================

Generally the Adult category includes sites whose dominant theme is either:
* To appeal to the prurient interest in sex without any serious literary, artistic, political, or
  scientific value
* The depiction or description of nudity, including sexual or excretory activities or organs in a
  lascivious way
* The depiction or description of sexually explicit conduct in a lascivious way (e.g. for
  entertainment purposes)

For a more complete definition and guidelines of adult content, use the full DMOZ guidelines at
http://www.dmoz.org/docs/en/guidelines/adult/general.html.

Updating Lists
==============

After approximately every two releases, Product (with Legal) will review current lists and
sanitizing methods, and update the lists accordingly.
