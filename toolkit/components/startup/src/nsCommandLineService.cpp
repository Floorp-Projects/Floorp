/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


#include "nsICmdLineService.h"
#include "nsCommandLineService.h"
#include "nsIComponentManager.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#ifdef XP_MACOSX
#include "nsCommandLineServiceMac.h"
#endif

nsCmdLineService::nsCmdLineService()
	:  mArgCount(0), mArgc(0), mArgv(0)
{
}

/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ISUPPORTS1(nsCmdLineService, nsICmdLineService)

static void* ProcessURLArg(char* str)
{
  // Problem: since the arg parsing code doesn't know which flags
  // take arguments, it always calls this method for the last
  // non-flag argument. But sometimes that argument is actually
  // the arg for the last switch, e.g. -width 500 or -Profile default.
  // nsLocalFile will only work on absolute pathnames, so return
  // if str doesn't start with '/' or '\'.
  if (str && (*str == '\\' || *str == '/'))
  {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), str);
    if (NS_FAILED(rv))
    {
      nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
      if (file)
      {
        rv = file->InitWithNativePath(nsDependentCString(str));
        if (NS_SUCCEEDED(rv))
        {
          nsCAutoString fileurl;
          rv = NS_GetURLSpecFromFile(file, fileurl);
          if (NS_SUCCEEDED(rv))
            return NS_REINTERPRET_CAST(void*, ToNewCString(fileurl));
        }
      }
    }
  }

  return NS_REINTERPRET_CAST(void*, nsCRT::strdup(str));
}

NS_IMETHODIMP
nsCmdLineService::Initialize(int aArgc, char ** aArgv)
{


  PRInt32   i=0;
  nsresult  rv = nsnull;

#ifdef XP_MACOSX
  rv = InitializeMacCommandLine(aArgc, aArgv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Initializing AppleEvents failed");
#endif

  // Save aArgc and argv
  mArgc = aArgc;
  mArgv = new char*[ aArgc ];
  for(i=0; i<aArgc; i++) {
    mArgv[i] = nsCRT::strdup( aArgv[i] ? aArgv[i] : "" );
  }
  //Insert the program name 
  if (aArgc > 0 && aArgv[0])
  {
    mArgList.AppendElement(NS_REINTERPRET_CAST(void*, nsCRT::strdup("-progname")));
    mArgValueList.AppendElement(NS_REINTERPRET_CAST(void*, nsCRT::strdup(aArgv[0])));
    mArgCount++;
    i++;
  }

  for(i=1; i<aArgc; i++) {

    if ((aArgv[i][0] == '-')
#if defined(XP_WIN) || defined(XP_OS2)
        || (aArgv[i][0] == '/')
#endif
      ) {
       /* An option that starts with -. May or may not
	    * have a value after it. 
	    */
	   mArgList.AppendElement(NS_REINTERPRET_CAST(void*, nsCRT::strdup(aArgv[i])));
	   //Increment the index to look ahead at the next option.
       i++;


     //Look ahead if this option has a value like -w 60

	   if (i == aArgc) {
	     /* All args have been parsed. Append a PR_TRUE for the
	      * previous option in the mArgValueList
	      */
	     mArgValueList.AppendElement(NS_REINTERPRET_CAST(void*, nsCRT::strdup("1")));
	     mArgCount++;
	     break;
	   }
     if ((aArgv[i][0] == '-')
#if defined(XP_WIN) || defined(XP_OS2)
         || (aArgv[i][0] == '/')
#endif
       ) {
        /* An other option. The previous one didn't have a value.
         * So, store the previous one's value as PR_TRUE in the
	     * mArgValue array and retract the index so that this option 
	     * will get stored in the next iteration
	     */
        mArgValueList.AppendElement(NS_REINTERPRET_CAST(void*, nsCRT::strdup("1")));
   	    mArgCount++;
        i--;
        continue;
	    }
      else {
        /* The next argument does not start with '-'. This 
	     * could be value to the previous option 
	     */
	      if (i == (aArgc-1)) {
	       /* This is the last argument and a URL 
            * Append a PR_TRUE for the previous option in the value array
            */
		       mArgValueList.AppendElement(ProcessURLArg(aArgv[i]));
	 	       mArgCount++;
           continue;
        }
	      else {
	         /* This is a value to the previous option.
	          * Store it in the mArgValue array 
	          */
             mArgValueList.AppendElement(NS_REINTERPRET_CAST(void*, nsCRT::strdup(aArgv[i])));
	         mArgCount++;
	      }
	   }
  }
  else {
       if (i == (aArgc-1)) {
	      /* This must be the  URL at the end 
	       * Append the url to the arrays
           */
           mArgList.AppendElement(NS_REINTERPRET_CAST(void*, nsCRT::strdup("-url")));
	         mArgValueList.AppendElement(ProcessURLArg(aArgv[i]));
	         mArgCount++;
	     }
	     else {
	       /* A bunch of unrecognized arguments */
	       rv = NS_ERROR_INVALID_ARG;
	     }
  }
	
 }  // for

#if 0
  for (i=0; i<mArgCount; i++)
  {
       printf("Argument: %s, ****** Value: %s\n", (char *)mArgList.ElementAt(i), (char *) mArgValueList.ElementAt(i));      
  }
#endif /* 0 */

   return rv;
	
}

NS_IMETHODIMP
nsCmdLineService::GetURLToLoad(char ** aResult)
{

   return GetCmdLineValue("-url", aResult);
}

NS_IMETHODIMP
nsCmdLineService::GetProgramName(char ** aResult)
{
  nsresult rv = NS_OK;

  *aResult = (char *)mArgValueList.SafeElementAt(0);

  return rv;

}

PRBool nsCmdLineService::ArgsMatch(const char *lookingFor, const char *userGave)
{
    if (!lookingFor || !userGave) return PR_FALSE;

    if (!PL_strcasecmp(lookingFor,userGave)) return PR_TRUE;

#if defined(XP_UNIX) || defined(XP_BEOS)
    /* on unix and beos, we'll allow --mail for -mail */
    if (lookingFor && userGave && (lookingFor[0] != '\0') && (userGave[0] != '\0') && (userGave[1] != '\0')) {
        if (!PL_strcasecmp(lookingFor+1,userGave+2) && (lookingFor[0] == '-') && (userGave[0] == '-') && (userGave[1] == '-')) return PR_TRUE;
    }
#endif
#if defined(XP_WIN) || defined(XP_OS2)
    /* on windows /mail is the same as -mail */
    if (lookingFor && userGave && (lookingFor[0] != '\0') && (userGave[0] != '\0')) {
        if (!PL_strcasecmp(lookingFor+1,userGave+1) && (lookingFor[0] == '-') && (userGave[0] == '/')) return PR_TRUE;
    }
#endif 
    return PR_FALSE;
}

NS_IMETHODIMP
nsCmdLineService::GetCmdLineValue(const char * aArg, char ** aResult)
{
   nsresult  rv = NS_OK;
   
   if (nsnull == aArg || nsnull == aResult ) {
	    return NS_ERROR_NULL_POINTER;
   }

   for (int i = 0; i<mArgCount; i++)
   {
     if (ArgsMatch(aArg,(char *) mArgList.ElementAt(i))) {
       *aResult = nsCRT::strdup((char *)mArgValueList.ElementAt(i));
        return NS_OK;
     }
   }

   *aResult = nsnull;
   return rv;
	
}

NS_IMETHODIMP
nsCmdLineService::GetArgc(PRInt32 * aResult)
{

    if (nsnull == aResult)
        return NS_ERROR_NULL_POINTER;

    // if we are null, we were never initialized.
    if (mArgc == 0)
      return NS_ERROR_FAILURE;

    *aResult =  mArgc;
    return NS_OK;
}

NS_IMETHODIMP
nsCmdLineService::GetArgv(char *** aResult)
{
    if (nsnull == aResult)
      return NS_ERROR_NULL_POINTER;

    // if we are 0, we were never set.
    if (!mArgv)
      return NS_ERROR_FAILURE;

    *aResult = mArgv;

    return NS_OK;
}

nsCmdLineService::~nsCmdLineService()
{
  PRInt32 curr = mArgList.Count();
  while ( curr ) {
    char* str = NS_REINTERPRET_CAST(char*, mArgList[curr-1]);
    if ( str )
      nsMemory::Free(str);
    --curr;
  }
  
  curr = mArgValueList.Count();
  while ( curr ) {
    char* str = NS_REINTERPRET_CAST(char*, mArgValueList[curr-1]);
    if ( str )
      nsMemory::Free(str);
    --curr;
  }

  curr = mArgc;
  while ( curr ) {
    char *str = mArgv ? mArgv[curr-1] : 0;
    if ( str )
      nsMemory::Free( mArgv[curr-1] );
    --curr;
  }
  delete [] mArgv;
}

NS_IMETHODIMP
nsCmdLineService::GetHandlerForParam(const char *aParam,
                                     nsICmdLineHandler** aResult)
{
  nsresult rv;

  // allocate temp on the stack
  nsAutoVoidArray oneParameter;

  nsVoidArray *paramList;
  
  // if user passed in "null", then we want to go through each one
  if (!aParam)
    paramList = &mArgList;
  else {
    oneParameter.AppendElement((void *)aParam);
    paramList = &oneParameter;
  }

  PRUint32 i;
  for (i=0; i < (PRUint32)paramList->Count(); i++) {
    const char *param = (const char*)paramList->ElementAt(i);
    
    // skip past leading / and -
    if (*param == '-' || *param == '/') {
      ++param;
      if (*param == *(param-1)) // skip "--" or "//"
        ++param;
    }
    
    nsCAutoString
      contractID("@mozilla.org/commandlinehandler/general-startup;1?type=");
    
    contractID += param;

    nsCOMPtr<nsICmdLineHandler> handler =
      do_GetService(contractID.get(), &rv);
    if (NS_FAILED(rv)) continue;

    *aResult = handler;
    NS_ADDREF(*aResult);
    return NS_OK;
  }

  // went through all the parameters, didn't find one
  return NS_ERROR_FAILURE;
}

#if 0
NS_IMETHODIMP
nsCmdLineService::PrintCmdArgs()
{

   if (mArgCount == 0) {
     printf("No command line options provided\n");
     return;
   }
   
   for (int i=0; i<mArgCount; i++)
   {
       printf("Argument: %s, ****** Value: %s\n", mArgList.ElementAt(i), mArgValueList.ElementAt(i));      

   }

  return NS_OK;

}
#endif

