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


/**
 *
 * A sample of XPConnect. This file contains an implementation nsSample
 * of the interface nsISample.
 *
 */
#include <stdio.h>

#include "nsSample.h"
#include "nsMemory.h"

#include "nsEmbedString.h"
////////////////////////////////////////////////////////////////////////

nsSampleImpl::nsSampleImpl() : mValue(nsnull)
{
    mValue = (char*)nsMemory::Clone("initial value", 14);
}

nsSampleImpl::~nsSampleImpl()
{
    if (mValue)
        nsMemory::Free(mValue);
}

/**
 * NS_IMPL_ISUPPORTS1 expands to a simple implementation of the nsISupports
 * interface.  This includes a proper implementation of AddRef, Release,
 * and QueryInterface.  If this class supported more interfaces than just
 * nsISupports, 
 * you could use NS_IMPL_ADDREF() and NS_IMPL_RELEASE() to take care of the
 * simple stuff, but you would have to create QueryInterface on your own.
 * nsSampleFactory.cpp is an example of this approach.
 * Notice that the second parameter to the macro is name of the interface, and
 * NOT the #defined IID.
 *
 * The _CI variant adds support for nsIClassInfo, which permits introspection
 * and interface flattening.
 */
NS_IMPL_ISUPPORTS1_CI(nsSampleImpl, nsISample)

/**
 * Notice that in the protoype for this function, the NS_IMETHOD macro was
 * used to declare the return type.  For the implementation, the return
 * type is declared by NS_IMETHODIMP
 */
NS_IMETHODIMP
nsSampleImpl::GetValue(char** aValue)
{
    NS_PRECONDITION(aValue != nsnull, "null ptr");
    if (! aValue)
        return NS_ERROR_NULL_POINTER;

    if (mValue) {
        /**
         * GetValue's job is to return data known by an instance of
         * nsSampleImpl to the outside world.  If we  were to simply return 
         * a pointer to data owned by this instance, and the client were to
         * free it, bad things would surely follow.
         * On the other hand, if we create a new copy of the data for our
         * client, and it turns out that client is implemented in JavaScript,
         * there would be no way to free the buffer.  The solution to the 
         * buffer ownership problem is the nsMemory singleton.  Any buffer
         * returned by an XPCOM method should be allocated by the nsMemory.
         * This convention lets things like JavaScript reflection do their
         * job, and simplifies the way C++ clients deal with returned buffers.
         */
        *aValue = (char*) nsMemory::Clone(mValue, strlen(mValue) + 1);
        if (! *aValue)
            return NS_ERROR_NULL_POINTER;
    }
    else {
        *aValue = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSampleImpl::SetValue(const char* aValue)
{
    NS_PRECONDITION(aValue != nsnull, "null ptr");
    if (! aValue)
        return NS_ERROR_NULL_POINTER;

    if (mValue) {
        nsMemory::Free(mValue);
    }

    /**
     * Another buffer passing convention is that buffers passed INTO your
     * object ARE NOT YOURS.  Keep your hands off them, unless they are
     * declared "inout".  If you want to keep the value for posterity,
     * you will have to make a copy of it.
     */
    mValue = (char*) nsMemory::Clone(aValue, strlen(aValue) + 1);
    return NS_OK;
}

NS_IMETHODIMP
nsSampleImpl::Poke(const char* aValue)
{
    return SetValue((char*) aValue);
}


static void GetStringValue(nsACString& aValue)
{
    NS_CStringSetData(aValue, "GetValue");
}

NS_IMETHODIMP
nsSampleImpl::WriteValue(const char* aPrefix)
{
    NS_PRECONDITION(aPrefix != nsnull, "null ptr");
    if (! aPrefix)
        return NS_ERROR_NULL_POINTER;

    printf("%s %s\n", aPrefix, mValue);

    // This next part illustrates the nsEmbedString:
    nsEmbedString foopy;
    foopy.Append(PRUnichar('f'));
    foopy.Append(PRUnichar('o'));
    foopy.Append(PRUnichar('o'));
    foopy.Append(PRUnichar('p'));
    foopy.Append(PRUnichar('y'));
    
    const PRUnichar* f = foopy.get();
    PRUint32 l = foopy.Length();
    printf("%c%c%c%c%c %d\n", char(f[0]), char(f[1]), char(f[2]), char(f[3]), char(f[4]), l);
    
    nsEmbedCString foopy2;
    GetStringValue(foopy2);

    //foopy2.AppendLiteral("foopy");
    const char* f2 = foopy2.get();
    PRUint32 l2 = foopy2.Length();

    printf("%s %d\n", f2, l2);

    return NS_OK;
}
