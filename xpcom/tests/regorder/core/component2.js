/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gRegTestCoreComponent =
{
  /* nsISupports implementation. */
  QueryInterface: function (aIID)
  {
    if (!aIID.equals(Components.interfaces.nsISupports) &&
        !aIID.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
  },

  /* nsIFactory implementation. */
  createInstance: function (aOuter, aIID)
  {
    if (null != aOuter)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    return this.QueryInterface(aIID);
  },

  lockFactory: function (aDoLock) {}
};

const kClassID = Components.ID("{d04d1298-6dac-459b-a13b-bcab235730a0}");

this.NSGetFactory =
  function NSGetFactory(aClassID) {
  if (!aClassID.equals(kClassID))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return gRegTestCoreComponent;
}
