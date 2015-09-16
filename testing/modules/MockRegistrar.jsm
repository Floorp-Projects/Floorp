/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "MockRegistrar",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr, manager: Cm} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
var logger = Log.repository.getLogger("MockRegistrar");

this.MockRegistrar = Object.freeze({
  _registeredComponents: new Map(),
  _originalCIDs: new Map(),
  get registrar() {
    return Cm.QueryInterface(Ci.nsIComponentRegistrar);
  },

  /**
   * Register a mock to override target interfaces.
   * The target interface may be accessed through _genuine property of the mock.
   * If you register multiple mocks to the same contract ID, you have to call
   * unregister in reverse order. Otherwise the previous factory will not be
   * restored.
   *
   * @param contractID The contract ID of the interface which is overridden by
                       the mock.
   *                   e.g. "@mozilla.org/file/directory_service;1"
   * @param mock       An object which implements interfaces for the contract ID.
   * @param args       An array which is passed in the constructor of mock.
   *
   * @return           The CID of the mock.
   */
  register(contractID, mock, args) {
    let originalCID = this._originalCIDs.get(contractID);
    if (!originalCID) {
      originalCID = this.registrar.contractIDToCID(contractID);
      this._originalCIDs.set(contractID, originalCID);
    }

    let originalFactory = Cm.getClassObject(originalCID, Ci.nsIFactory);

    let factory = {
      createInstance(outer, iid) {
        if (outer) {
          throw Cr.NS_ERROR_NO_AGGREGATION;
        }

        let wrappedMock;
        if (mock.prototype && mock.prototype.constructor) {
          wrappedMock = Object.create(mock.prototype);
          mock.apply(wrappedMock, args);
        } else {
          wrappedMock = mock;
        }

        try {
          let genuine = originalFactory.createInstance(outer, iid);
          wrappedMock._genuine = genuine;
        } catch(ex) {
          logger.info("Creating original instance failed", ex);
        }

        return wrappedMock.QueryInterface(iid);
      },
      lockFactory(lock) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
    };

    this.registrar.unregisterFactory(originalCID, originalFactory);
    this.registrar.registerFactory(originalCID,
                                   "A Mock for " + contractID,
                                   contractID,
                                   factory);

    this._registeredComponents.set(originalCID, {
      contractID: contractID,
      factory: factory,
      originalFactory: originalFactory
    });

    return originalCID;
  },

  /**
   * Unregister the mock.
   *
   * @param cid The CID of the mock.
   */
  unregister(cid) {
    let component = this._registeredComponents.get(cid);
    if (!component) {
      return;
    }

    this.registrar.unregisterFactory(cid, component.factory);
    if (component.originalFactory) {
      this.registrar.registerFactory(cid,
                                     "",
                                     component.contractID,
                                     component.originalFactory);
    }

    this._registeredComponents.delete(cid);
  },

  /**
   * Unregister all registered mocks.
   */
  unregisterAll() {
    for (let cid of this._registeredComponents.keys()) {
      this.unregister(cid);
    }
  }

});
