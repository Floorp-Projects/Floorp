/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *  Jean-Francois Ducarroz <ducarroz@netscape.com> 
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
 
#include "nsAppleFileDecoder.h"
#include "prmem.h"
#include "nsCRT.h"


NS_IMPL_THREADSAFE_ISUPPORTS2(nsAppleFileDecoder, nsIAppleFileDecoder, nsIOutputStream);

nsAppleFileDecoder::nsAppleFileDecoder()
{
  NS_INIT_ISUPPORTS();

  m_state = parseHeaders;
  m_dataBufferLength = 0;
  m_dataBuffer = (unsigned char*) PR_MALLOC(MAX_BUFFERSIZE);
  m_entries = nsnull;
  m_rfRefNum = -1;
  m_totalDataForkWritten = 0;
  m_totalResourceForkWritten = 0;
  m_headerOk = PR_FALSE;
  
  m_comment[0] = 0;
  nsCRT::zero(&m_dates, sizeof(m_dates));
  nsCRT::zero(&m_finderInfo, sizeof(m_dates));
  nsCRT::zero(&m_finderExtraInfo, sizeof(m_dates));
}

nsAppleFileDecoder::~nsAppleFileDecoder()
{
  if (m_output)
    Close();

  PR_FREEIF(m_dataBuffer);
  if (m_entries)
    delete [] m_entries;
}

NS_IMETHODIMP nsAppleFileDecoder::Initialize(nsIOutputStream *outputStream, nsIFile *outputFile)
{
  m_output = outputStream;
  
  nsCOMPtr<nsILocalFileMac> macFile = do_QueryInterface(outputFile);
  macFile->GetTargetFSSpec(&m_fsFileSpec);

  m_offset = 0;
  m_dataForkOffset = 0;

  return NS_OK;
}

NS_IMETHODIMP nsAppleFileDecoder::Close(void)
{
  nsresult rv;
  rv = m_output->Close();

  PRInt32 i;

  if (m_rfRefNum != -1)
    FSClose(m_rfRefNum);
    
  /* Check if the file is complete and if it's the case, write file attributes */
  if (m_headerOk)
  {
    PRBool dataOk = PR_TRUE; /* It's ok if the file doesn't have a datafork, therefore set it to true by default. */
    if (m_headers.magic == APPLESINGLE_MAGIC)
    {
      for (i = 0; i < m_headers.entriesCount; i ++)
        if (ENT_DFORK == m_entries[i].id)
        {
          dataOk = (PRBool)(m_totalDataForkWritten == m_entries[i].length);
          break;
        }
    }

    PRBool resourceOk = FALSE;
    for (i = 0; i < m_headers.entriesCount; i ++)
      if (ENT_RFORK == m_entries[i].id)
      {
        resourceOk = (PRBool)(m_totalResourceForkWritten == m_entries[i].length);
        break;
      }
      
    if (dataOk && resourceOk)
    {
      HFileInfo *fpb;
      CInfoPBRec cipbr;
      
      fpb = (HFileInfo *) &cipbr;
      fpb->ioVRefNum = m_fsFileSpec.vRefNum;
      fpb->ioDirID   = m_fsFileSpec.parID;
      fpb->ioNamePtr = m_fsFileSpec.name;
      fpb->ioFDirIndex = 0;
      PBGetCatInfoSync(&cipbr);

      /* set finder info */
      nsCRT::memcpy(&fpb->ioFlFndrInfo, &m_finderInfo, sizeof (FInfo));
      nsCRT::memcpy(&fpb->ioFlXFndrInfo, &m_finderExtraInfo, sizeof (FXInfo));
      fpb->ioFlFndrInfo.fdFlags &= 0xfc00; /* clear flags maintained by finder */
      
      /* set file dates */
      fpb->ioFlCrDat = m_dates.create - CONVERT_TIME;
      fpb->ioFlMdDat = m_dates.modify - CONVERT_TIME;
      fpb->ioFlBkDat = m_dates.backup - CONVERT_TIME;
    
      /* update file info */
      fpb->ioDirID = fpb->ioFlParID;
      PBSetCatInfoSync(&cipbr);
      
      /* set comment */
      IOParam vinfo;
      GetVolParmsInfoBuffer vp;
      DTPBRec dtp;

      nsCRT::zero((void *) &vinfo, sizeof (vinfo));
      vinfo.ioVRefNum = fpb->ioVRefNum;
      vinfo.ioBuffer  = (Ptr) &vp;
      vinfo.ioReqCount = sizeof (vp);
      if (PBHGetVolParmsSync((HParmBlkPtr) &vinfo) == noErr && ((vp.vMAttrib >> bHasDesktopMgr) & 1)) 
      {
        nsCRT::zero((void *) &dtp, sizeof (dtp));
        dtp.ioVRefNum = fpb->ioVRefNum;
        if (PBDTGetPath(&dtp) == noErr) 
        {
          dtp.ioDTBuffer = (Ptr) &m_comment[1];
          dtp.ioNamePtr  = fpb->ioNamePtr;
          dtp.ioDirID    = fpb->ioDirID;
          dtp.ioDTReqCount = m_comment[0];
          if (PBDTSetCommentSync(&dtp) == noErr) 
            PBDTFlushSync(&dtp);
        }
      }
    }
  }
  
  /* setting m_headerOk to false will prevent us to reprocess the header in case the Close function is called several time*/
  m_headerOk = PR_FALSE;

  return rv;
}

NS_IMETHODIMP nsAppleFileDecoder::Flush(void)
{
  return m_output->Flush();
} 

NS_IMETHODIMP nsAppleFileDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  return m_output->WriteFrom(inStr, count, _retval);
}

NS_IMETHODIMP nsAppleFileDecoder::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
  return m_output->WriteSegments(reader, closure, count, _retval);
}

NS_IMETHODIMP nsAppleFileDecoder::GetNonBlocking(PRBool *aNonBlocking)
{
  return m_output->GetNonBlocking(aNonBlocking);
}

NS_IMETHODIMP nsAppleFileDecoder::SetNonBlocking(PRBool aNonBlocking)
{
  return m_output->SetNonBlocking(aNonBlocking);
}

NS_IMETHODIMP nsAppleFileDecoder::GetObserver(nsIOutputStreamObserver * *aObserver)
{
  return m_output->GetObserver(aObserver);
}

NS_IMETHODIMP nsAppleFileDecoder::SetObserver(nsIOutputStreamObserver * aObserver)
{
  return m_output->SetObserver(aObserver);
}

NS_IMETHODIMP nsAppleFileDecoder::Write(const char *buffer, PRUint32 bufferSize, PRUint32* writeCount)
{
  /* WARNING: to simplify my life, I presume that I should get all appledouble headers in the first block,
              else I would have to implement a buffer */

  const char * buffPtr = buffer;
  PRUint32 dataCount;
  PRInt32 i;
  nsresult rv = NS_OK;

  *writeCount = 0;
  
  while (bufferSize > 0 && NS_SUCCEEDED(rv))
  {
    switch (m_state)
    {
      case parseHeaders :
        dataCount = sizeof(ap_header) - m_dataBufferLength;
        if (dataCount > bufferSize)
          dataCount = bufferSize;
        nsCRT::memcpy(&m_dataBuffer[m_dataBufferLength], buffPtr, dataCount);
        m_dataBufferLength += dataCount;
        
        if (m_dataBufferLength == sizeof(ap_header))
        {
          nsCRT::memcpy(&m_headers, m_dataBuffer, sizeof(ap_header));

          /* Check header to be sure we are dealing with the right kind of data, else just write it to the data fork. */
          if ((m_headers.magic == APPLEDOUBLE_MAGIC || m_headers.magic == APPLESINGLE_MAGIC) && 
              m_headers.version == VERSION && m_headers.entriesCount)
          {
            /* Just to be sure, the filler must contains only 0 */
            for (i = 0; i < 4 && m_headers.fill[i] == 0L; i ++)
              ;
            if (i == 4)
              m_state = parseEntries;
          }
          m_dataBufferLength = 0;
          
          if (m_state == parseHeaders)
          {
            dataCount = 0;
            m_state = parseWriteThrough;
          }
        }
        break;
      
      case parseEntries :
        if (!m_entries)
        {
          m_entries = new ap_entry[m_headers.entriesCount];
          if (!m_entries)
            return NS_ERROR_OUT_OF_MEMORY;
        }
        PRUint32 entriesSize = sizeof(ap_entry) * m_headers.entriesCount;
        dataCount = entriesSize - m_dataBufferLength;
        if (dataCount > bufferSize)
          dataCount = bufferSize;
        nsCRT::memcpy(&m_dataBuffer[m_dataBufferLength], buffPtr, dataCount);
        m_dataBufferLength += dataCount;

        if (m_dataBufferLength == entriesSize)
        {
          for (i = 0; i < m_headers.entriesCount; i ++)
          {
            nsCRT::memcpy(&m_entries[i], &m_dataBuffer[i * sizeof(ap_entry)], sizeof(ap_entry));
            if (m_headers.magic == APPLEDOUBLE_MAGIC)
            {
              PRUint32 offset = m_entries[i].offset + m_entries[i].length;
              if (offset > m_dataForkOffset)
                m_dataForkOffset = offset;
            }
          }
          m_headerOk = PR_TRUE;          
          m_state = parseLookupPart;
        }
        break;
      
      case parseLookupPart :
        /* which part are we parsing? */
        m_currentPartID = -1;
        for (i = 0; i < m_headers.entriesCount; i ++)
          if (m_offset == m_entries[i].offset && m_entries[i].length)
          {
              m_currentPartID = m_entries[i].id;
              m_currentPartLength = m_entries[i].length;
              m_currentPartCount = 0;

              switch (m_currentPartID)
              {
                case ENT_DFORK    : m_state = parseDataFork;           break;
                case ENT_RFORK    : m_state = parseResourceFork;       break;

                case ENT_COMMENT  :
                case ENT_DATES    :
                case ENT_FINFO    :
                  m_dataBufferLength = 0;
                  m_state = parsePart;
                  break;
                
                default           : m_state = parseSkipPart;           break;
              }
              break;
          }
          
        if (m_currentPartID == -1)
        {
          /* maybe is the datafork of an appledouble file? */
          if (m_offset == m_dataForkOffset)
          {
              m_currentPartID = ENT_DFORK;
              m_currentPartLength = -1;
              m_currentPartCount = 0;
              m_state = parseDataFork;
          }
          else
            dataCount = 1;
        }        
        break;
      
      case parsePart :
        dataCount = m_currentPartLength - m_dataBufferLength;
        if (dataCount > bufferSize)
          dataCount = bufferSize;
        nsCRT::memcpy(&m_dataBuffer[m_dataBufferLength], buffPtr, dataCount);
        m_dataBufferLength += dataCount;
        
        if (m_dataBufferLength == m_currentPartLength)
        {
          switch (m_currentPartID)
          {
            case ENT_COMMENT  :
              m_comment[0] = m_currentPartLength > 255 ? 255 : m_currentPartLength;
              nsCRT::memcpy(&m_comment[1], buffPtr, m_comment[0]);
              break;
            case ENT_DATES    :
              if (m_currentPartLength == sizeof(m_dates))
                nsCRT::memcpy(&m_dates, buffPtr, m_currentPartLength);
              break;
            case ENT_FINFO    :
              if (m_currentPartLength == (sizeof(m_finderInfo) + sizeof(m_finderExtraInfo)))
              {
                nsCRT::memcpy(&m_finderInfo, buffPtr, sizeof(m_finderInfo));
                nsCRT::memcpy(&m_finderExtraInfo, buffPtr + sizeof(m_finderInfo), sizeof(m_finderExtraInfo));
              }
              break;
          }
          m_state = parseLookupPart;
        }
        break;
      
      case parseSkipPart :
        dataCount = m_currentPartLength - m_currentPartCount;
        if (dataCount > bufferSize)
          dataCount = bufferSize;
        else
          m_state = parseLookupPart;
        break;
      
      case parseDataFork :
        if (m_headers.magic == APPLEDOUBLE_MAGIC)
          dataCount = bufferSize;
        else
        {
          dataCount = m_currentPartLength - m_currentPartCount;
          if (dataCount > bufferSize)
            dataCount = bufferSize;
          else
            m_state = parseLookupPart;
        }
        
        if (m_output)
        {
          PRUint32 writeCount;
          rv = m_output->Write((const char *)buffPtr, dataCount, &writeCount);
          if (dataCount != writeCount)
            rv = NS_ERROR_FAILURE;
          m_totalDataForkWritten += dataCount;
        }
        
        break;
      
      case parseResourceFork :
        dataCount = m_currentPartLength - m_currentPartCount;
        if (dataCount > bufferSize)
          dataCount = bufferSize;
        else
          m_state = parseLookupPart;
        
        if (m_rfRefNum == -1)
        {
          if (noErr != FSpOpenRF(&m_fsFileSpec, fsWrPerm, &m_rfRefNum))
            return NS_ERROR_FAILURE;
        }
        
        long count = dataCount;
        if (noErr != FSWrite(m_rfRefNum, &count, buffPtr) || count != dataCount)
            return NS_ERROR_FAILURE;
        m_totalResourceForkWritten += dataCount;
        break;
      
      case parseWriteThrough :
        dataCount = bufferSize;
        if (m_output)
        {
          PRUint32 writeCount;
          rv = m_output->Write((const char *)buffPtr, dataCount, &writeCount);
          if (dataCount != writeCount)
            rv = NS_ERROR_FAILURE;
        }
        break;
    }

    if (dataCount)
    {
      *writeCount += dataCount;
      bufferSize -= dataCount;
      buffPtr += dataCount;
      m_currentPartCount += dataCount;
      m_offset += dataCount;
      dataCount = 0;
    }
  }

  return rv;
}
