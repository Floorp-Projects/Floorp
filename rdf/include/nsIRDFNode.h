/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIRDFNode_h__
#define nsIRDFNode_h__

#include "nsISupports.h"

class nsString;
class nsIAtom;

// {0F78DA50-8321-11d2-8EAC-00805F29F370}
#define NS_IRDFNODE_IID \
{ 0xf78da50, 0x8321, 0x11d2, { 0x8e, 0xac, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

/**
  A globally unique node in the RDF graph that can be compared 
  for equality using "==". Nodes are created using an instance of
  nsIRDFResourceManager, which should be obtained from the service
  manager:

  nsIRDFResourceManager* mgr;
  if (NS_SUCCEEDED(nsServiceManager::GetService(kCRDFResourceManagerCID,
                                                kIRDFResourceManagerIID,
                                                &mgr))) {
      nsIRDFNode* node;
      if (NS_SUCCEEDED(mgr->GetNode(nsString("http://foo.bar/"), node))) {
          // do something useful here...
          NS_IF_RELEASE(node);
      }
      nsServiceManager::ReleaseService(kCRDFManagerCID, mgr);
  }

 */

class nsIRDFNode : public nsISupports {
public:
    /**
     * Get the string value of the node.
     */
    NS_IMETHOD GetStringValue(nsString& value) const = 0;

    /**
     * Get the string value of the node as an atom
     */
    NS_IMETHOD GetAtomValue(nsIAtom*& value) const = 0;
};



#endif // nsIRDFNode_h__
