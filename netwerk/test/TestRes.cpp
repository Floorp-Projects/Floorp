/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIResProtocolHandler.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIInputStream.h"
#include "nsIComponentManager.h"
#include "nsIStreamListener.h"
#include "nsIEventQueueService.h"
#include "nsIURI.h"
#include "nsCRT.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsresult
SetupMapping()
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProtocolHandler> ph;
    rv = serv->GetProtocolHandler("res", getter_AddRefs(ph));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIResProtocolHandler> resPH = do_QueryInterface(ph, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = resPH->AppendSubstitution("foo", "file://y|/");
    if (NS_FAILED(rv)) return rv;

    rv = resPH->AppendSubstitution("foo", "file://y|/mozilla/dist/win32_D.OBJ/bin/");
    if (NS_FAILED(rv)) return rv;

    return rv;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
TestOpenInputStream(const char* url)
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = serv->NewChannel(url,
                          nsnull, // base uri
                          getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> in;
    rv = channel->Open(getter_AddRefs(in));
    if (NS_FAILED(rv)) {
        fprintf(stdout, "failed to OpenInputStream for %s\n", url);
        return NS_OK;
    }

    char buf[1024];
    while (1) {
        PRUint32 amt;
        rv = in->Read(buf, sizeof(buf), &amt);
        if (NS_FAILED(rv)) return rv;
        if (amt == 0) break;    // eof
        
        char* str = buf;
        while (amt-- > 0) {
            fputc(*str++, stdout);
        }
    }
    nsCOMPtr<nsIURI> uri;
    char* str;

    rv = channel->GetOriginalURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;
    rv = uri->GetSpec(&str);
    if (NS_FAILED(rv)) return rv;
    fprintf(stdout, "%s resolved to ", str);
    nsCRT::free(str);

    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;
    rv = uri->GetSpec(&str);
    if (NS_FAILED(rv)) return rv;
    fprintf(stdout, "%s\n", str);
    nsCRT::free(str);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

PRBool gDone = PR_FALSE;
nsIEventQueue* gEventQ = nsnull;

class Listener : public nsIStreamListener 
{
public:
    NS_DECL_ISUPPORTS

    Listener() { NS_INIT_REFCNT(); }
    virtual ~Listener() {}

    NS_IMETHOD OnStartRequest(nsIRequest *request, nsISupports *ctxt) {
        nsresult rv;
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        
        rv = channel->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv)) {
            char* str;
            rv = uri->GetSpec(&str);
            if (NS_SUCCEEDED(rv)) {
                fprintf(stdout, "Starting to load %s\n", str);
                nsCRT::free(str);
            }
        }
        return NS_OK;
    }
    
    NS_IMETHOD OnStopRequest(nsIRequest *request, nsISupports *ctxt, 
                             nsresult aStatus) {
        nsresult rv;
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        
        rv = channel->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv)) {
            char* str;
            rv = uri->GetSpec(&str);
            if (NS_SUCCEEDED(rv)) {
                fprintf(stdout, "Ending load %s, status=%x\n", str, aStatus);
                nsCRT::free(str);
            }
        }
        gDone = PR_TRUE;
        return NS_OK;
    }

    NS_IMETHOD OnDataAvailable(nsIRequest *request, nsISupports *ctxt, 
                               nsIInputStream *inStr,
                               PRUint32 sourceOffset, PRUint32 count) {
        nsresult rv;
        char buf[1024];
        while (count > 0) {
            PRUint32 amt;
            rv = inStr->Read(buf, sizeof(buf), &amt);
            count -= amt;
            char* c = buf;
            while (amt-- > 0) {
                fputc(*c++, stdout);
            }
        }
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS2(Listener, nsIStreamListener, nsIRequestObserver)

nsresult
TestAsyncRead(const char* url)
{
    nsresult rv;

    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = serv->NewChannel(url,
                          nsnull, // base uri
                          getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListener> listener = new Listener();
    if (listener == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = channel->AsyncOpen(nsnull, listener);
    if (NS_FAILED(rv)) return rv;

    while (!gDone) {
        PLEvent* event;
        rv = gEventQ->GetEvent(&event);
        if (NS_FAILED(rv)) return rv;
        rv = gEventQ->HandleEvent(event);
        if (NS_FAILED(rv)) return rv;
    }

    return rv;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
NS_AutoregisterComponents()
{
    nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                                   NULL /* default */);
    return rv;
}

int
main(int argc, char* argv[])
{
    nsresult rv;

    rv = NS_AutoregisterComponents();
    NS_ASSERTION(NS_SUCCEEDED(rv), "AutoregisterComponents failed");

    if (argc < 2) {
        printf("usage: %s resource://foo/<path-to-resolve>\n", argv[0]);
        return -1;
    }

    rv = SetupMapping();
    NS_ASSERTION(NS_SUCCEEDED(rv), "SetupMapping failed");
    if (NS_FAILED(rv)) return rv;

    rv = TestOpenInputStream(argv[1]);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestOpenInputStream failed");

    rv = TestAsyncRead(argv[1]);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncRead failed");

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
