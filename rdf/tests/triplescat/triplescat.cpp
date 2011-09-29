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
 *   Axel Hecht <axel@pike.org>
 *   Chase Tingley <tingley@sundell.net>
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
 * A simple test program that reads in RDF/XML into an in-memory data
 * source, then serializes NTriples format back to stdout (or none).
 *
 * The program takes a single parameter: the URL from which to read,
 * plus an optional parameter -q
 */

#include <stdio.h>
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
//#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsIIOService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsRDFCID.h"
#include "plstr.h"
#include "prio.h"
#include "prthread.h"

#include "rdf.h"
#include "rdfIDataSource.h"
#include "rdfITripleVisitor.h"
#include "rdfISerializer.h"

////////////////////////////////////////////////////////////////////////
// Blatantly stolen from netwerk/test/
#define RETURN_IF_FAILED(rv, step) \
    PR_BEGIN_MACRO \
    if (NS_FAILED(rv)) { \
        printf(">>> %s failed: rv=%x\n", step, rv); \
        return rv;\
    } \
    PR_END_MACRO

////////////////////////////////////////////////////////////////////////

class ConsoleOutputStreamImpl : public nsIOutputStream
{
public:
    ConsoleOutputStreamImpl(void) {}
    virtual ~ConsoleOutputStreamImpl(void) {}

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIOutputStream interface
    NS_IMETHOD Close(void) {
        return NS_OK;
    }

    NS_IMETHOD Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount) {
        PR_Write(PR_GetSpecialFD(PR_StandardOutput), aBuf, aCount);
        *aWriteCount = aCount;
        return NS_OK;
    }

    NS_IMETHOD
    WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("WriteFrom");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("WriteSegments");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    IsNonBlocking(bool *aNonBlocking) {
        NS_NOTREACHED("IsNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Flush(void) {
        PR_Sync(PR_GetSpecialFD(PR_StandardOutput));
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS1(ConsoleOutputStreamImpl, nsIOutputStream)

////////////////////////////////////////////////////////////////////////

int
main(int argc, char** argv)
{
    nsresult rv;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <url>\n", argv[0]);
        return 1;
    }

    NS_InitXPCOM2(nsnull, nsnull, nsnull);

    // Create a stream data source and initialize it on argv[1], which
    // is hopefully a "file:" URL.
    nsCOMPtr<nsIRDFDataSource> ds =
        do_CreateInstance(NS_RDF_DATASOURCE_CONTRACTID_PREFIX "xml-datasource",
                          &rv);
    RETURN_IF_FAILED(rv, "RDF/XML datasource creation");

    nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(ds, &rv);
    RETURN_IF_FAILED(rv, "QI to nsIRDFRemoteDataSource");

    rv = remote->Init(argv[1]);
    RETURN_IF_FAILED(rv, "datasource initialization");

    // Okay, this should load the XML file...
    rv = remote->Refresh(PR_FALSE);
    RETURN_IF_FAILED(rv, "datasource refresh");

    // Pump events until the load is finished
    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
    bool done = false;
    while (!done) {
        NS_ENSURE_STATE(NS_ProcessNextEvent(thread));
        remote->GetLoaded(&done);
    }

    nsCOMPtr<rdfIDataSource> rdfds = do_QueryInterface(ds, &rv);
    RETURN_IF_FAILED(rv, "QI to rdIDataSource");
    {
        nsCOMPtr<nsIOutputStream> out = new ConsoleOutputStreamImpl();
        nsCOMPtr<rdfISerializer> ser =
            do_CreateInstance(NS_RDF_SERIALIZER "ntriples", &rv);
        RETURN_IF_FAILED(rv, "Creation of NTriples Serializer");
        rv = ser->Serialize(rdfds, out);
        RETURN_IF_FAILED(rv, "Serialization to NTriples");
        out->Close();
    }

    return NS_OK;
}
