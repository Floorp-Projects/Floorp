/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gRegTestExtComponent =
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

const kClassID = Components.ID("{fe64efb7-c5ab-41a6-b639-e6c0f483181e}");
this.NSGetFactory = function NSGetFactory(cid)
{
  if (!cid.equals(kClassID))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return gRegTestExtComponent;
}
