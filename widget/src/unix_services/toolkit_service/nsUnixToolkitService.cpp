/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsUnixToolkitService.h"  

#include "nsUnixTimerCIID.h"      // For timer CIDs
#include "nsIComponentManager.h"  // For FindFactory()
#include "nsITimer.h"             // For nsITimer
#include "nsCRT.h"                // For strcasecmp()
#include "prenv.h"                // For PR_GetEnv()
#include "nsFileSpec.h"           // For nsCAutoString

/* static */ nsCString * nsUnixToolkitService::sWidgetToolkitName = nsnull;
/* static */ nsCString * nsUnixToolkitService::sGfxToolkitName = nsnull;
/* static */ nsCString * nsUnixToolkitService::sWidgetDllName = nsnull;
/* static */ nsCString * nsUnixToolkitService::sGfxDllName = nsnull;

/* static */ const nsCID * nsUnixToolkitService::sTimerCID = nsnull;

#ifdef MOZ_DEFAULT_TOOLKIT
/* static */ const char *  nsUnixToolkitService::ksDefaultToolkit = MOZ_DEFAULT_TOOLKIT;
#else
/* static */ const char *  nsUnixToolkitService::ksDefaultToolkit = "gtk";
#endif

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
  NS_INIT_REFCNT();
}

nsUnixToolkitService::~nsUnixToolkitService()
{
}

NS_IMPL_ADDREF(nsUnixToolkitService)
NS_IMPL_RELEASE(nsUnixToolkitService)
NS_IMPL_QUERY_INTERFACE(nsUnixToolkitService, NS_GET_IID(nsIUnixToolkitService))

//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::SetToolkitName(const nsCString & aToolkitName)
{
  PRBool isValid;
  nsresult rv = NS_OK;

  IsValidToolkit(aToolkitName,&isValid);

  NS_ASSERTION(isValid == PR_TRUE,"Invalid toolkit.");
  
  if (isValid)
  {
    Cleanup();

    rv = SetWidgetToolkitName(aToolkitName);

    if (NS_OK == rv)
    {
      rv = SetGfxToolkitName(aToolkitName);
    }
  }

  return rv;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::SetWidgetToolkitName(const nsCString & aToolkitName)
{
  PRBool isValid;

  IsValidWidgetToolkit(aToolkitName,&isValid);

  NS_ASSERTION(isValid == PR_TRUE,"Invalid toolkit.");
  
  if (isValid)
  {
    Cleanup();

    sWidgetToolkitName = new nsCString(aToolkitName);
    sGfxToolkitName = new nsCString(aToolkitName);
  }

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::SetGfxToolkitName(const nsCString & aToolkitName)
{
  PRBool isValid;

  IsValidGfxToolkit(aToolkitName,&isValid);

  NS_ASSERTION(isValid == PR_TRUE,"Invalid toolkit.");
  
  if (isValid)
  {
    Cleanup();

    sWidgetToolkitName = new nsCString(aToolkitName);
    sGfxToolkitName = new nsCString(aToolkitName);
  }

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::IsValidToolkit(const nsCString & aToolkitName,
                                     PRBool *         aResultOut)
{
  NS_ASSERTION(nsnull != aResultOut,"null out param.");

  *aResultOut = PR_FALSE;

  if (aToolkitName == "gtk"   ||
      aToolkitName == "motif" ||
      aToolkitName == "xlib"  ||
      aToolkitName == "qt")
  {
    *aResultOut = PR_TRUE;
  }

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::IsValidWidgetToolkit(const nsCString & aToolkitName,
                                           PRBool *         aResultOut)
{
  return IsValidToolkit(aToolkitName, aResultOut);
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::IsValidGfxToolkit(const nsCString & aToolkitName,
                                        PRBool *         aResultOut)
{
  return IsValidToolkit(aToolkitName, aResultOut);
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::GetToolkitName(nsCString & aToolkitNameOut)
{
  aToolkitNameOut = "";

  return GetWidgetToolkitName(aToolkitNameOut);
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::GetWidgetToolkitName(nsCString & aToolkitNameOut)
{
  aToolkitNameOut = "";

  GlobalGetWidgetToolkitName(aToolkitNameOut);

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::GetGfxToolkitName(nsCString & aToolkitNameOut)
{
  aToolkitNameOut = "";

  GlobalGetGfxToolkitName(aToolkitNameOut);

  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::GetWidgetDllName(nsCString & aWidgetDllNameOut)
{
  nsresult rv = NS_ERROR_FAILURE;

  // Set sWidgetDllName only once
  if (nsnull == sWidgetDllName)
  {
    nsCAutoString  name;
    nsCAutoString  toolkit;
    
    nsresult rv2 = GlobalGetWidgetToolkitName(toolkit);

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

    sWidgetDllName = new nsCString(name);

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
nsUnixToolkitService::GetGfxDllName(nsCString & aGfxDllNameOut)
{
  nsresult rv = NS_ERROR_FAILURE;

  // Set sGfxDllName only once
  if (nsnull == sGfxDllName)
  {
    nsCAutoString  name;
    nsCAutoString  toolkit;
    
    nsresult rv2 = GlobalGetGfxToolkitName(toolkit);

    if (NS_OK == rv2)
    {
      name = ksDllPrefix;
      
      name += ksGfxName;
      
      name += "_";
      
      name += toolkit;
//      name += "xlib";
      
      name += ksDllSuffix;

      rv = NS_OK;
    }
    else
    {
      name = "error";
    }

    sGfxDllName = new nsCString(name);

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
static NS_DEFINE_CID(kCTimerQtCID, NS_TIMER_QT_CID);

//////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsUnixToolkitService::GetTimerCID(nsCID ** aTimerCIDOut)
{
  NS_ASSERTION(nsnull != aTimerCIDOut,"Out param aTimerCIDOut is null,");

  *aTimerCIDOut = nsnull;

  if (nsnull == sTimerCID)
  {
    nsCAutoString unixToolkitName;
    
    GlobalGetWidgetToolkitName(unixToolkitName);
    
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
    // Qt
    else if (unixToolkitName == "qt")
    {
      sTimerCID = &kCTimerQtCID;
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
nsUnixToolkitService::GlobalGetWidgetToolkitName(nsCString & aStringOut)
{
  nsresult rv = EnsureWidgetToolkitName();

  if (NS_OK == rv)
  {
    aStringOut = *sWidgetToolkitName;
  }
  else
  {
    aStringOut = "error";
  }

  return rv;
}

//////////////////////////////////////////////////////////////////////////
/* static */ nsresult
nsUnixToolkitService::GlobalGetGfxToolkitName(nsCString & aStringOut)
{
  nsresult rv = EnsureGfxToolkitName();

  if (NS_OK == rv)
  {
    aStringOut = *sGfxToolkitName;
  }
  else
  {
    aStringOut = "error";
  }

  return rv;
}

//////////////////////////////////////////////////////////////////////////
/* static */ nsresult
nsUnixToolkitService::EnsureWidgetToolkitName()
{
  // Initialize sWidgetToolkitName only once
  if (nsnull != sWidgetToolkitName)
    return NS_OK;

  sWidgetToolkitName = new nsCString("unknown");

  if (!sWidgetToolkitName)
    return NS_ERROR_OUT_OF_MEMORY;
  
  // The env variable
  const char * MOZ_WIDGET_TOOLKIT = nsnull;

  // Look in the environment for MOZ_WIDGET_TOOLKIT.  A variable
  // that controls the widget toolkit the user wants to use.
  MOZ_WIDGET_TOOLKIT = PR_GetEnv("MOZ_WIDGET_TOOLKIT");

  // If MOZ_WIDGET_TOOLKIT is not set, look for MOZ_TOOLKIT.
  if (!MOZ_WIDGET_TOOLKIT)
  {
    // Look in the environment for MOZ_TOOLKIT.  A variable
    // that controls the toolkit the user wants to use.
    MOZ_WIDGET_TOOLKIT = PR_GetEnv("MOZ_TOOLKIT");
  }
   
  // If MOZ_TOOLKIT is not set, assume default
  if (!MOZ_WIDGET_TOOLKIT)
  {
    *sWidgetToolkitName = ksDefaultToolkit;
  }
  // Gtk
  else if (nsCRT::strcasecmp(MOZ_WIDGET_TOOLKIT,"gtk") == 0)
  {
    *sWidgetToolkitName = "gtk";
  }
  // Xlib
  else if (nsCRT::strcasecmp(MOZ_WIDGET_TOOLKIT,"xlib") == 0)
  {
    *sWidgetToolkitName = "xlib";
  }
  // Motif
  else if (nsCRT::strcasecmp(MOZ_WIDGET_TOOLKIT,"motif") == 0)
  {
    *sWidgetToolkitName = "motif";
  }
  // Qt
  else if (nsCRT::strcasecmp(MOZ_WIDGET_TOOLKIT,"qt") == 0)
  {
    *sWidgetToolkitName = "qt";
  }
  else
  {
    *sWidgetToolkitName = ksDefaultToolkit;
    
#ifdef NS_DEBUG
    printf("nsUnixToolkitService: Unknown widget toolkit '%s'.  Using '%s'.\n",
           (const char *) MOZ_WIDGET_TOOLKIT,
           (const char *) ksDefaultToolkit);
#endif
  }
  
#ifdef NS_DEBUG
  printf("nsUnixToolkitService: Using '%s' for the Widget Toolkit.\n",
         (const char *) nsCAutoString(*sWidgetToolkitName));
#endif
  
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
/* static */ nsresult
nsUnixToolkitService::EnsureGfxToolkitName()
{
  // Initialize sGfxToolkitName only once
  if (nsnull != sGfxToolkitName)
    return NS_OK;

  sGfxToolkitName = new nsCString("unknown");

  if (!sGfxToolkitName)
    return NS_ERROR_OUT_OF_MEMORY;
  
  // The env variable
  const char * MOZ_GFX_TOOLKIT = nsnull;

  // Look in the environment for MOZ_GFX_TOOLKIT.  A variable
  // that controls the toolkit the user wants to use.
  MOZ_GFX_TOOLKIT = PR_GetEnv("MOZ_GFX_TOOLKIT");

  // If MOZ_GFX_TOOLKIT is not set, look for MOZ_TOOLKIT.
  if (!MOZ_GFX_TOOLKIT)
  {  
    // Look in the environment for MOZ_TOOLKIT.  A variable
    // that controls the toolkit the user wants to use.
    MOZ_GFX_TOOLKIT = PR_GetEnv("MOZ_TOOLKIT");
  }
  
  // If MOZ_TOOLKIT is not set, assume default
  if (!MOZ_GFX_TOOLKIT)
  {
    *sGfxToolkitName = ksDefaultToolkit;
  }
  // Gtk
  else if (nsCRT::strcasecmp(MOZ_GFX_TOOLKIT,"gtk") == 0)
  {
    *sGfxToolkitName = "gtk";
  }
  // Xlib
  else if (nsCRT::strcasecmp(MOZ_GFX_TOOLKIT,"xlib") == 0)
  {
    *sGfxToolkitName = "xlib";
  }
  // Motif
  else if (nsCRT::strcasecmp(MOZ_GFX_TOOLKIT,"motif") == 0)
  {
    *sGfxToolkitName = "motif";
  }
  // Qt
  else if (nsCRT::strcasecmp(MOZ_GFX_TOOLKIT,"qt") == 0)
  {
    *sGfxToolkitName = "qt";
  }
  else
  {
    *sGfxToolkitName = ksDefaultToolkit;
    
#ifdef NS_DEBUG
    printf("nsUnixToolkitService: Unknown gfx toolkit '%s'.  Using '%s'.\n",
           (const char *) MOZ_GFX_TOOLKIT,
           (const char *) ksDefaultToolkit);
#endif
  }
  
#ifdef NS_DEBUG
  printf("nsUnixToolkitService: Using '%s' for the Gfx Toolkit.\n",
         (const char *) nsCAutoString(*sGfxToolkitName));
#endif
  
  return NS_OK;
}
//////////////////////////////////////////////////////////////////////////
/* static */ nsresult
nsUnixToolkitService::Cleanup()
{
  if (nsnull != sWidgetToolkitName)
  {
    delete sWidgetToolkitName;
    
    sWidgetToolkitName = nsnull;
  }

  if (nsnull != sGfxToolkitName)
  {
    delete sGfxToolkitName;
    
    sGfxToolkitName = nsnull;
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
