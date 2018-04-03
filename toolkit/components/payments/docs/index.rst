==============
WebPayments UI
==============

User Interface for the WebPayments `Payment Request API <https://w3c.github.io/browser-payment-api/>`_ and `Payment Handler API <https://w3c.github.io/payment-handler/>`_.

JSDoc style comments are used within the JS files of the component. This document will focus on higher-level and shared concepts.

.. toctree::
   :maxdepth: 5


Debugging/Development
=====================

Must Have Electrolysis
-------

Web Payments `does not work without e10s <https://bugzilla.mozilla.org/show_bug.cgi?id=1365964>`_!

Logging
-------

Set the pref ``dom.payments.loglevel`` to "Debug" to increase the verbosity of console messages.

Unprivileged UI Development
---------------------------
During development of the unprivileged custom elements, you can load the dialog from a
local server without even requiring a build. Simply run `./mach python toolkit/components/payments/server.py`
then load `http://localhost:8000/paymentRequest.xhtml?debug=1` in the browser.
Use the debugging console to load sample data.

Debugging Console
-----------------

To open the debugging console in the dialog, use the keyboard shortcut
**Ctrl-Alt-d (Ctrl-Option-d on macOS)**. While loading `paymentRequest.xhtml` directly in the
browser, add `?debug=1` to have the debugging console open by default.

Debugging the unprivileged frame with the developer tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To open a debugger in the context of the remote payment frame, click the "Debug frame" button in the
debugging console.

Use the `tabs` variable in the Browser Content Toolbox's console to access the frame contents.
There can be multiple frames loaded in the same process so you will need to find the correct tab
in the array by checking the file name is `paymentRequest.xhtml` (e.g. `tabs[0].content.location`).


Communication with the DOM
==========================

Communication from the DOM to the UI happens via the `paymentUIService.js` (implementing ``nsIPaymentUIService``).
The UI talks to the DOM code via the ``nsIPaymentRequestService`` interface.


Dialog Architecture
===================

Privileged wrapper XHTML document (paymentDialogWrapper.xhtml) containing a remote ``<iframe mozbrowser="true" remote="true">`` containing unprivileged XHTML (paymentRequest.xhtml).
Keeping the dialog contents unprivileged is useful since the dialog will render payment line items and shipping options that are provided by web developers and should therefore be considered untrusted.
In order to communicate across the process boundary a privileged frame script (`paymentDialogFrameScript.js`) is loaded into the iframe to relay messages.
This is because the unprivileged document cannot access message managers.
Instead, all communication across the privileged/unprivileged boundary is done via custom DOM events:

* A ``paymentContentToChrome`` event is dispatched when the dialog contents want to communicate with the privileged dialog wrapper.
* A ``paymentChromeToContent`` event is dispatched on the ``window`` with the ``detail`` property populated when the privileged dialog wrapper communicates with the unprivileged dialog.

These events are converted to/from message manager messages of the same name to communicate to the other process.
The purpose of `paymentDialogFrameScript.js` is to simply convert unprivileged DOM events to/from messages from the other process.
