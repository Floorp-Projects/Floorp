/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C) 1999
 * Christopher Blizzard.  All Rights Reserved.
 */

#include "nsUnixToolkitService.h"  

#include "nsUnixTimerCIID.h"      // For timer CIDs
#include "nsIComponentManager.h"  // For FindFactory()
#include "nsITimer.h"             // For nsITimer
#include "nsCRT.h"                // For strcasecmp()
#include "prenv.h"                // For PR_GetEnv()
#include "nsFileSpec.h"           // For nsAutoCString

/* static */ nsString * nsUnixToolkitService::sToolkitName = nsnull;
/* static */ nsString * nsUnixToolkitService::sWidgetDllName = nsnull;
/* static */ nsString * nsUnixToolkitService::sGfxDllName = nsnull;

/* static */ const nsCID * nsUnixToolkitService::sTimerCID = nsnull;

/* static */ const char *  nsUnixToolkitService::ksDefaultToolkit = "gtk";

#ifdef MOZ_DLL_SUFFIX    
/* static */ const char *  nsUnixToolkitService::ksDllSuffix = MOZ_DLL_SUFFIX;
#else
/* static */ const char *  nsUnixToolkitService::ksDllSuffix = ".so";
#endif

/* static */ const char *  nsUnixToolkitService::ksDllPrefix = "lib";

/* static */ const char *  nsUnixToolkitService::ksWidgetName = "widget";
/* static */ const char *  nsUnixToolkitService::ksGfxName = "gfx";

nsUnixToolkitService::nsUnixToolkitService()
{
}

nsUnixToolkitService::~nsUnixToolkitService()
{
}

NS_IMPL_ADDREF(nsUnixToolkitService)
NS_IMPL_RELEASE(nsUnixToolkitService)
NS_IMPL_QUERY_INTERFACE(nsUnixToolkitService, nsCOMTypeInfo<nsIUnixToolkitService>::GetIID())

//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::SetToolkitName(const nsString & aToolkitName)
{
  PRBool isValid;

  IsValidToolkit(aToolkitName,&isValid);

  NS_ASSERTION(isValid == PR_TRUE,"Invalid toolkit.");
  
  if (isValid)
  {
    Cleanup();

    sToolkitName = new nsString(aToolkitName);
  }

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::IsValidToolkit(const nsString & aToolkitName,
                                     PRBool *         aResultOut)
{
  NS_ASSERTION(nsnull != aResultOut,"null out param.");

  *aResultOut = PR_FALSE;

  if (aToolkitName == "gtk" ||
      aToolkitName == "motif" ||
      aToolkitName == "xlib")
  {
    *aResultOut = PR_TRUE;
  }

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::GetToolkitName(nsString & aToolkitNameOut)
{
  aToolkitNameOut = "";

  GlobalGetToolkitName(aToolkitNameOut);

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::GetWidgetDllName(nsString & aWidgetDllNameOut)
{
  nsresult rv = NS_ERROR_FAILURE;

  // Set sWidgetDllName only once
  if (nsnull == sWidgetDllName)
  {
    nsString  name("");
    nsString  toolkit("");
    
    nsresult rv2 = GlobalGetToolkitName(toolkit);

    if (NS_OK == rv2)
    {
      name = ksDllPrefix;
      
      name += ksWidgetName;
      
      name += "_";
      
      name += toolkit;
      
      name += ksDllSuffix;

      rv = NS_OK;
    }
    else
    {
      name = "error";
    }

    sWidgetDllName = new nsString(name);

    if (!sWidgetDllName)
    {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }    

  NS_ASSERTION(nsnull != sWidgetDllName,"Global widget dll name is null.");
    
  aWidgetDllNameOut = *sWidgetDllName;

  return rv;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::GetGfxDllName(nsString & aGfxDllNameOut)
{
  nsresult rv = NS_ERROR_FAILURE;

  // Set sGfxDllName only once
  if (nsnull == sGfxDllName)
  {
    nsString  name("");
    nsString  toolkit("");
    
    nsresult rv2 = GlobalGetToolkitName(toolkit);

    if (NS_OK == rv2)
    {
      name = ksDllPrefix;
      
      name += ksGfxName;
      
      name += "_";
      
//      name += toolkit;
      name += "xlib";
      
      name += ksDllSuffix;

      rv = NS_OK;
    }
    else
    {
      name = "error";
    }

    sGfxDllName = new nsString(name);

    if (!sGfxDllName)
    {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }    

  NS_ASSERTION(nsnull != sGfxDllName,"Global gfx dll name is null.");
    
  aGfxDllNameOut = *sGfxDllName;

  return rv;
}
//////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

static NS_DEFINE_CID(kCTimerGtkCID, NS_TIMER_GTK_CID);
static NS_DEFINE_CID(kCTimerMotifCID, NS_TIMER_MOTIF_CID);
static NS_DEFINE_CID(kCTimerXlibCID, NS_TIMER_XLIB_CID);

//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::GetTimerCID(nsCID ** aTimerCIDOut)
{
  NS_ASSERTION(nsnull != aTimerCIDOut,"Out param aTimerCIDOut is null,");

  *aTimerCIDOut = nsnull;

  if (nsnull == sTimerCID)
  {
    nsString unixToolkitName;
    
    GlobalGetToolkitName(unixToolkitName);
    
    //
    // Aassume that unixToolkitName is valid.
    //
    
    // Gtk
    if (unixToolkitName == "gtk")
    {
      sTimerCID = &kCTimerGtkCID;
    }
    // Motif
    else if (unixToolkitName == "motif")
    {
      sTimerCID = &kCTimerMotifCID;
    }
    // Xlib
    else if (unixToolkitName == "xlib")
    {
      sTimerCID = &kCTimerXlibCID;
    }
    else
    {
      NS_ASSERTION(0,"Not a valid toolkit,");
    }
  }

  // Cast away the const, hmmm...
  *aTimerCIDOut = (nsCID *) sTimerCID;

  return (nsnull != *aTimerCIDOut) ? NS_OK : NS_ERROR_FAILURE;
}
//////////////////////////////////////////////////////////////////////////
/* static */ nsresult
nsUnixToolkitService::GlobalGetToolkitName(nsString & aStringOut)
{
  nsresult rv = EnsureToolkitName();

  if (NS_OK == rv)
  {
    aStringOut = *sToolkitName;
  }
  else
  {
    aStringOut = "error";
  }

  return rv;
}
//////////////////////////////////////////////////////////////////////////
/* static */ nsresult
nsUnixToolkitService::EnsureToolkitName()
{
  // Initialize sToolkitName only once
  if (nsnull != sToolkitName)
    return NS_OK;

  sToolkitName = new nsString("unknown");

  if (!sToolkitName)
    return NS_ERROR_OUT_OF_MEMORY;
  
  // The env variable
  const char * MOZ_TOOLKIT = nsnull;

  // Look in the invironment for MOZ_TOOLKIT.  A variable
  // that controls the toolkit the user wants to use.
  MOZ_TOOLKIT = PR_GetEnv("MOZ_TOOLKIT");

  // If MOZ_TOOLKIT is not set, assume default
  if (!MOZ_TOOLKIT)
  {
    *sToolkitName = ksDefaultToolkit;
  }
  // Gtk
  else if (nsCRT::strcasecmp(MOZ_TOOLKIT,"gtk") == 0)
  {
    *sToolkitName = "gtk";
  }
  // Xlib
  else if (nsCRT::strcasecmp(MOZ_TOOLKIT,"xlib") == 0)
  {
    *sToolkitName = "xlib";
  }
  // Motif
  else if (nsCRT::strcasecmp(MOZ_TOOLKIT,"motif") == 0)
  {
    *sToolkitName = "motif";
  }
  else
  {
    *sToolkitName = ksDefaultToolkit;
    
#ifdef NS_DEBUG
    printf("nsUnixToolkitService: Unknown toolkit '%s'.  Using '%s'.\n",
           (const char *) MOZ_TOOLKIT,
           (const char *) ksDefaultToolkit);
#endif
  }
  
#ifdef NS_DEBUG
  printf("nsUnixToolkitService: Using '%s' for the Toolkit.\n",
         (const char *) nsAutoCString(*sToolkitName));
#endif
  
  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
/* static */ nsresult
nsUnixToolkitService::Cleanup()
{
  if (nsnull != sToolkitName)
  {
    delete sToolkitName;
    
    sToolkitName = nsnull;
  }

  if (nsnull != sWidgetDllName)
  {
    delete sWidgetDllName;
    
    sWidgetDllName = nsnull;
  }

  if (nsnull != sGfxDllName)
  {
    delete sGfxDllName;
    
    sGfxDllName = nsnull;
  }
  
  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
