
/* nsJARInputStream.h
 * 
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Netscape Communicator source code. The Initial Developer of the Original Code is Netscape Communications Corporation.  Portions created by Netscape are Copyright (C) 1999 Netscape Communications Corporation.  All Rights Reserved.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mitch Stoltz <mstoltz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsJARINPUTSTREAM_h__
#define nsJARINPUTSTREAM_h__

// {a756724a-1dd1-11b2-90d8-9c98fc2b7ac0}
#define NS_JARINPUTSTREAM_CID \
   {0xa756724a, 0x1dd1, 0x11b2, \
     {0x90, 0xd8, 0x9c, 0x98, 0xfc, 0x2b, 0x7a, 0xc0}}

#include "nsIInputStream.h"
#include "nsJAR.h"

/*-------------------------------------------------------------------------
 * Class nsJARInputStream declaration. This class defines objects returned
 * by calls to nsJAR::GetInputStream(filename) for the purpose of reading 
 * a file item out of a JAR file. 
 *------------------------------------------------------------------------*/
class nsJARInputStream : public nsIInputStream
{
  public:

    nsJARInputStream();
    virtual ~nsJARInputStream();
    
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_JARINPUTSTREAM_CID );
  
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
   
    nsresult 
    Init(nsJAR* jar, const char* aFilename);
  
  protected:
    nsZipReadState  mReadInfo;

    nsJAR*      mJAR;
};

#endif /* nsJAR_h__ */

