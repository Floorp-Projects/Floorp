/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
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
 * The Original Code is the XPT zip reader.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  John Bandhauer <jband@netscape.com>
 *  Alec Flett <alecf@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsISupports.h"
#include "nsIXPTLoader.h"

#include "nsIZipReader.h"

// {0320E073-79C7-4dae-8055-81BED8B8DB96}
#define NS_XPTZIPREADER_CID \
  { 0x320e073, 0x79c7, 0x4dae, \
      { 0x80, 0x55, 0x81, 0xbe, 0xd8, 0xb8, 0xdb, 0x96 } }


class nsXPTZipLoader : public nsIXPTLoader
{
 public:
    nsXPTZipLoader();
    virtual ~nsXPTZipLoader() {};
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPTLOADER

 private:
    nsIZipReader* GetZipReader(nsILocalFile* aFile);
    nsCOMPtr<nsIZipReaderCache> mCache;
};

