New-to-Necko Resources - An Aggregation
=======================================

This doc serves as a hub for resources/technologies a new-to-necko developer
should get familiar with.

Code Generation and IPC
~~~~~~~~~~~~~~~~~~~~~~~

* `IPC`_ and `IPDL`_
* `IDL`_
    - Implementing an interface (C++/JS)
    - XPCONNECT (scriptable/builtin)
    - QueryInterface (QI) - do_QueryInterface/do_QueryObject
    - do_GetService, do_CreateInstance
* `WebIDL`_

.. _IPC: /toolkit/components/glean/dev/ipc.html
.. _IDL: /xpcom/xpidl.html
.. _IPDL: /ipc/ipdl.html
.. _WebIDL: /toolkit/components/extensions/webextensions/webidl_bindings.html


Necko interfaces
~~~~~~~~~~~~~~~~

* :searchfox:`nsISupports <xpcom/base/nsISupports.idl>`
* :searchfox:`nsIRequest <netwerk/base/nsIRequest.idl>` ->
  :searchfox:`nsIChannel <netwerk/base/nsIChannel.idl>` ->
  :searchfox:`nsIHttpChannel <netwerk/protocol/http/nsIHttpChannel.idl>`
* :searchfox:`nsIRequestObserver <netwerk/base/nsIRequestObserver.idl>` (onStart/onStopRequest)
* :searchfox:`nsIStreamListener <netwerk/base/nsIStreamListener.idl>` (onDataAvailable)
* :searchfox:`nsIInputStream <xpcom/io/nsIInputStream.idl>`/
  :searchfox:`nsIOutputStream <xpcom/io/nsIOutputStream.idl>`

Libraries
~~~~~~~~~
* `NSPR`_
* `NSS`_
* `PSM`_

.. _NSPR: https://firefox-source-docs.mozilla.org/nspr/about_nspr.html?highlight=nspr
.. _NSS: https://firefox-source-docs.mozilla.org/security/nss/legacy/faq/index.html
.. _PSM: https://firefox-source-docs.mozilla.org/security/nss/legacy/faq/index.html?highlight=psm


Preferences
~~~~~~~~~~~
* :searchfox:`all.js <modules/libpref/init/all.js>`
* :searchfox:`firefox.js <browser/app/profile/firefox.js>`
* :searchfox:`StaticPrefList.yaml <modules/libpref/init/StaticPrefList.yaml>`


Testing
~~~~~~~
* `xpcshell`_
* `mochitest`_
* `web-platform`_
* `gtest`_
* `marionette`_

.. _xpcshell: /testing/xpcshell/index.html
.. _mochitest:  /browser/components/newtab/docs/v2-system-addon/mochitests.html
.. _web-platform: /web-platform/index.html
.. _gtest: /gtest/index.html
.. _marionette: /testing/marionette/index.html


See also
~~~~~~~~
  - E10S (Electrolysis) -> Split ``HttpChannel`` into: ``HttpChannelChild`` & ``HttpChannelParent``
  - Fission -> Site isolation
