/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Bob Lord <lord@netscape.com>
 *  Ian McGreer <mcgreer@netscape.com>
 */

function setWindowName()
{
  myName = self.name;
//  alert(myName);
  var windowReference=document.getElementById('certDetails');
  windowReference.setAttribute("title","Certificate Detail: "+myName);

  certmgr = Components
            .classes["@mozilla.org/security/certmanager;1"]
            .createInstance();
  certmgr = certmgr.QueryInterface(Components
                                   .interfaces
                                   .nsICertificateManager);

  cnstr = certmgr.getCertCN(myName);
  var cn=document.getElementById('commonname');
  cn.setAttribute("value", cnstr);
  // for now
  orgstr = certmgr.getCertCN(myName);
  var org=document.getElementById('organization');
  org.setAttribute("value", orgstr);
  oustr = certmgr.getCertCN(myName);
  var ou=document.getElementById('orgunit');
  ou.setAttribute("value", oustr);
}
