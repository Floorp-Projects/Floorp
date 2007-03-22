/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
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

#ifndef _NSUUIDGENERATOR_H_
#define _NSUUIDGENERATOR_H_

#include "nsIUUIDGenerator.h"
#include "prlock.h"

class nsUUIDGenerator : public nsIUUIDGenerator {
public:
    nsUUIDGenerator();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIUUIDGENERATOR

    nsresult Init();

private:
    ~nsUUIDGenerator();

protected:

    PRLock* mLock;
#if !defined(XP_WIN) && !defined(XP_MACOSX)
    char mState[128];
    char *mSavedState;
    PRUint8 mRBytes;
#endif
};

#define NS_UUID_GENERATOR_CONTRACTID "@mozilla.org/uuid-generator;1"
#define NS_UUID_GENERATOR_CLASSNAME "UUID Generator"
#define NS_UUID_GENERATOR_CID \
{ 0x706d36bb, 0xbf79, 0x4293, \
{ 0x81, 0xf2, 0x8f, 0x68, 0x28, 0xc1, 0x8f, 0x9d } }

#endif /* _NSUUIDGENERATOR_H_ */
