/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla embedding code.
 *
 * The Initial Developers of the Original Code are
 * Benjamin Smedberg <bsmedberg@covad.net> and 
 * Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2003/2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsEnvironment_h__
#define nsEnvironment_h__

#include "nsIEnvironment.h"
#include "prlock.h"

#define NS_ENVIRONMENT_CID \
  { 0X3D68F92UL, 0X9513, 0X4E25, \
  { 0X9B, 0XE9, 0X7C, 0XB2, 0X39, 0X87, 0X41, 0X72 } }
#define NS_ENVIRONMENT_CONTRACTID "@mozilla.org/process/environment;1"

class nsEnvironment : public nsIEnvironment
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIENVIRONMENT

    static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID,
                           void **aResult);

private:
    nsEnvironment() { }
    ~nsEnvironment();

    PRLock *mLock;
};

#endif /* !nsEnvironment_h__ */
