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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Delgadillo <javi@netscape.com>
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
#ifndef _NSSASNTREE_H_
#define _NSSASNTREE_H_

#include "nscore.h"
#include "nsIX509Cert.h"
#include "nsIASN1Tree.h"
#include "nsIASN1Object.h"
#include "nsIASN1Sequence.h"
#include "nsITreeView.h"
#include "nsITreeBoxObject.h"
#include "nsITreeSelection.h"
#include "nsCOMPtr.h"

//4bfaa9f0-1dd2-11b2-afae-a82cbaa0b606
#define NS_NSSASN1OUTINER_CID  {             \
   0x4bfaa9f0,                               \
   0x1dd2,                                   \
   0x11b2,                                   \
   {0xaf,0xae,0xa8,0x2c,0xba,0xa0,0xb6,0x06} \
  }
  

class nsNSSASN1Tree : public nsIASN1Tree
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIASN1TREE
  NS_DECL_NSITREEVIEW
  
  nsNSSASN1Tree();
  virtual ~nsNSSASN1Tree();
protected:

  class myNode
  {
  public:
    nsCOMPtr<nsIASN1Object> obj;
    nsCOMPtr<nsIASN1Sequence> seq;
    myNode *child;
    myNode *next;
    myNode *parent;
    
    myNode() {
      child = next = parent = nsnull;
    }
  };

  myNode *mTopNode;

  nsCOMPtr<nsIASN1Object> mASN1Object;
  nsCOMPtr<nsITreeSelection> mSelection;
  nsCOMPtr<nsITreeBoxObject> mTree;

  void InitNodes();
  void InitChildsRecursively(myNode *n);

  void ClearNodes();
  void ClearNodesRecursively(myNode *n);

  PRInt32 CountVisibleNodes(myNode *n);
  myNode *FindNodeFromIndex(myNode *n, PRInt32 wantedIndex,
                            PRInt32 &index_counter, PRInt32 &level_counter,
                            PRInt32 *optionalOutParentIndex, PRInt32 *optionalOutLevel);
  myNode *FindNodeFromIndex(PRInt32 wantedIndex, 
                            PRInt32 *optionalOutParentIndex = nsnull, 
                            PRInt32 *optionalOutLevel = nsnull);

};
#endif //_NSSASNTREE_H_
