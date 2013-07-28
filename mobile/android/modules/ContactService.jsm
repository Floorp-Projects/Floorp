/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- Android ContactService component: " + s + "\n"); }

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = [];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PhoneNumberUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm", "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

let ContactService = {
  init: function() {
    if (DEBUG) debug("Init");
    this._requestMessages = {};

    // Add listeners for all messages from ContactManager.js
    let messages = ["Contacts:Clear", "Contacts:Find", "Contacts:GetAll",
                    "Contacts:GetAll:SendNow", "Contacts:GetCount", "Contacts:GetRevision",
                    "Contact:Remove", "Contact:Save",];
    messages.forEach(function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }.bind(this));

    // Add listeners for all messages from ContactService.java
    let returnMessages = ["Android:Contacts:Count",
                          "Android:Contacts:Clear:Return:OK", "Android:Contacts:Clear:Return:KO",
                          "Android:Contacts:Find:Return:OK", "Android:Contacts:Find:Return:KO",
                          "Android:Contacts:GetAll:Next", "Android:Contacts:RegisterForMessages",
                          "Android:Contact:Remove:Return:OK", "Android:Contact:Remove:Return:KO",
                          "Android:Contact:Save:Return:OK", "Android:Contact:Save:Return:KO",];

    returnMessages.forEach(function(msgName) {
      Services.obs.addObserver(this, msgName, false);
    }.bind(this));
  },

  _sendMessageToJava: function(aMsg) {
    Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge).handleGeckoMessage(JSON.stringify(aMsg));
  },

  _sendReturnMessage: function(aTopic, aRequestID, aResult) {
    this._requestMessages[aRequestID].target.sendAsyncMessage(aTopic, aResult);
  },

  _sendAndDeleteReturnMessage: function(aTopic, aRequestID, aResult) {
    this._sendReturnMessage(aTopic, aRequestID, aResult)
    delete this._requestMessages[aRequestID];
  },

  observe: function(aSubject, aTopic, aData) {
    if (DEBUG) {
      debug("observe: subject: " + aSubject + " topic: " + aTopic + " data: " + aData);
    }

    let message = JSON.parse(aData);
    let requestID = message.requestID;

    // The return message topic is the same as the current topic, but without the "Android:" prefix
    let returnMessageTopic = aTopic.substring(8);

    switch (aTopic) {
      case "Android:Contacts:Find:Return:OK":
        this._sendAndDeleteReturnMessage(returnMessageTopic, requestID, {requestID: requestID, contacts: message.contacts});
        break;

      case "Android:Contacts:Find:Return:KO":
        this._sendAndDeleteReturnMessage(returnMessageTopic, requestID, {requestID: requestID});
        break;

      case "Android:Contact:Save:Return:OK":
        this._sendReturnMessage(returnMessageTopic, requestID, {requestID: requestID, contactID: message.contactID});
        this._sendAndDeleteReturnMessage("Contact:Changed", requestID, {contactID: message.contactID, reason: message.reason});
        break;

      case "Android:Contact:Save:Return:KO":
        this._sendAndDeleteReturnMessage(returnMessageTopic, requestID, {requestID: requestID});
        break;

      case "Android:Contact:Remove:Return:OK":
        this._sendReturnMessage(returnMessageTopic, requestID, {requestID: requestID, contactID: message.contactID});
        this._sendAndDeleteReturnMessage("Contact:Changed", requestID, {contactID: message.contactID, reason: "remove"});
        break;

      case "Android:Contact:Remove:Return:KO":
        this._sendAndDeleteReturnMessage(returnMessageTopic, requestID, {requestID: requestID});
        break;

      case "Android:Contacts:Clear:Return:OK":
        this._sendReturnMessage(returnMessageTopic, requestID, {requestID: requestID});
        this._sendAndDeleteReturnMessage("Contact:Changed", requestID, {reason: "remove"});
        break;

      case "Android:Contact:Clear:Return:KO":
        this._sendAndDeleteReturnMessage(returnMessageTopic, requestID, {requestID: requestID});
        break;

      case "Android:Contacts:GetAll:Next":
        // GetAll uses a cursor ID instead of a request ID. Translate the request ID back to the cursor ID
        this._sendReturnMessage(returnMessageTopic, requestID, {cursorId: requestID, contacts: message.contacts});

        // Send a message with no contacts to denote the end of contacts returned by the query
        this._sendAndDeleteReturnMessage(returnMessageTopic, requestID, {cursorId: requestID});
        break;

      case "Android:Contacts:Count":
        this._sendAndDeleteReturnMessage(returnMessageTopic, requestID, {requestID: requestID, count: message.count});
        break;

      default:
        throw "Wrong message received: " + aTopic;
    }
  },

  assertPermission: function(aMessage, aPerm) {
    if (!aMessage.target.assertPermission(aPerm)) {
      Cu.reportError("Contacts message " + aMessage.name +
                     " from a content process with no" + aPerm + " privileges.");
      return false;
    }
    return true;
  },

  receiveMessage: function(aMessage) {
    if (DEBUG) debug("receiveMessage " + aMessage.name);

    // GetAll uses a cursor ID instead of a request ID, but they can be treated the same from here
    if (!aMessage.data.requestID && aMessage.data.cursorId) {
      aMessage.data.requestID = aMessage.data.cursorId;
    }
    let requestID = aMessage.data.requestID;

    // Store the message so it the request callback can be called when the Java side is finished
    this._requestMessages[requestID] = aMessage;

    switch (aMessage.name) {
      case "Contacts:Find":
        this.findContacts(aMessage);
        break;

      case "Contacts:GetAll":
        this.getAllContacts(aMessage);
        break;

      case "Contacts:GetAll:SendNow":
        // Send an empty message to denote there are no most contacts for the getAll query
        this._sendAndDeleteReturnMessage("Contacts:GetAll:Next", requestID, {cursorId: requestID});
        break;

      case "Contact:Save":
        this.saveContact(aMessage);
        break;

      case "Contact:Remove":
        this.removeContact(aMessage);
        break;

      case "Contacts:Clear":
        this.clearContacts(aMessage);
        break;

      case "Contacts:GetCount":
        this.getContactsCount(aMessage);
        break;

      case "Contacts:GetRevision":
        // Android does not support the get revision function
        this._sendAndDeleteReturnMessage("Contacts:GetRevision:Return:KO", requestID, {requestID: requestID,
                                          errorMsg: "Android does not support the revision function."});
        break;

      case "Contacts:RegisterForMessages":
        delete this._requestMessages[requestID];
        break;

      default:
        delete this._requestMessages[requestID];
        throw "Wrong message received: " + aMessage.name;
    }
  },

  findContacts: function(aMessage) {
    if (!this.assertPermission(aMessage, "contacts-read")) {
      return;
    }

    let countryName = PhoneNumberUtils.getCountryName();
    let substringmatchingPref = "dom.phonenumber.substringmatching." + countryName;
    let substringmatchingValue = 0;
    if (Services.prefs.getPrefType(substringmatchingPref) == Ci.nsIPrefBranch.PREF_INT) {
      substringmatchingValue = Services.prefs.getIntPref(substringmatchingPref);
    }

    // Add the substring matching value to the find options JSON
    aMessage.data.options.findOptions.substringMatching = substringmatchingValue;

    this._sendMessageToJava({type: "Android:Contacts:Find", data: aMessage.data});
  },

  getAllContacts: function(aMessage) {
    if (!this.assertPermission(aMessage, "contacts-read")) {
      return;
    }

    this._sendMessageToJava({type: "Android:Contacts:GetAll", data: aMessage.data});
  },

  saveContact: function(aMessage) {
    if ((aMessage.data.options.reason === "create" &&
        !this.assertPermission(aMessage, "contacts-create")) ||
        !this.assertPermission(aMessage, "contacts-write")) {
        return;
    }

    this._sendMessageToJava({type: "Android:Contact:Save", data: aMessage.data});
  },

  removeContact: function(aMessage) {
    if (!this.assertPermission(aMessage, "contacts-write")) {
      return;
    }

    this._sendMessageToJava({type: "Android:Contact:Remove", data: aMessage.data});
  },

  clearContacts: function(aMessage) {
    if (!this.assertPermission(aMessage, "contacts-write")) {
      return;
    }

    this._sendMessageToJava({type: "Android:Contacts:Clear", data: aMessage.data});
  },

  getContactsCount: function(aMessage) {
    if (!this.assertPermission(aMessage, "contacts-read")) {
      return;
    }

    this._sendMessageToJava({type: "Android:Contacts:GetCount", data: aMessage.data});
  },
}

ContactService.init();
