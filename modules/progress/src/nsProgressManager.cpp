/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "nsProgressManager.h"
#include "nsTopProgressManager.h"
#include "nsSubProgressManager.h"

static NS_DEFINE_IID(kITransferListenerIID, NS_ITRANSFERLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////
//
// nsProgressManager
//
//

/**
 * <p>
 * This unholiness is used to keep reference counts straight for
 * all the wild and wooly ways that a progress manager can get
 * created. It recursively creates and installs progress managers,
 * walking the grid hierarchy up to the root, if necessary. Recursion
 * terminates as soon as a context is found with a progress manager
 * already installed.
 * </p>
 *
 * <p>
 * Just for posterity, there are several ways that I can think of
 * that a progress manager might be born. We need to survive all
 * of them:
 * </p>
 *
 * <ol>
 * <li>
 * A progress manager can be created in a simple window context
 * </li>
 *
 * <li>
 * A progress manager can be created in a top-level grid context,
 * which will quickly receive an <b>AllConnectionsComplete</b>
 * as soon as it reads the HTML for the frameset.
 * </li>
 *
 * <li>
 * A progress manager can be created in a child grid context, in
 * a frameset.
 * </li>
 *
 * <li>
 * A progress manager can be created in a child grid context, in
 * response to somebody clicking a link on a page in the child
 * context
 * </li>
 * </ol>
 *
 * <p>
 * In each case, we need to make sure that we properly create
 * A top-level grid context and that everything gets refcounted
 * nicely. Could be nasty situations like, I'm loading a link
 * on a left frame, and the user gets bored and clicks a link
 * in the right frame! This should take care of that, too.
 * </p>
 */

static void
pm_RecursiveEnsure(MWContext* context)
{
    if (! context)
        return;

    if (context->progressManager)
        return;

    if (context->grid_parent) {
        pm_RecursiveEnsure(context->grid_parent);
        context->progressManager =
            new nsSubProgressManager(context);
    }
    else {   
        context->progressManager =
            new nsTopProgressManager(context);
    }
}

void
nsProgressManager::Ensure(MWContext* context)
{
    pm_RecursiveEnsure(context);

    if (context->progressManager)
        context->progressManager->AddRef();
}


void
nsProgressManager::Release(MWContext* context)
{
    if (! context)
        return;

    // Note that we DO NOT set the context's progressManager
    // slot to NULL. This is due to the obscene reference
    // counting that we do: the slot will be set to NULL by
    // the progressManager's destructor.
    if (context->progressManager)
        context->progressManager->Release();
}

nsProgressManager::nsProgressManager(MWContext* context)
    : fContext(context)
{
    NS_INIT_REFCNT();
}



nsProgressManager::~nsProgressManager(void)
{
    // HERE is where we null the context's progressManager slot. We
    // wait until the progress manager is actually destroyed, because
    // there are several ways that a reference to the progress manager
    // might be held, and unfortunately, the only public way to access
    // the progress manager is through the contexts.
    if (fContext) {
        fContext->progressManager = NULL;
        fContext = NULL;
    }
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(nsProgressManager);
NS_IMPL_RELEASE(nsProgressManager);

nsresult
nsProgressManager::QueryInterface(const nsIID& iid, void** result)
{
    if (iid.Equals(kITransferListenerIID)) {
        (*result) = (void*) ((nsITransferListener*) this);
        AddRef();
        return NS_OK;
    }
    else if (iid.Equals(kISupportsIID)) {
        (*result) = (void*) ((nsISupports*) this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

