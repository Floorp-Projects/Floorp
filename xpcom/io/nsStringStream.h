/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   darin@netscape.com (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsStringStream_h__
#define nsStringStream_h__

#include "nsISeekableStream.h"

/* a6cf90e8-15b3-11d2-932e-00805f8add32 */
#define NS_IRANDOMACCESS_IID \
{  0xa6cf90eb, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

//========================================================================================
class nsIRandomAccessStore
// Supports Seek, Tell etc.
//========================================================================================
: public nsISeekableStream
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRANDOMACCESS_IID)

/* "PROTECTED" */
    NS_IMETHOD                         GetAtEOF(PRBool* outAtEOF) = 0;
    NS_IMETHOD                         SetAtEOF(PRBool inAtEOF) = 0;
}; // class nsIRandomAccessStore

#include "nsIStringStream.h"

/**
 * nsStringInputStream : nsIStringInputStream
 *                     , nsIInputStream
 *                     , nsISeekableStream
 *                     , nsIRandomAccessStore
 */
#define NS_STRINGINPUTSTREAM_CLASSNAME  "nsStringInputStream"
#define NS_STRINGINPUTSTREAM_CONTRACTID "@mozilla.org/io/string-input-stream;1"
#define NS_STRINGINPUTSTREAM_CID                     \
{ /* 0abb0835-5000-4790-af28-61b3ba17c295 */         \
    0x0abb0835,                                      \
    0x5000,                                          \
    0x4790,                                          \
    {0xaf, 0x28, 0x61, 0xb3, 0xba, 0x17, 0xc2, 0x95} \
}
extern NS_METHOD nsStringInputStreamConstructor(nsISupports *, REFNSIID, void **);



#endif // nsStringStream_h__
