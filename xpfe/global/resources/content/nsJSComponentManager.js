/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Ben Matthew Goodger <ben@netscape.com>
 *
 * Contributor(s): 
 */

var nsJSComponentManager = {
  createInstance: function (aContractID, aIID)
    {
      try
        {
          var iid = Components.interfaces[aIID];
          return Components.classes[aContractID].createInstance(iid);
        }
      catch(e)
        {
        }
        return null;
    },

  createInstanceByID: function (aID, aIID)
    {
      try
        {
          var iid = Components.interfaces[aIID];
          return Components.classesByID[aID].createInstance(iid);
        }
      catch(e)
        {
        }
        return null;
    },
        
  getService: function (aContractID, aIID)
    {
      try
        {
          var iid = Components.interfaces[aIID];
          return Components.classes[aContractID].getService(iid);
        }
      catch(e)
        {
        }
        return null;
    },
  
  getServiceByID: function (aID, aIID)  
    {
      try
        {
          var iid = Components.interfaces[aIID];
          return Components.classesByID[aID].getService(iid);
        }
      catch(e)
        {
        }
        return null;
    }
};
