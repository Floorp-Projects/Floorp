/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * The purpose of this file is to help phase out XP_ library
 * from the image library. In general, XP_ data structures and
 * functions will be replaced with the PR_ or PL_ equivalents.
 * In cases where the PR_ or PL_ equivalents don't yet exist,
 * this file (and its header equivalent) will play the role 
 * of the XP_ library.
 */
#include "xpcompat.h"
#include "xp_mcom.h"
#include <stdlib.h>
/* BSDI did not have this header and we do not need it here.  -slamm */
/* #include <search.h> */
#include "prlog.h"
#include "prmem.h"
#include "plstr.h"
#include "ilISystemServices.h"

extern ilISystemServices *il_ss;

PR_BEGIN_EXTERN_C
int MK_UNABLE_TO_LOCATE_FILE = -1;
int MK_OUT_OF_MEMORY = -2;

int XP_MSG_IMAGE_PIXELS = -7;
int XP_MSG_IMAGE_NOT_FOUND = -8;
int XP_MSG_XBIT_COLOR = -9;	
int XP_MSG_1BIT_MONO = -10;
int XP_MSG_XBIT_GREYSCALE = -11;
int XP_MSG_XBIT_RGB = -12;
int XP_MSG_DECODED_SIZE = -13;	
int XP_MSG_WIDTH_HEIGHT = -14;	
int XP_MSG_SCALED_FROM = -15;	
int XP_MSG_IMAGE_DIM = -16;	
int XP_MSG_COLOR = -17;	
int XP_MSG_NB_COLORS = -18;	
int XP_MSG_NONE = -19;	
int XP_MSG_COLORMAP = -20;	
int XP_MSG_BCKDRP_VISIBLE = -21;	
int XP_MSG_SOLID_BKGND = -22;	
int XP_MSG_JUST_NO = -23;	
int XP_MSG_TRANSPARENCY = -24;	
int XP_MSG_COMMENT = -25;	
int XP_MSG_UNKNOWN = -26;	
int XP_MSG_COMPRESS_REMOVE = -27;	
PR_END_EXTERN_C

char *XP_GetString(int i)
{
  return ("XP_GetString replacement needed");
}

#ifdef XP_MAC
#include <OSUtils.h>

static void MyReadLocation(MachineLocation * loc)
{
	static MachineLocation storedLoc;	// InsideMac, OSUtilities, page 4-20
	static Boolean didReadLocation = FALSE;
	if (!didReadLocation)
	{	
		ReadLocation(&storedLoc);
		didReadLocation = TRUE;
	}
	*loc = storedLoc;
}

// current local time = GMTDelta() + GMT
// GMT = local time - GMTDelta()
static long GMTDelta()
{
	MachineLocation loc;
	long gmtDelta;
	
	MyReadLocation(&loc);
	gmtDelta = loc.u.gmtDelta & 0x00FFFFFF;
	if ((gmtDelta & 0x00800000) != 0)
		gmtDelta |= 0xFF000000;
	return gmtDelta;
}

// This routine simulates stdclib time(), time in seconds since 1.1.1970
// The time is in GMT
time_t GetTimeMac()
{
	unsigned long maclocal;
	// Get Mac local time
	GetDateTime(&maclocal); 
	// Get Mac GMT	
	maclocal -= GMTDelta();
	// return unix GMT
	return (maclocal - UNIXMINUSMACTIME);
}

// Returns the GMT times
time_t Mactime(time_t *timer)
{
	time_t t = GetTimeMac();
	if (timer != NULL)
		*timer = t;
	return t;
}
#endif /* XP_MAC */

/*	Allocate a new copy of a block of binary data, and returns it
 */
char * 
NET_BACopy (char **destination, const char *source, size_t length)
{
	if(*destination)
	  {
	    PR_FREEIF(*destination);
		*destination = 0;
	  }

    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) PR_MALLOC (length);
        if (*destination == NULL) 
	        return(NULL);
        XP_MEMCPY(*destination, source, length);
      }
    return *destination;
}

/*	binary block Allocate and Concatenate
 *
 *   destination_length  is the length of the existing block
 *   source_length   is the length of the block being added to the 
 *   destination block
 */
char * 
NET_BACat (char **destination, 
		   size_t destination_length, 
		   const char *source, 
		   size_t source_length)
{
    if (source) 
	  {
        if (*destination) 
	      {
      	    *destination = (char *) PR_REALLOC (*destination, destination_length + source_length);
            if (*destination == NULL) 
	          return(NULL);

            XP_MEMMOVE (*destination + destination_length, source, source_length);

          } 
		else 
		  {
            *destination = (char *) PR_MALLOC (source_length);
            if (*destination == NULL) 
	          return(NULL);

            XP_MEMCPY(*destination, source, source_length);
          }
    }

  return *destination;
}

/*	Very similar to strdup except it free's too
 */
char * 
NET_SACopy (char **destination, const char *source)
{
	if(*destination)
	  {
	    PR_FREEIF(*destination);
		*destination = 0;
	  }
    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) PR_MALLOC (PL_strlen(source) + 1);
        if (*destination == NULL) 
 	        return(NULL);

        PL_strcpy (*destination, source);
      }
    return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
char *
NET_SACat (char **destination, const char *source)
{
    if (source && *source)
      {
        if (*destination)
          {
            int length = PL_strlen (*destination);
            *destination = (char *) PR_REALLOC (*destination, length + PL_strlen(source) + 1);
            if (*destination == NULL)
            return(NULL);

            PL_strcpy (*destination + length, source);
          }
        else
          {
            *destination = (char *) PR_MALLOC (PL_strlen(source) + 1);
            if (*destination == NULL)
                return(NULL);

             PL_strcpy (*destination, source);
          }
      }
    return *destination;
}

#if 0
#include <windows.h>
#include <limits.h>

static void wfe_ProcessTimeouts(DWORD dwNow);

// structure to keep track of FE_SetTimeOut objects
typedef struct WinTimeStruct {
    TimeoutCallbackFunction   fn;
    void                    * closure;
    DWORD                     dwFireTime;
    struct WinTimeStruct    * pNext;
} WinTime;

// the one and only list of objects waiting for FE_SetTimeOut
WinTime *gTimeOutList = NULL;

//  Process timeouts now.
//  Called once per round of fire.
UINT uTimeoutTimer = 0;
DWORD dwNextFire = (DWORD)-1;
DWORD dwSyncHack = 0;

void CALLBACK FireTimeout(HWND hWnd, UINT uMessage, UINT uTimerID, DWORD dwTime)
{
    static BOOL bCanEnter = TRUE;

    //  Don't allow old timer messages in here.
    if(uMessage != WM_TIMER)    {
        PR_ASSERT(0);
        return;
    }
    if(uTimerID != uTimeoutTimer)   {
        return;
    }

    //  Block only one entry into this function, or else.
    if(bCanEnter)   {
        bCanEnter = FALSE;
        // see if we need to fork off any timeout functions
        if(gTimeOutList)    {
            wfe_ProcessTimeouts(dwTime);
        }
        bCanEnter = TRUE;
    }
}

//  Function to correctly have the timer be set.
void SyncTimeoutPeriod(DWORD dwTickCount)
{
    //  May want us to set tick count ourselves.
    if(dwTickCount == 0)    {
        if(dwSyncHack == 0) {
            dwTickCount = GetTickCount();
        }
        else    {
            dwTickCount = dwSyncHack;
        }
    }

    //  If there's no list, we should clear the timer.
    if(!gTimeOutList)    {
        if(uTimeoutTimer)   {
            ::KillTimer(NULL, uTimeoutTimer);
            uTimeoutTimer = 0;
            dwNextFire = (DWORD)-1;
        }
    }
    else {
        //  See if we need to clear the current timer.
        //  Curcumstances are that if the timer will not
        //      fire on time for the next timeout.
        BOOL bSetTimer = FALSE;
        WinTime *pTimeout = gTimeOutList;
        if(uTimeoutTimer)   {
            if(pTimeout->dwFireTime != dwNextFire)   {
                //  There is no need to kill the timer if we are just going to set it again.
                //  Windows will simply respect the new time interval passed in.
                ::KillTimer(NULL, uTimeoutTimer);
                uTimeoutTimer = 0;
                dwNextFire = (DWORD)-1;

                //  Set the timer.
                bSetTimer = TRUE;
            }
        }
        else    {
            //  No timer set, attempt.
            bSetTimer = TRUE;
        }

        if(bSetTimer)   {
            DWORD dwFireWhen = pTimeout->dwFireTime > dwTickCount ?
                pTimeout->dwFireTime - dwTickCount : 0;
            UINT uFireWhen = (UINT)dwFireWhen;

            PR_ASSERT(uTimeoutTimer == 0);
            uTimeoutTimer = ::SetTimer(
                NULL, 
                0, 
                uFireWhen, 
                (TIMERPROC)FireTimeout);

            if(uTimeoutTimer)   {
                //  Set the fire time.
                dwNextFire = pTimeout->dwFireTime;
            }
        }
    }
}

/* this function should register a function that will
 * be called after the specified interval of time has
 * elapsed.  This function should return an id 
 * that can be passed to IL_ClearTimeout to cancel 
 * the Timeout request.
 *
 * A) Timeouts never fail to trigger, and
 * B) Timeouts don't trigger *before* their nominal timestamp expires, and
 * C) Timeouts trigger in the same ordering as their timestamps
 *
 * After the function has been called it is unregistered
 * and will not be called again unless re-registered.
 *
 * func:    The function to be invoked upon expiration of
 *          the Timeout interval 
 * closure: Data to be passed as the only argument to "func"
 * msecs:   The number of milli-seconds in the interval
 */
void * 
IL_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs)
{
    WinTime * pTime = new WinTime;
    if(!pTime)
        return(NULL);

    // fill it out
    DWORD dwNow = GetTickCount();
    pTime->fn = func;
    pTime->closure = closure;
    pTime->dwFireTime = (DWORD) msecs + dwNow;
    //CLM: Always clear this else the last timer added will have garbage!
    //     (Win16 revealed this bug!)
    pTime->pNext = NULL;
    
    // add it to the list
    if(!gTimeOutList) {        
        // no list add it
        gTimeOutList = pTime;
    } else {

        // is it before everything else on the list?
        if(pTime->dwFireTime < gTimeOutList->dwFireTime) {

            pTime->pNext = gTimeOutList;
            gTimeOutList = pTime;

        } else {

            WinTime * pPrev = gTimeOutList;
            WinTime * pCurrent = gTimeOutList;

            while(pCurrent && (pCurrent->dwFireTime <= pTime->dwFireTime)) {
                pPrev = pCurrent;
                pCurrent = pCurrent->pNext;
            }

            PR_ASSERT(pPrev);

            // insert it after pPrev (this could be at the end of the list)
            pTime->pNext = pPrev->pNext;
            pPrev->pNext = pTime;

        }

    }

    //  Sync the timer fire period.
    SyncTimeoutPeriod(dwNow);

    return(pTime);
}


/* This function cancels a Timeout that has previously been
 * set.  
 * Callers should not pass in NULL or a timer_id that
 * has already expired.
 */
void 
IL_ClearTimeout(void * pStuff)
{
    WinTime * pTimer = (WinTime *) pStuff;

    if(!pTimer) {
        return;
    }

    if(gTimeOutList == pTimer) {

        // first element in the list lossage
        gTimeOutList = pTimer->pNext;

    } else {

        // walk until no next pointer
        for(WinTime * p = gTimeOutList; p && p->pNext && (p->pNext != pTimer); p = p->pNext)
            ;

        // if we found something valid pull it out of the list
        if(p && p->pNext && p->pNext == pTimer) {
            p->pNext = pTimer->pNext;

        } else {
            // get out before we delete something that looks bogus
            return;
        }

    }

    // if we got here it must have been a valid element so trash it
    delete pTimer;

    //  If there's now no be sure to clear the timer.
    SyncTimeoutPeriod(0);
}

//
// Walk down the timeout list and launch anyone appropriate
// Cleaned up logic 04-30-96 GAB
//
static void wfe_ProcessTimeouts(DWORD dwNow)
{
    WinTime *p = gTimeOutList;
    if(dwNow == 0)   {
        dwNow = GetTickCount();
    }

    BOOL bCalledSync = FALSE;

    //  Set the hack, such that when IL_ClearTimeout
    //      calls SyncTimeoutPeriod, that GetTickCount()
    //      overhead is not incurred.
    dwSyncHack = dwNow;
    
    // loop over all entries
    while(p) {
        // send it
        if(p->dwFireTime < dwNow) {
            //  Fire it.
            (*p->fn) (p->closure);

            //  Clear the timer.
            //  Period synced.
            IL_ClearTimeout(p);
            bCalledSync = TRUE;

            //  Reset the loop (can't look at p->pNext now, and called
            //      code may have added/cleared timers).
            //  (could do this by going recursive and returning).
            p = gTimeOutList;
        } else {
			//	Make sure we fire an timer.
            //  Also, we need to check to see if things are backing up (they
            //      may be asking to be fired long before we ever get to them,
            //      and we don't want to pass in negative values to the real
            //      timer code, or it takes days to fire....
            if(bCalledSync == FALSE)    {
                SyncTimeoutPeriod(dwNow);
                bCalledSync = TRUE;
            }
            //  Get next timer.
            p = p->pNext;
        }
    }
    dwSyncHack = 0;
}
#else
NS_EXPORT void*
IL_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs)
{
    return il_ss->SetTimeout((ilTimeoutCallbackFunction)func,
                             closure, msecs);
}

NS_EXPORT void
IL_ClearTimeout(void *timer_id)
{
    il_ss->ClearTimeout(timer_id);
}
#endif /* XP_PC */

