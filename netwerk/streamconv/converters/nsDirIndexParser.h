/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSDIRINDEX_H_
#define __NSDIRINDEX_H_

#include "nsIDirIndex.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIDirIndexListener.h"
#include "nsITextToSubURI.h"

/* CID: {a0d6ad32-1dd1-11b2-aa55-a40187b54036} */

class nsDirIndexParser : public nsIDirIndexParser {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIDIRINDEXPARSER
    
    nsDirIndexParser();
    virtual ~nsDirIndexParser();
    nsresult Init();

    enum fieldType {
        FIELD_UNKNOWN = 0, // MUST be 0
        FIELD_FILENAME,
        FIELD_DESCRIPTION,
        FIELD_CONTENTLENGTH,
        FIELD_LASTMODIFIED,
        FIELD_CONTENTTYPE,
        FIELD_FILETYPE
    };

protected:
    nsCOMPtr<nsIDirIndexListener> mListener;

    nsCString    mEncoding;
    nsCString    mComment;
    nsCString    mBuf;
    int32_t      mLineStart;
    bool         mHasDescription;
    int*         mFormat;

    nsresult ProcessData(nsIRequest *aRequest, nsISupports *aCtxt);
    nsresult ParseFormat(const char* buf);
    nsresult ParseData(nsIDirIndex* aIdx, char* aDataStr);

    struct Field {
        const char *mName;
        fieldType mType;
    };

    static Field gFieldTable[];

    static nsrefcnt gRefCntParser;
    static nsITextToSubURI* gTextToSubURI;
};

#endif
