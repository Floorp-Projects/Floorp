/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#include "nsAppleFileDecoder.h"
#include "prmem.h"
#include "prnetdb.h"
#include "nsCRT.h"


NS_IMPL_ISUPPORTS2(nsAppleFileDecoder, nsIAppleFileDecoder, nsIOutputStream)

nsAppleFileDecoder::nsAppleFileDecoder()
{
  m_state = parseHeaders;
  m_dataBufferLength = 0;
  m_dataBuffer = (unsigned char*) PR_MALLOC(MAX_BUFFERSIZE);
  m_entries = nullptr;
  m_rfRefNum = -1;
  m_totalDataForkWritten = 0;
  m_totalResourceForkWritten = 0;
  m_headerOk = false;
  
  m_comment[0] = 0;
  memset(&m_dates, 0, sizeof(m_dates));
  memset(&m_finderInfo, 0, sizeof(m_dates));
  memset(&m_finderExtraInfo, 0, sizeof(m_dates));
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
  bool saveFollowLinks;
  macFile->GetFollowLinks(&saveFollowLinks);
  macFile->SetFollowLinks(true);
  macFile->GetFSSpec(&m_fsFileSpec);
  macFile->SetFollowLinks(saveFollowLinks);

  m_offset = 0;
  m_dataForkOffset = 0;

  return NS_OK;
}

NS_IMETHODIMP nsAppleFileDecoder::Close(void)
{
  nsresult rv;
  rv = m_output->Close();

  int32_t i;

  if (m_rfRefNum != -1)
    FSClose(m_rfRefNum);
    
  /* Check if the file is complete and if it's the case, write file attributes */
  if (m_headerOk)
  {
    bool dataOk = true; /* It's ok if the file doesn't have a datafork, therefore set it to true by default. */
    if (m_headers.magic == APPLESINGLE_MAGIC)
    {
      for (i = 0; i < m_headers.entriesCount; i ++)
        if (ENT_DFORK == m_entries[i].id)
        {
          dataOk = (bool)(m_totalDataForkWritten == m_entries[i].length);
          break;
        }
    }

    bool resourceOk = FALSE;
    for (i = 0; i < m_headers.entriesCount; i ++)
      if (ENT_RFORK == m_entries[i].id)
      {
        resourceOk = (bool)(m_totalResourceForkWritten == m_entries[i].length);
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
      memcpy(&fpb->ioFlFndrInfo, &m_finderInfo, sizeof (FInfo));
      memcpy(&fpb->ioFlXFndrInfo, &m_finderExtraInfo, sizeof (FXInfo));
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

      memset((void *) &vinfo, 0, sizeof (vinfo));
      vinfo.ioVRefNum = fpb->ioVRefNum;
      vinfo.ioBuffer  = (Ptr) &vp;
      vinfo.ioReqCount = sizeof (vp);
      if (PBHGetVolParmsSync((HParmBlkPtr) &vinfo) == noErr && ((vp.vMAttrib >> bHasDesktopMgr) & 1)) 
      {
        memset((void *) &dtp, 0, sizeof (dtp));
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
  m_headerOk = false;

  return rv;
}

NS_IMETHODIMP nsAppleFileDecoder::Flush(void)
{
  return m_output->Flush();
} 

NS_IMETHODIMP nsAppleFileDecoder::WriteFrom(nsIInputStream *inStr, uint32_t count, uint32_t *_retval)
{
  return m_output->WriteFrom(inStr, count, _retval);
}

NS_IMETHODIMP nsAppleFileDecoder::WriteSegments(nsReadSegmentFun reader, void * closure, uint32_t count, uint32_t *_retval)
{
  return m_output->WriteSegments(reader, closure, count, _retval);
}

NS_IMETHODIMP nsAppleFileDecoder::IsNonBlocking(bool *aNonBlocking)
{
  return m_output->IsNonBlocking(aNonBlocking);
}

NS_IMETHODIMP nsAppleFileDecoder::Write(const char *buffer, uint32_t bufferSize, uint32_t* writeCount)
{
  /* WARNING: to simplify my life, I presume that I should get all appledouble headers in the first block,
              else I would have to implement a buffer */

  const char * buffPtr = buffer;
  uint32_t dataCount;
  int32_t i;
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
        memcpy(&m_dataBuffer[m_dataBufferLength], buffPtr, dataCount);
        m_dataBufferLength += dataCount;
        
        if (m_dataBufferLength == sizeof(ap_header))
        {
          memcpy(&m_headers, m_dataBuffer, sizeof(ap_header));
          m_headers.magic   = (int32_t)PR_ntohl((uint32_t)m_headers.magic);
          m_headers.version = (int32_t)PR_ntohl((uint32_t)m_headers.version);
          // m_headers.fill is required to be all zeroes; no endian issues
          m_headers.entriesCount =
            (int16_t)PR_ntohs((uint16_t)m_headers.entriesCount);

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
        {
        if (!m_entries)
        {
          m_entries = new ap_entry[m_headers.entriesCount];
          if (!m_entries)
            return NS_ERROR_OUT_OF_MEMORY;
        }
        uint32_t entriesSize = sizeof(ap_entry) * m_headers.entriesCount;
        dataCount = entriesSize - m_dataBufferLength;
        if (dataCount > bufferSize)
          dataCount = bufferSize;
        memcpy(&m_dataBuffer[m_dataBufferLength], buffPtr, dataCount);
        m_dataBufferLength += dataCount;

        if (m_dataBufferLength == entriesSize)
        {
          for (i = 0; i < m_headers.entriesCount; i ++)
          {
            memcpy(&m_entries[i], &m_dataBuffer[i * sizeof(ap_entry)], sizeof(ap_entry));
            m_entries[i].id     = (int32_t)PR_ntohl((uint32_t)m_entries[i].id);
            m_entries[i].offset =
              (int32_t)PR_ntohl((uint32_t)m_entries[i].offset);
            m_entries[i].length =
              (int32_t)PR_ntohl((uint32_t)m_entries[i].length);
            if (m_headers.magic == APPLEDOUBLE_MAGIC)
            {
              uint32_t offset = m_entries[i].offset + m_entries[i].length;
              if (offset > m_dataForkOffset)
                m_dataForkOffset = offset;
            }
          }
          m_headerOk = true;          
          m_state = parseLookupPart;
        }
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
        memcpy(&m_dataBuffer[m_dataBufferLength], buffPtr, dataCount);
        m_dataBufferLength += dataCount;
        
        if (m_dataBufferLength == m_currentPartLength)
        {
          switch (m_currentPartID)
          {
            case ENT_COMMENT  :
              m_comment[0] = m_currentPartLength > 255 ? 255 : m_currentPartLength;
              memcpy(&m_comment[1], buffPtr, m_comment[0]);
              break;
            case ENT_DATES    :
              if (m_currentPartLength == sizeof(m_dates)) {
                memcpy(&m_dates, buffPtr, m_currentPartLength);
                m_dates.create = (int32_t)PR_ntohl((uint32_t)m_dates.create);
                m_dates.modify = (int32_t)PR_ntohl((uint32_t)m_dates.modify);
                m_dates.backup = (int32_t)PR_ntohl((uint32_t)m_dates.backup);
                m_dates.access = (int32_t)PR_ntohl((uint32_t)m_dates.access);
              }
              break;
            case ENT_FINFO    :
              if (m_currentPartLength == (sizeof(m_finderInfo) + sizeof(m_finderExtraInfo)))
              {
                memcpy(&m_finderInfo, buffPtr, sizeof(m_finderInfo));
                // OSType (four character codes) are still integers; swap them.
                m_finderInfo.fdType =
                  (OSType)PR_ntohl((uint32_t)m_finderInfo.fdType);
                m_finderInfo.fdCreator =
                  (OSType)PR_ntohl((uint32_t)m_finderInfo.fdCreator);
                m_finderInfo.fdFlags =
                  (UInt16)PR_ntohs((uint16_t)m_finderInfo.fdFlags);
                m_finderInfo.fdLocation.v =
                  (short)PR_ntohs((uint16_t)m_finderInfo.fdLocation.v);
                m_finderInfo.fdLocation.h =
                  (short)PR_ntohs((uint16_t)m_finderInfo.fdLocation.h);
                m_finderInfo.fdFldr =
                  (SInt16)PR_ntohs((uint16_t)m_finderInfo.fdFldr);

                memcpy(&m_finderExtraInfo, buffPtr + sizeof(m_finderInfo), sizeof(m_finderExtraInfo));
                m_finderExtraInfo.fdIconID =
                  (SInt16)PR_ntohs((uint16_t)m_finderExtraInfo.fdIconID);
                m_finderExtraInfo.fdReserved[0] =
                  (SInt16)PR_ntohs((uint16_t)m_finderExtraInfo.fdReserved[0]);
                m_finderExtraInfo.fdReserved[1] =
                  (SInt16)PR_ntohs((uint16_t)m_finderExtraInfo.fdReserved[1]);
                m_finderExtraInfo.fdReserved[2] =
                  (SInt16)PR_ntohs((uint16_t)m_finderExtraInfo.fdReserved[2]);
                // fdScript is a byte
                // fdXFlags is a byte
                m_finderExtraInfo.fdComment =
                  (SInt16)PR_ntohs((uint16_t)m_finderExtraInfo.fdComment);
                m_finderExtraInfo.fdPutAway =
                  (SInt32)PR_ntohl((uint32_t)m_finderExtraInfo.fdPutAway);
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
          uint32_t writeCount;
          rv = m_output->Write((const char *)buffPtr, dataCount, &writeCount);
          if (dataCount != writeCount)
            rv = NS_ERROR_FAILURE;
          m_totalDataForkWritten += dataCount;
        }
        
        break;
      
      case parseResourceFork :
        {
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
        }
        break;
      
      case parseWriteThrough :
        dataCount = bufferSize;
        if (m_output)
        {
          uint32_t writeCount;
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
