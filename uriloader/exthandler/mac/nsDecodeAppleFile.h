/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDecodeAppleFile_h__
#define nsDecodeAppleFile_h__

#include "nsCom.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsILocalFileMac.h"
#include "nsIOutputStream.h"

/*
** applefile definitions used 
*/
#if PRAGMA_ALIGN_SUPPORTED
  #pragma options align=mac68k
#endif

#define APPLESINGLE_MAGIC   0x00051600L
#define APPLEDOUBLE_MAGIC 	0x00051607L
#define VERSION             0x00020000

#define NUM_ENTRIES 		6

#define ENT_DFORK   		1L
#define ENT_RFORK   		2L
#define ENT_NAME    		3L
#define ENT_COMMENT 		4L
#define ENT_DATES   		8L
#define ENT_FINFO   		9L

#define CONVERT_TIME 		1265437696L

/*
** data type used in the header decoder.
*/
typedef struct ap_header 
{
	PRInt32 	magic;
	PRInt32   version;
	PRInt32 	fill[4];
	PRInt16 	entriesCount;

} ap_header;

typedef struct ap_entry 
{
	PRInt32   id;
	PRInt32	  offset;
	PRInt32	  length;
	
} ap_entry;

typedef struct ap_dates 
{
	PRInt32 create, modify, backup, access;

} ap_dates;

#if PRAGMA_ALIGN_SUPPORTED
  #pragma options align=reset
#endif

/*
**Error codes
*/
enum {
  errADNotEnoughData = -12099,
  errADNotSupported,
  errADBadVersion
};


class nsAppleFileDecoder : public nsIOutputStream
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  
  nsAppleFileDecoder();
  virtual ~nsAppleFileDecoder();
  
  nsresult Initialize(nsIOutputStream *output, nsIFile *file);
  
private:
  #define MAX_BUFFERSIZE    1024
  enum ParserState {parseHeaders, parseEntries, parseLookupPart, parsePart, parseSkipPart,
                    parseDataFork, parseResourceFork, parseWriteThrough};
  
  nsCOMPtr<nsIOutputStream> m_output;
  FSSpec            m_fsFileSpec;
  SInt16            m_rfRefNum;
  
  unsigned char *   m_dataBuffer;
  PRInt32           m_dataBufferLength;
  ParserState       m_state;
  ap_header         m_headers;
  ap_entry *        m_entries;
  PRInt32           m_offset;
  PRInt32           m_dataForkOffset;
  PRInt32           m_totalDataForkWritten;
  PRInt32           m_totalResourceForkWritten;
  PRBool            m_headerOk;
  
  PRInt32           m_currentPartID;
  PRInt32           m_currentPartLength;
  PRInt32           m_currentPartCount;
  
  Str255            m_comment;
  ap_dates          m_dates;
  FInfo             m_finderInfo;
  FXInfo            m_finderExtraInfo;
};

#endif
