/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mozilla/Attributes.h"

class TriplesVisitor MOZ_FINAL : public rdfITripleVisitor
{
public:
    explicit TriplesVisitor(nsIOutputStream* aOut) : mOut(aOut) {}
    NS_DECL_RDFITRIPLEVISITOR
    NS_DECL_ISUPPORTS
protected:
    ~TriplesVisitor() {}
    nsresult writeResource(nsIRDFResource* aResource);
    nsIOutputStream* mOut;
};

NS_IMPL_ISUPPORTS(TriplesVisitor, rdfITripleVisitor)

nsresult
TriplesVisitor::writeResource(nsIRDFResource *aResource)
{
    nsCString res;
    uint32_t writeCount, wroteCount;
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
    uint32_t wroteCount;
    if (res) {
        rv = writeResource(res);
    } else if ((lit = do_QueryInterface(aObject)) != nullptr) {
        const char16_t *value;
        lit->GetValueConst(&value);
        nsAutoCString object;
        object.Append('"');
        AppendUTF16toUTF8(value, object);
        object.AppendLiteral("\" ");
        uint32_t writeCount = object.Length();
        rv = mOut->Write(object.get(), writeCount, &wroteCount);
        NS_ENSURE_TRUE(writeCount == wroteCount, NS_ERROR_FAILURE);
    } else if ((intLit = do_QueryInterface(aObject)) != nullptr) {
        int32_t value;
        intLit->GetValue(&value);
        nsPrintfCString
            object("\"%i\"^^<http://www.w3.org/2001/XMLSchema#integer> ",
                   value);
        uint32_t writeCount = object.Length();
        rv = mOut->Write(object.get(), writeCount, &wroteCount);
        NS_ENSURE_TRUE(writeCount == wroteCount, NS_ERROR_FAILURE);
    }
    NS_ENSURE_SUCCESS(rv, rv);
    return mOut->Write(".\n", 2, &wroteCount);
}

class rdfTriplesSerializer MOZ_FINAL : public rdfISerializer
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
    NS_PRECONDITION(aResult != nullptr, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = new rdfTriplesSerializer();
    if (! *aResult)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS(rdfTriplesSerializer, rdfISerializer)

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
