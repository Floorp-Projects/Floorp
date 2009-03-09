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
 * The Original Code is Mozilla XPCOM.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "nsIVersionComparator.h"

class nsVersionComparatorImpl : public nsIVersionComparator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVERSIONCOMPARATOR
};

#define NS_VERSIONCOMPARATOR_CONTRACTID "@mozilla.org/xpcom/version-comparator;1"

// c6e47036-ca94-4be3-963a-9abd8705f7a8
#define NS_VERSIONCOMPARATOR_CID \
{ 0xc6e47036, 0xca94, 0x4be3, \
  { 0x96, 0x3a, 0x9a, 0xbd, 0x87, 0x05, 0xf7, 0xa8 } }

#define NS_VERSIONCOMPARATOR_CLASSNAME "nsVersionComparatorImpl"
