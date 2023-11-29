/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSDIRINDEX_H_
#define __NSDIRINDEX_H_

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIDirIndexListener.h"
#include "mozilla/RefPtr.h"

class nsIDirIndex;
class nsITextToSubURI;

/* CID: {a0d6ad32-1dd1-11b2-aa55-a40187b54036} */

class nsDirIndexParser : public nsIDirIndexParser {
 private:
  virtual ~nsDirIndexParser() = default;

  nsDirIndexParser() = default;
  void Init();

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIDIRINDEXPARSER

  static already_AddRefed<nsIDirIndexParser> CreateInstance() {
    RefPtr<nsDirIndexParser> parser = new nsDirIndexParser();
    parser->Init();
    return parser.forget();
  }

  enum fieldType {
    FIELD_UNKNOWN = 0,  // MUST be 0
    FIELD_FILENAME,
    FIELD_CONTENTLENGTH,
    FIELD_LASTMODIFIED,
    FIELD_FILETYPE
  };

 protected:
  nsCOMPtr<nsIDirIndexListener> mListener;

  nsCString mBuf;
  int32_t mLineStart{0};
  int mFormat[8]{-1};

  nsresult ProcessData(nsIRequest* aRequest);
  void ParseFormat(const char* aFormatStr);
  void ParseData(nsIDirIndex* aIdx, char* aDataStr, int32_t lineLen);

  struct Field {
    const char* mName;
    fieldType mType;
  };

  static Field gFieldTable[];
};

#endif
