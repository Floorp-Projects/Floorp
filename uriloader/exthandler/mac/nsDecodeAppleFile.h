/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDecodeAppleFile_h__
#define nsDecodeAppleFile_h__

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIOutputStream.h"

/*
** applefile definitions used
*/
#if PRAGMA_STRUCT_ALIGN
#  pragma options align = mac68k
#endif

#define APPLESINGLE_MAGIC 0x00051600L
#define APPLEDOUBLE_MAGIC 0x00051607L
#define VERSION 0x00020000

#define NUM_ENTRIES 6

#define ENT_DFORK 1L
#define ENT_RFORK 2L
#define ENT_NAME 3L
#define ENT_COMMENT 4L
#define ENT_DATES 8L
#define ENT_FINFO 9L

#define CONVERT_TIME 1265437696L

/*
** data type used in the header decoder.
*/
typedef struct ap_header {
  int32_t magic;
  int32_t version;
  int32_t fill[4];
  int16_t entriesCount;

} ap_header;

typedef struct ap_entry {
  int32_t id;
  int32_t offset;
  int32_t length;

} ap_entry;

typedef struct ap_dates {
  int32_t create, modify, backup, access;

} ap_dates;

#if PRAGMA_STRUCT_ALIGN
#  pragma options align = reset
#endif

/*
**Error codes
*/
enum { errADNotEnoughData = -12099, errADNotSupported, errADBadVersion };

class nsDecodeAppleFile : public nsIOutputStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM

  nsDecodeAppleFile();
  virtual ~nsDecodeAppleFile();

  [[nodiscard]] nsresult Initialize(nsIOutputStream* output, nsIFile* file);

 private:
#define MAX_BUFFERSIZE 1024
  enum ParserState {
    parseHeaders,
    parseEntries,
    parseLookupPart,
    parsePart,
    parseSkipPart,
    parseDataFork,
    parseResourceFork,
    parseWriteThrough
  };

  nsCOMPtr<nsIOutputStream> m_output;
  FSSpec m_fsFileSpec;
  SInt16 m_rfRefNum;

  unsigned char* m_dataBuffer;
  int32_t m_dataBufferLength;
  ParserState m_state;
  ap_header m_headers;
  ap_entry* m_entries;
  int32_t m_offset;
  int32_t m_dataForkOffset;
  int32_t m_totalDataForkWritten;
  int32_t m_totalResourceForkWritten;
  bool m_headerOk;

  int32_t m_currentPartID;
  int32_t m_currentPartLength;
  int32_t m_currentPartCount;

  Str255 m_comment;
  ap_dates m_dates;
  FInfo m_finderInfo;
  FXInfo m_finderExtraInfo;
};

#endif
