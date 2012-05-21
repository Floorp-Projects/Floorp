/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEnumeratorUtils_h__
#define nsEnumeratorUtils_h__

#include "nscore.h"

class nsISupports;
class nsISimpleEnumerator;

NS_COM_GLUE nsresult
NS_NewEmptyEnumerator(nsISimpleEnumerator* *aResult);

NS_COM_GLUE nsresult
NS_NewSingletonEnumerator(nsISimpleEnumerator* *result,
                          nsISupports* singleton);

NS_COM_GLUE nsresult
NS_NewUnionEnumerator(nsISimpleEnumerator* *result,
                      nsISimpleEnumerator* firstEnumerator,
                      nsISimpleEnumerator* secondEnumerator);

#endif /* nsEnumeratorUtils_h__ */
