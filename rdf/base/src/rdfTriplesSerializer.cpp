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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <axel@pike.org>
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

#include "nsIOutputStream.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsIBufferedStreams.h"
#include "nsNetCID.h"
#include "nsComponentManagerUtils.h"

#include "rdfISerializer.h"
#include "rdfIDataSource.h"
#include "rdfITripleVisitor.h"

#include "nsIRDFResource.h"
#include "nsIRDFLiteral.h"

class TriplesVisitor : public rdfITripleVisitor
{
public:
    TriplesVisitor(nsIOutputStream* aOut) : mOut(aOut) {}
    NS_DECL_RDFITRIPLEVISITOR
    NS_DECL_ISUPPORTS
protected:
    nsresult writeResource(nsIRDFResource* aResource);
    nsIOutputStream* mOut;
};

NS_IMPL_ISUPPORTS1(TriplesVisitor, rdfITripleVisitor)

nsresult
TriplesVisitor::writeResource(nsIRDFResource *aResource)
{
    nsCString res;
    PRUint32 writeCount, wroteCount;
    mOut->Write("<", 1, &wroteCount);
    NS_ENSURE_TRUE(wroteCount == 1, NS_ERROR_FAILURE);
    nsresult rv = aResource->GetValueUTF8(res);
    NS_ENSURE_SUCCESS(rv, rv);
    writeCount = res.Length();
    mOut->Write(res.get(), writeCount, &wroteCount);
    NS_ENSURE_TRUE(writeCount == wroteCount, NS_ERROR_FAILURE);
    mOut->Write("> ", 2, &wroteCount);
    NS_ENSURE_TRUE(wroteCount == 2, NS_ERROR_FAILURE);
    return NS_OK;
}

NS_IMETHODIMP
TriplesVisitor::Visit(nsIRDFNode *aSubject, nsIRDFResource *aPredicate,
                      nsIRDFNode *aObject, bool aTruthValue)
{
    nsCOMPtr<nsIRDFResource> subjectRes = do_QueryInterface(aSubject);
    nsresult rv = NS_OK;
    if (subjectRes) {
        rv = writeResource(subjectRes);
    }
    if (NS_FAILED(rv)) {
        return rv;
    }
    rv = writeResource(aPredicate);
    if (NS_FAILED(rv)) {
        return rv;
    }
    nsCOMPtr<nsIRDFResource> res = do_QueryInterface(aObject);
    nsCOMPtr<nsIRDFLiteral> lit;
    nsCOMPtr<nsIRDFInt> intLit;
    PRUint32 wroteCount;
    if (res) {
        rv = writeResource(res);
    } else if ((lit = do_QueryInterface(aObject)) != nsnull) {
        const PRUnichar *value;
        lit->GetValueConst(&value);
        nsCAutoString object;
        object.AppendLiteral("\"");
        AppendUTF16toUTF8(value, object);
        object.AppendLiteral("\" ");
        PRUint32 writeCount = object.Length();
        rv = mOut->Write(object.get(), writeCount, &wroteCount);
        NS_ENSURE_TRUE(writeCount == wroteCount, NS_ERROR_FAILURE);
    } else if ((intLit = do_QueryInterface(aObject)) != nsnull) {
        PRInt32 value;
        intLit->GetValue(&value);
        nsPrintfCString
            object(128,
                   "\"%i\"^^<http://www.w3.org/2001/XMLSchema#integer> ",
                   value);
        PRUint32 writeCount = object.Length();
        rv = mOut->Write(object.get(), writeCount, &wroteCount);
        NS_ENSURE_TRUE(writeCount == wroteCount, NS_ERROR_FAILURE);
    }
    NS_ENSURE_SUCCESS(rv, rv);
    return mOut->Write(".\n", 2, &wroteCount);
}

class rdfTriplesSerializer : public rdfISerializer
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_RDFISERIALIZER

  rdfTriplesSerializer();

private:
  ~rdfTriplesSerializer();

};

nsresult
NS_NewTriplesSerializer(rdfISerializer** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = new rdfTriplesSerializer();
    if (! *aResult)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(rdfTriplesSerializer, rdfISerializer)

rdfTriplesSerializer::rdfTriplesSerializer()
{
}

rdfTriplesSerializer::~rdfTriplesSerializer()
{
}

NS_IMETHODIMP
rdfTriplesSerializer::Serialize(rdfIDataSource *aDataSource,
                                nsIOutputStream *aOut)
{
    nsresult rv;
    nsCOMPtr<nsIBufferedOutputStream> bufout = 
        do_CreateInstance(NS_BUFFEREDOUTPUTSTREAM_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = bufout->Init(aOut, 1024);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<rdfITripleVisitor> tv = new TriplesVisitor(bufout);
    NS_ENSURE_TRUE(tv, NS_ERROR_OUT_OF_MEMORY);
    return aDataSource->VisitAllTriples(tv);
}
