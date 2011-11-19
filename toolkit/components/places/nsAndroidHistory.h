/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kartikaya Gupta <kgupta@mozilla.com>
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

#ifndef NS_ANDROIDHISTORY_H
#define NS_ANDROIDHISTORY_H

#include "IHistory.h"
#include "nsDataHashtable.h"
#include "nsTPriorityQueue.h"
#include "nsThreadUtils.h"

#define NS_ANDROIDHISTORY_CID \
    {0xCCAA4880, 0x44DD, 0x40A7, {0xA1, 0x3F, 0x61, 0x56, 0xFC, 0x88, 0x2C, 0x0B}}

class nsAndroidHistory : public mozilla::IHistory, public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IHISTORY
  NS_DECL_NSIRUNNABLE

  /**
   * Obtains a pointer that has had AddRef called on it.  Used by the service
   * manager only.
   */
  static nsAndroidHistory* GetSingleton();

  nsAndroidHistory();
  static void NotifyURIVisited(const nsString& str);

private:
  static nsAndroidHistory* sHistory;

  nsDataHashtable<nsStringHashKey, nsTArray<mozilla::dom::Link *> *> mListeners;
  nsTPriorityQueue<nsString> mPendingURIs;
};

#endif
