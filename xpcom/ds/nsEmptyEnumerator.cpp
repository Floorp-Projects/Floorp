/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   L. David Baron <dbaron@dbaron.org>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/*

  An empty enumerator.

 */

#include "nsEmptyEnumerator.h"

////////////////////////////////////////////////////////////////////////

MOZ_DECL_CTOR_COUNTER(EmptyEnumeratorImpl)

EmptyEnumeratorImpl::EmptyEnumeratorImpl(void)
{
    MOZ_COUNT_CTOR(EmptyEnumeratorImpl);
}

EmptyEnumeratorImpl::~EmptyEnumeratorImpl(void)
{
    MOZ_COUNT_DTOR(EmptyEnumeratorImpl);
}

// nsISupports interface
NS_IMETHODIMP_(nsrefcnt) EmptyEnumeratorImpl::AddRef(void)
{
    return 2;
}

NS_IMETHODIMP_(nsrefcnt) EmptyEnumeratorImpl::Release(void)
{
    return 1;
}

NS_IMPL_QUERY_INTERFACE1(EmptyEnumeratorImpl, nsISimpleEnumerator)


// nsISimpleEnumerator interface
NS_IMETHODIMP EmptyEnumeratorImpl::HasMoreElements(PRBool* aResult)
{
    *aResult = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP EmptyEnumeratorImpl::GetNext(nsISupports** aResult)
{
    return NS_ERROR_UNEXPECTED;
}

static EmptyEnumeratorImpl* gEmptyEnumerator = nsnull;

extern "C" NS_COM nsresult
NS_NewEmptyEnumerator(nsISimpleEnumerator** aResult)
{
    nsresult rv = NS_OK;
    if (!gEmptyEnumerator) {
        gEmptyEnumerator = new EmptyEnumeratorImpl();
        if (!gEmptyEnumerator)
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    *aResult = gEmptyEnumerator;
    return rv;
}

/* static */ void
EmptyEnumeratorImpl::Shutdown()
{
    delete gEmptyEnumerator;
    gEmptyEnumerator = nsnull;
}
