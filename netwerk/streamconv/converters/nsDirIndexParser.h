/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-2001 Netscape Communications Corporation.
 * All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson           <waterson@netscape.com>
 *   Robert John Churchill    <rjc@netscape.com>
 *   Pierre Phaneuf           <pp@ludusdesign.com>
 *   Bradley Baetz            <bbaetz@cs.mcgill.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
    PRInt32      mLineStart;
    PRBool       mHasDescription;
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
