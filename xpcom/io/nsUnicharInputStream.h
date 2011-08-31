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
 * the Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Smedberg <benjamin@smedbergs.us> (Initial author)
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

#ifndef nsUnicharInputStream_h__
#define nsUnicharInputStream_h__

#include "nsISimpleUnicharStreamFactory.h"
#include "nsIUnicharInputStream.h"
#include "nsIFactory.h"

// {428DCA6F-1A0F-4cda-B516-0D5244745A6A}
#define NS_SIMPLE_UNICHAR_STREAM_FACTORY_CID \
{ 0x428dca6f, 0x1a0f, 0x4cda, { 0xb5, 0x16, 0xd, 0x52, 0x44, 0x74, 0x5a, 0x6a } }

class nsSimpleUnicharStreamFactory :
  public nsIFactory, 
  private nsISimpleUnicharStreamFactory
{
public:
  nsSimpleUnicharStreamFactory() {}
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFACTORY
  NS_DECL_NSISIMPLEUNICHARSTREAMFACTORY

  static nsSimpleUnicharStreamFactory* GetInstance();

private:
  static const nsSimpleUnicharStreamFactory kInstance;
};

#endif // nsUnicharInputStream_h__
