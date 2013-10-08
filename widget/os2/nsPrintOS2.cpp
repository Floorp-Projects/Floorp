/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
   ::SplQueryQueue (nullptr, pInfo->pszName, 3, nullptr, 0, &SizeNeeded);
   mpPQI3 = (PRQINFO3*)malloc (SizeNeeded);
   ::SplQueryQueue (nullptr, pInfo->pszName, 3, mpPQI3, SizeNeeded, &SizeNeeded);

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
  
  rc = ::SplEnumQueue(nullptr, 3, nullptr, 0, &mQueueCount,
                      &TotalQueues, &MemNeeded, nullptr);
  PRQINFO3* pPQI3Buf = (PRQINFO3*) malloc (MemNeeded);
  rc = ::SplEnumQueue(nullptr, 3, pPQI3Buf, MemNeeded, &mQueueCount,
                      &TotalQueues, &MemNeeded, nullptr);

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
  
  rc = ::SplEnumQueue(nullptr, 3, nullptr, 0, &newQueueCount,
                      &TotalQueues, &MemNeeded, nullptr);
  PRQINFO3* pPQI3Buf = (PRQINFO3*)malloc(MemNeeded);
  rc = ::SplEnumQueue(nullptr, 3, pPQI3Buf, MemNeeded, &newQueueCount,
                      &TotalQueues, &MemNeeded, nullptr);

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
 
   nsAutoCString pName(mPQBuf[printerNdx]->QueueName());
 
   pName.ReplaceChar('\r', ' ');
   pName.StripChars("\n");
   *printerName = ToNewCString(pName);
}

PRTQUEUE* PRINTDLG::SetPrinterQueue(ULONG printerNdx)
{
   PRTQUEUE *pPQ = nullptr;

   if (printerNdx >= mQueueCount)
      return nullptr;

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
      return nullptr;

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
    PDRIVDATA     pNewDrivData = nullptr;
    LONG          buflen;

/* check size of buffer required for job properties */
    buflen = DevPostDeviceModes( 0 /*hab*/,
                                 nullptr,
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

HDC PrnOpenDC( PRTQUEUE *pInfo, PCSZ pszApplicationName, int copies, int destination, char *file )
{
   HDC hdc = 0;
   PCSZ pszLogAddress;
   PCSZ pszDataType;
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

   dop.pszLogAddress      = const_cast<PSZ>(pszLogAddress); 
   dop.pszDriverName      = (char*)pInfo->DriverName ();
   dop.pdriv              = pInfo->PQI3 ().pDriverData;
   dop.pszDataType        = const_cast<PSZ>(pszDataType); 
   dop.pszComment         = const_cast<PSZ>(pszApplicationName);
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
      lAvail = ::DevQueryHardcopyCaps( hdc, 0, 0, nullptr);

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

