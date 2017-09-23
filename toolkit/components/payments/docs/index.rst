==============
WebPayments UI
==============

User Interface for the WebPayments `Payment Request API <https://w3c.github.io/browser-payment-api/>`_ and `Payment Handler API <https://w3c.github.io/payment-handler/>`_.

JSDoc style comments are used within the JS files of the component. This document will focus on higher-level and shared concepts.

.. toctree::
   :maxdepth: 5

Debugging
=========

Set the pref ``dom.payments.loglevel`` to "Debug".


Communication with the DOM
==========================

Communication from the DOM to the UI happens via the `paymentUIService.js` (implementing ``nsIPaymentUIService``).
The UI talks to the DOM code via the ``nsIPaymentRequestService`` interface.


Dialog Architecture
===================

Privileged wrapper XHTML document (paymentDialog.xhtml) containing a remote ``<iframe mozbrowser="true" remote="true">`` containing unprivileged XHTML (paymentRequest.xhtml).
Keeping the dialog contents unprivileged is useful since the dialog will render payment line items and shipping options that are provided by web developers and should therefore be considered untrusted.
In order to communicate across the process boundary a privileged frame script (`paymentDialogFrameScript.js`) is loaded into the iframe to relay messages.
This is because the unprivileged document cannot access message managers.
Instead, all communication across the privileged/unprivileged boundary is done via custom DOM events:

* A ``paymentContentToChrome`` event is dispatched when the dialog contents want to communicate with the privileged dialog wrapper.
* A ``paymentChromeToContent`` event is dispatched on the ``document`` with the ``detail`` property populated when the privileged dialog wrapper communicates with the unprivileged dialog.

These events are converted to/from message manager messages of the same name to communicate to the other process.
The purpose of `paymentDialogFrameScript.js` is to simply convert unprivileged DOM events to/from messages from the other process.
