/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
 * This class can be used to expose nsI(BiDirectional)Enumerator interfaces
 * around nsHashtable objects.  
 * Contributed by Rob Ginda, rginda@ix.netcom.com
 */

#ifndef nsHashtableEnumerator_h___
#define nsHashtableEnumerator_h___

#include "nscore.h"
#include "nsIEnumerator.h"
#include "nsHashtable.h"

typedef NS_CALLBACK(NS_HASH_ENUMERATOR_CONVERTER) (nsHashKey *key, void *data,
                                                   void *convert_data,
                                                   nsISupports **retval);

extern nsresult
NS_NewHashtableEnumerator (nsHashtable *aHash, 
                           NS_HASH_ENUMERATOR_CONVERTER aConverter,
                           void *aData, nsIEnumerator **retval);


#endif /* nsHashtableEnumerator_h___ */

