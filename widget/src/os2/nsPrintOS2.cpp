/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Weilbacher <mozilla@weilbacher.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPrintOS2.h"

#include "nsOS2Uni.h"

#include <stdlib.h>

//---------------------------------------------------------------------------
// OS/2 Printing   - was in libprint.cpp
//---------------------------------------------------------------------------
static HMODULE hmodRes;

#define SHIFT_PTR(ptr,offset) ( *((LONG*)&ptr) += offset )


class PRTQUEUE
{
public:
   PRTQUEUE (const PRQINFO3* pPQI3)  { InitWithPQI3 (pPQI3); }
   PRTQUEUE (const PRTQUEUE& PQInfo);
  ~PRTQUEUE (void) { free (mpPQI3); }

   PRQINFO3& PQI3 () const { return *mpPQI3; }
   const char* DriverName () const { return mDriverName; }
   const char* DeviceName () const { return mDeviceName; }
   const char* PrinterName() const { return mPrinterName; }
   const char* QueueName  () const { return mpPQI3->pszComment; }
   
private:
   PRTQUEUE& operator = (const PRTQUEUE& z);        // prevent copying
   void InitWithPQI3 (const PRQINFO3* pInfo);

   PRQINFO3* mpPQI3;
   unsigned  mPQI3BufSize;
   char mDriverName  [DRIV_NAME_SIZE + 1];          // Driver name
   char mDeviceName  [DRIV_DEVICENAME_SIZE + 1];    // Device name
   char mPrinterName [PRINTERNAME_SIZE + 1];        // Printer name
};


PRTQUEUE::PRTQUEUE (const PRTQUEUE& PQInfo)
{
   mPQI3BufSize = PQInfo.mPQI3BufSize;
   mpPQI3 = (PRQINFO3*)malloc (mPQI3BufSize);
   memcpy (mpPQI3, PQInfo.mpPQI3, mPQI3BufSize);    // Copy entire buffer

   long Diff = (long)mpPQI3 - (long)PQInfo.mpPQI3;  // Calculate the difference between addresses
   SHIFT_PTR (mpPQI3->pszName,       Diff);         // Modify internal pointers accordingly
   SHIFT_PTR (mpPQI3->pszSepFile,    Diff);
   SHIFT_PTR (mpPQI3->pszPrProc,     Diff);
   SHIFT_PTR (mpPQI3->pszParms,      Diff);
   SHIFT_PTR (mpPQI3->pszComment,    Diff);
   SHIFT_PTR (mpPQI3->pszPrinters,   Diff);
   SHIFT_PTR (mpPQI3->pszDriverName, Diff);
   SHIFT_PTR (mpPQI3->pDriverData,   Diff);

   strcpy (mDriverName, PQInfo.mDriverName);
   strcpy (mDeviceName, PQInfo.mDeviceName);
   strcpy (mPrinterName, PQInfo.mPrinterName);
}

void PRTQUEUE::InitWithPQI3(const PRQINFO3* pInfo)
{
   // Make local copy of PPRQINFO3 object
   ULONG SizeNeeded;
   ::SplQueryQueue (NULL, pInfo->pszName, 3, NULL, 0, &SizeNeeded);
   mpPQI3 = (PRQINFO3*)malloc (SizeNeeded);
   ::SplQueryQueue (NULL, pInfo->pszName, 3, mpPQI3, SizeNeeded, &SizeNeeded);

   mPQI3BufSize = SizeNeeded;

   PCHAR sep = strchr (pInfo->pszDriverName, '.');

   if (sep)
   {
      *sep = '\0';
      strcpy (mDriverName, pInfo->pszDriverName);
      strcpy (mDeviceName, sep + 1);
      *sep = '.';
   } else
   {
      strcpy (mDriverName, pInfo->pszDriverName);
      mDeviceName [0] = '\0';
   }


   sep = strchr (pInfo->pszPrinters, ',');

   if (sep)
   {
      *sep = '\0';
      strcpy (mPrinterName, pInfo->pszPrinters);
      *sep = '.';
   } else
   {
      strcpy (mPrinterName, pInfo->pszPrinters);
   }
}


//===========================================================================

PRINTDLG::PRINTDLG()
{
  mQueueCount = 0;

  ULONG TotalQueues = 0;
  ULONG MemNeeded = 0;
  SPLERR rc;
  
  rc = ::SplEnumQueue(NULL, 3, NULL, 0, &mQueueCount, &TotalQueues, &MemNeeded, NULL);
  PRQINFO3* pPQI3Buf = (PRQINFO3*) malloc (MemNeeded);
  rc = ::SplEnumQueue(NULL, 3, pPQI3Buf, MemNeeded, &mQueueCount, &TotalQueues, &MemNeeded, NULL);

  if (mQueueCount > MAX_PRINT_QUEUES)
    mQueueCount = MAX_PRINT_QUEUES;

  ULONG defaultQueue = 0;
  for (ULONG cnt = 0; cnt < mQueueCount; cnt++) {
    if (pPQI3Buf[cnt].fsType & PRQ3_TYPE_APPDEFAULT)
      defaultQueue = cnt;
    mPQBuf[cnt] = new PRTQUEUE(&pPQI3Buf[cnt]);
  }

  // move the entry for the default printer to index 0 (if necessary)
  if (defaultQueue > 0) {
    PRTQUEUE* temp = mPQBuf[0];
    mPQBuf[0] = mPQBuf[defaultQueue];
    mPQBuf[defaultQueue] = temp;
  }

  free(pPQI3Buf);
}

PRINTDLG::~PRINTDLG()
{
  for (ULONG index = 0; index < mQueueCount; index++)
    delete mPQBuf[index];
}

void PRINTDLG::RefreshPrintQueue()
{
  ULONG newQueueCount = 0;
  ULONG TotalQueues = 0;
  ULONG MemNeeded = 0;
  SPLERR rc;
  
  rc = ::SplEnumQueue(NULL, 3, NULL, 0, &newQueueCount, &TotalQueues, &MemNeeded, NULL);
  PRQINFO3* pPQI3Buf = (PRQINFO3*)malloc(MemNeeded);
  rc = ::SplEnumQueue(NULL, 3, pPQI3Buf, MemNeeded, &newQueueCount, &TotalQueues, &MemNeeded, NULL);

  if (newQueueCount > MAX_PRINT_QUEUES)
    newQueueCount = MAX_PRINT_QUEUES;

  PRTQUEUE* tmpBuf[MAX_PRINT_QUEUES];

  ULONG defaultQueue = 0;
  for (ULONG cnt = 0; cnt < newQueueCount; cnt++) {
    if (pPQI3Buf[cnt].fsType & PRQ3_TYPE_APPDEFAULT)
      defaultQueue = cnt;

    BOOL found = FALSE;
    for (ULONG index = 0; index < mQueueCount && !found; index++) {
       //Compare printer from requeried list with what's already in Mozilla's printer list(mPQBuf)
       //If printer is already there, use current properties; otherwise create a new printer in list
       if (mPQBuf[index] != 0) {
         if ((strcmp(pPQI3Buf[cnt].pszPrinters, mPQBuf[index]->PrinterName()) == 0) && 
             (strcmp(pPQI3Buf[cnt].pszDriverName, mPQBuf[index]->PQI3().pszDriverName) == 0)) {
           found = TRUE;
           tmpBuf[cnt] = mPQBuf[index];
           mPQBuf[index] = 0;
         }
       }
    }
    if (!found) 
       tmpBuf[cnt] = new PRTQUEUE(&pPQI3Buf[cnt]); 
  }

  for (ULONG index = 0; index < newQueueCount; index++) {
    if (mPQBuf[index] != 0)
      delete(mPQBuf[index]);
    mPQBuf[index] = tmpBuf[index];
  }

  if (mQueueCount > newQueueCount)
    for (ULONG index = newQueueCount; index < mQueueCount; index++)
       if (mPQBuf[index] != 0)
         delete(mPQBuf[index]);

  mQueueCount = newQueueCount;

  // move the entry for the default printer to index 0 (if necessary)
  if (defaultQueue > 0) {
    PRTQUEUE* temp = mPQBuf[0];
    mPQBuf[0] = mPQBuf[defaultQueue];
    mPQBuf[defaultQueue] = temp;
  }

  free(pPQI3Buf);
}

ULONG PRINTDLG::GetNumPrinters()
{
   return mQueueCount;
}

void PRINTDLG::GetPrinter(ULONG printerNdx, char** printerName)
{
   if (printerNdx >= mQueueCount)
      return;
 
   nsCAutoString pName(mPQBuf[printerNdx]->QueueName());
 
   pName.ReplaceChar('\r', ' ');
   pName.StripChars("\n");
   *printerName = ToNewCString(pName);
}

PRTQUEUE* PRINTDLG::SetPrinterQueue(ULONG printerNdx)
{
   PRTQUEUE *pPQ = NULL;

   if (printerNdx >= mQueueCount)
      return NULL;

   pPQ = mPQBuf[printerNdx];

   return new PRTQUEUE(*pPQ);
}

LONG PRINTDLG::GetPrintDriverSize(ULONG printerNdx)
{
   return mPQBuf[printerNdx]->PQI3().pDriverData->cb;
}

PDRIVDATA PRINTDLG::GetPrintDriver(ULONG printerNdx)
{
   if (printerNdx >= mQueueCount)
      return NULL;

   return mPQBuf[printerNdx]->PQI3().pDriverData;
}

HDC PRINTDLG::GetDCHandle(ULONG printerNdx)
{
    HDC hdc = 0;
    DEVOPENSTRUC dop;

    dop.pszLogAddress      = 0; 
    dop.pszDriverName      = (char *)mPQBuf[printerNdx]->DriverName();
    dop.pdriv              = mPQBuf[printerNdx]->PQI3().pDriverData;
    dop.pszDataType        = 0; 
    dop.pszComment         = 0;
    dop.pszQueueProcName   = 0;     
    dop.pszQueueProcParams = 0;   
    dop.pszSpoolerParams   = 0;     
    dop.pszNetworkParams   = 0;     

    hdc = ::DevOpenDC(0, OD_INFO, "*", 9, (PDEVOPENDATA) &dop, NULLHANDLE);
    return hdc;
}

char* PRINTDLG::GetDriverType(ULONG printerNdx)
{
  return (char *)mPQBuf[printerNdx]->DriverName ();
}

BOOL PRINTDLG::ShowProperties(ULONG printerNdx)
{
    BOOL          rc = FALSE;
    LONG          devrc = FALSE;
    PDRIVDATA     pOldDrivData;
    PDRIVDATA     pNewDrivData = NULL;
    LONG          buflen;

/* check size of buffer required for job properties */
    buflen = DevPostDeviceModes( 0 /*hab*/,
                                 NULL,
                                 mPQBuf[printerNdx]->DriverName (),
                                 mPQBuf[printerNdx]->DeviceName (),
                                 mPQBuf[printerNdx]->PrinterName (),
                                 DPDM_POSTJOBPROP);

/* return error to caller */
    if (buflen <= 0)
        return(buflen);

/* allocate some memory for larger job properties and */
/* return error to caller */

    if (buflen != mPQBuf[printerNdx]->PQI3().pDriverData->cb)
    {
        if (DosAllocMem((PPVOID)&pNewDrivData,buflen,fALLOC))
            return(FALSE); // DPDM_ERROR
    
/* copy over old data so driver can use old job */
/* properties as base for job properties dialog */
        pOldDrivData = mPQBuf[printerNdx]->PQI3().pDriverData;
        mPQBuf[printerNdx]->PQI3().pDriverData = pNewDrivData;
        memcpy( (PSZ)pNewDrivData, (PSZ)pOldDrivData, pOldDrivData->cb );
    }

/* display job properties dialog and get updated */
/* job properties from driver */

    devrc = DevPostDeviceModes( 0 /*hab*/,
                                mPQBuf[printerNdx]->PQI3().pDriverData,
                                mPQBuf[printerNdx]->DriverName (),
                                mPQBuf[printerNdx]->DeviceName (),
                                mPQBuf[printerNdx]->PrinterName (),
                                DPDM_POSTJOBPROP);
    rc = (devrc != DPDM_ERROR);
    return rc;
}

/****************************************************************************/
/*  Job management                                                          */
/****************************************************************************/

HDC PrnOpenDC( PRTQUEUE *pInfo, PSZ pszApplicationName, int copies, int destination, char *file )
{
   HDC hdc = 0;
   PSZ pszLogAddress;
   PSZ pszDataType;
   LONG dcType;
   DEVOPENSTRUC dop;

   if (!pInfo || !pszApplicationName)
      return hdc;

   if ( destination ) {
      pszLogAddress = pInfo->PQI3 ().pszName;
      pszDataType = "PM_Q_STD";
      if ( destination == 2 )
         dcType = OD_METAFILE;
      else
         dcType = OD_QUEUED;
   } else {
      if (file && *file)
         pszLogAddress = (PSZ) file;
      else    
         pszLogAddress = "FILE";
      pszDataType = "PM_Q_RAW";
      dcType = OD_DIRECT;
   } 

   dop.pszLogAddress      = pszLogAddress; 
   dop.pszDriverName      = (char*)pInfo->DriverName ();
   dop.pdriv              = pInfo->PQI3 ().pDriverData;
   dop.pszDataType        = pszDataType; 
   dop.pszComment         = pszApplicationName;
   dop.pszQueueProcName   = pInfo->PQI3 ().pszPrProc;     
   dop.pszQueueProcParams = 0;
   dop.pszSpoolerParams   = 0;     
   dop.pszNetworkParams   = 0;     

   hdc = ::DevOpenDC( 0, dcType, "*", 9, (PDEVOPENDATA) &dop, NULLHANDLE);

#ifdef DEBUG
   if (hdc == 0)
   {
      ULONG ErrorCode = ERRORIDERROR (::WinGetLastError (0));
      printf ("!ERROR! - Can't open DC for printer %04lX\a\n", ErrorCode);
   }   
#endif

   return hdc;
}

/* find the selected form */
BOOL PrnQueryHardcopyCaps( HDC hdc, PHCINFO pHCInfo)
{
   BOOL rc = FALSE;

   if( hdc && pHCInfo)
   {
      PHCINFO pBuffer;
      long    lAvail, i;

      /* query how many forms are available */
      lAvail = ::DevQueryHardcopyCaps( hdc, 0, 0, NULL);

      pBuffer = (PHCINFO) malloc( lAvail * sizeof(HCINFO));

      ::DevQueryHardcopyCaps( hdc, 0, lAvail, pBuffer);

      for( i = 0; i < lAvail; i++)
         if( pBuffer[ i].flAttributes & HCAPS_CURRENT)
         {
            memcpy( pHCInfo, pBuffer + i, sizeof(HCINFO));
            rc = TRUE;
            break;
         }

      free( pBuffer);
   }

   return rc;
}


/****************************************************************************/
/*  Library-level data and functions    -Printing                           */
/****************************************************************************/

BOOL PrnInitialize( HMODULE hmodResources)
{
   hmodRes = hmodResources;
   return TRUE;
}

BOOL PrnTerminate()
{
   /* nop for now, may do something eventually */
   return TRUE;
}

BOOL PrnClosePrinter( PRTQUEUE *pPrintQueue)
{
   BOOL rc = FALSE;

   if (pPrintQueue)
   {
      delete pPrintQueue;
      rc = TRUE;
   }

   return rc;
}

