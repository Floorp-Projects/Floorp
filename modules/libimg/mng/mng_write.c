/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : mng_write.c               copyright (c) 2000 G.Juyn        * */
/* * version   : 0.9.0                                                      * */
/* *                                                                        * */
/* * purpose   : Write management (implementation)                          * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the write management routines            * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *             0.5.1 - 05/16/2000 - G.Juyn                                * */
/* *             - moved the actual write_graphic functionality from        * */
/* *               mng_hlapi to it's appropriate function here             * */
/* *                                                                        * */
/* ************************************************************************** */

#include "libmng.h"
#include "mng_data.h"
#include "mng_error.h"
#include "mng_trace.h"
#ifdef __BORLANDC__
#pragma hdrstop
#endif
#include "mng_memory.h"
#include "mng_chunks.h"
#include "mng_chunk_io.h"
#include "mng_write.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */

#ifdef MNG_INCLUDE_WRITE_PROCS

/* ************************************************************************** */

mng_retcode write_graphic (mng_datap pData)
{
  mng_chunkp  pChunk;
  mng_retcode iRetcode;
  mng_uint32  iWritten;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_GRAPHIC, MNG_LC_START)
#endif

  pChunk = pData->pFirstchunk;         /* we'll start with the first, thank you */

  if (pChunk)                          /* is there anything to write ? */
  {                                    /* open the file */
    if (!pData->fOpenstream ((mng_handle)pData))
      MNG_ERROR (pData, MNG_APPIOERROR)
    else
    {
      pData->bWriting      = MNG_TRUE; /* indicate writing */
      pData->iWritebufsize = 32768;    /* get a temporary write buffer */
                                       /* reserve 12 bytes for length, chunkname & crc */
      MNG_ALLOC (pData, pData->pWritebuf, pData->iWritebufsize+12)
                                       /* write the signature */
      if (((mng_chunk_headerp)pChunk)->iChunkname == MNG_UINT_IHDR)
        *(mng_uint32p)pData->pWritebuf = PNG_SIG;
      else
      if (((mng_chunk_headerp)pChunk)->iChunkname == MNG_UINT_JHDR)
        *(mng_uint32p)pData->pWritebuf = JNG_SIG;
      else
        *(mng_uint32p)pData->pWritebuf = MNG_SIG;

      *(mng_uint32p)(pData->pWritebuf+4) = POST_SIG;

      if (!pData->fWritedata ((mng_handle)pData, pData->pWritebuf, 8, &iWritten))
      {
        MNG_FREE (pData, pData->pWritebuf, pData->iWritebufsize+12)
        MNG_ERROR (pData, MNG_APPIOERROR)
      }

      if (iWritten != 8)               /* disk full ? */
      {
        MNG_FREE (pData, pData->pWritebuf, pData->iWritebufsize+12)
        MNG_ERROR (pData, MNG_OUTPUTERROR)
      }

      while (pChunk)                   /* so long as there's something to write */
      {                                /* let's call it's output routine */
        iRetcode = ((mng_chunk_headerp)pChunk)->fWrite (pData, pChunk);

        if (iRetcode)                  /* on error bail out */
        {
          MNG_FREE (pData, pData->pWritebuf, pData->iWritebufsize+12)
          return iRetcode;
        }
                                       /* neeeext */
        pChunk = ((mng_chunk_headerp)pChunk)->pNext;
      }
                                       /* free the temporary buffer */
      MNG_FREE (pData, pData->pWritebuf, pData->iWritebufsize+12)

      pData->bWriting = MNG_FALSE;     /* done writing */
                                       /* close the stream now */
      if (!pData->fClosestream ((mng_handle)pData))
        MNG_ERROR (pData, MNG_APPIOERROR)

    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_GRAPHIC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_WRITE_PROCS */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */


