/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsITimer.h"
#include "nsTimerCIID.h"
#include "nsIComponentManager.h"
#include "nsCRT.h"
#include "prenv.h"

#include "prinrval.h"

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);
static NS_DEFINE_CID(kCTimerGtk, NS_TIMER_GTK_CID);
static NS_DEFINE_CID(kCTimerMotif, NS_TIMER_MOTIF_CID);
static NS_DEFINE_CID(kCTimerXlib, NS_TIMER_XLIB_CID);

// Yes, this debug code is evil cause it uses a static string.  
#ifdef DEBUG_ramiro
static nsString sToolkitName = "WTF";
#endif

static PRTime sCIStartTime = 0;
static PRTime sCIEndTime = 0;

static nsresult NewTimer(const nsCID & aClass,nsITimer ** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult   rv;
  nsITimer * timer = nsnull;


  sCIStartTime = PR_Now();

  rv = nsComponentManager::CreateInstance(aClass,
                                          nsnull,
                                          nsITimer::GetIID(),
                                          (void **)& timer);


  sCIEndTime = PR_Now();

#ifdef NS_DEBUG
  nsString message;
  
  message = "Couldn't create a ";

#ifdef DEBUG_ramiro
  message += sToolkitName;
  message += " timer";
#else
  message += "timer";
#endif

  NS_ASSERTION(NS_SUCCEEDED(rv), (const char *) nsAutoCString(message));
#endif

  if (nsnull == timer) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}
//////////////////////////////////////////////////////////////////////////
static nsAutoString GetToolkitName()
{
  static const char *   sMozillaToolkit = nsnull;
  static PRBool         sFirstTime = PR_TRUE;

  // Execute the following code only once.
  if ((nsnull == sMozillaToolkit) && !sFirstTime)
  {
    sFirstTime = PR_FALSE;

    // Look in the invironment for MOZ_TOOLKIT.  A variable
    // that controls the toolkit the user wants to use.
    sMozillaToolkit = PR_GetEnv("MOZ_TOOLKIT");

    // If MOZ_TOOLKIT is not set, assume gtk
    if (!sMozillaToolkit)
	  return nsAutoString("gtk");

	// Gtk
	if (nsCRT::strcasecmp(sMozillaToolkit,"gtk") == 0)
	  return nsAutoString("gtk");

	// Motif
	if (nsCRT::strcasecmp(sMozillaToolkit,"motif") == 0)
	  return nsAutoString("motif");

	// Xlib
	if (nsCRT::strcasecmp(sMozillaToolkit,"xlib") == 0)
	  return nsAutoString("xlib");
  }

#ifdef NS_DEBUG
	printf("Toolkit '%s' is unknown.  Assumming Gtk.\n",sMozillaToolkit);
#endif
	
	return nsAutoString("gtk");
}
//////////////////////////////////////////////////////////////////////////
static const nsCID * GetTimerCID()
{
  static const char *   sMozillaToolkit = nsnull;
  static const nsCID *  sTimerClassID = 0;
  static PRBool         sDone = PR_FALSE;

  // The following code should execute only once.  If the environment
  // variable is not set, then 'sMozillaToolkit' will still be null 
  // in the second iteration.  So, we use the sDone flag to make
  // sure it executes only once.
  if ((nsnull == sMozillaToolkit) && !sDone)
  {
    sDone = PR_TRUE;

    // Look in the invironment for MOZ_TOOLKIT.  A variable
    // that controls the toolkit the user wants to use.
    sMozillaToolkit = PR_GetEnv("MOZ_TOOLKIT");

    // If MOZ_TOOLKIT is not set, assume gtk
    if (!sMozillaToolkit)
    {
      sTimerClassID = &kCTimerGtk;
#ifdef DEBUG_ramiro
      sToolkitName = "gtk";
#endif
    }
    // Otherwise check MOZ_TOOLKIT for the toolkit name
    else
    {
      // Gtk
      if (nsCRT::strcasecmp(sMozillaToolkit,"gtk") == 0)
      {
        sTimerClassID = &kCTimerGtk;
#ifdef DEBUG_ramiro
        sToolkitName = "gtk";
#endif
      }
      // Motif
      else if (nsCRT::strcasecmp(sMozillaToolkit,"motif") == 0)
      {
        sTimerClassID = &kCTimerMotif;
#ifdef DEBUG_ramiro
        sToolkitName = "motif";
#endif
      }
      // Xlib
      else if (nsCRT::strcasecmp(sMozillaToolkit,"xlib") == 0)
      {
        sTimerClassID = &kCTimerXlib;
#ifdef DEBUG_ramiro
        sToolkitName = "xlib";
#endif
      }
      // Default to Gtk
      else
      {
#ifdef NS_DEBUG
        printf("Toolkit '%s' is unknown.  Assumming Gtk.\n",
               sMozillaToolkit);
#endif

        sTimerClassID = &kCTimerGtk;
#ifdef DEBUG_ramiro
        sToolkitName = "gtk";
#endif
      }
    }

#ifdef DEBUG_ramiro
    printf("NS_NewTimer() - Using '%s' for the X toolkit.\n",
           sMozillaToolkit);
#endif
  }

  return sTimerClassID;
}

static PRTime sStartTime = 0;
static PRTime sEndTime = 0;
static int sTimerCount = 0;
//////////////////////////////////////////////////////////////////////////
nsresult NS_NewTimer(nsITimer ** aInstancePtrResult)
{
  sStartTime = PR_Now();
  const nsCID * cid = GetTimerCID();

  NS_ASSERTION(nsnull != cid,"Dude! Trying to make a timer with a null CID.");

//  printf("NS_NewTimer(%s)\n",(const char *) nsAutoCString(GetToolkitName()));

  nsresult rv = NewTimer(*cid,aInstancePtrResult);

  sEndTime = PR_Now();

#if 0
  printf("NS_NewTimer(count=%-4d,time=%lld ms,ci=%lld ms)\n",
         sTimerCount++,
         sEndTime - sStartTime,
         sCIEndTime - sCIStartTime);
#endif

  return rv;
}
