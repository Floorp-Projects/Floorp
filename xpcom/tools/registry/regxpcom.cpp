/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mike Shaver <shaver@mozilla.org>
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

#include "stdlib.h"
#include "prenv.h"
#include "nspr.h"

#include "nsXPCOMPrivate.h" // for XPCOM_DLL defines.

#include "nsXPCOMGlue.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsEmbedString.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"


static PRBool gUnreg = PR_FALSE, gQuiet = PR_FALSE;

static const char* gXPCOMLocation = nsnull;
static const char* gCompRegLocation = nsnull;
static const char* gXPTIDatLocation = nsnull;
static char* gPathEnvString = nsnull;

class DirectoryServiceProvider : public nsIDirectoryServiceProvider
{
  public:
  DirectoryServiceProvider() {}
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER

  private:
  ~DirectoryServiceProvider() {}
};

NS_IMPL_ISUPPORTS1(DirectoryServiceProvider, nsIDirectoryServiceProvider)

NS_IMETHODIMP
DirectoryServiceProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{    
  nsCOMPtr<nsILocalFile> localFile;
  nsresult rv = NS_ERROR_FAILURE;

  *_retval = nsnull;
  *persistant = PR_TRUE;

  const char* fileLocation = nsnull;

  if(strcmp(prop, NS_XPCOM_CURRENT_PROCESS_DIR) == 0 && gXPCOMLocation)
  {
    fileLocation = gXPCOMLocation;
  }
  else if(strcmp(prop, NS_XPCOM_COMPONENT_REGISTRY_FILE) == 0 && gCompRegLocation)
  {
    fileLocation = gCompRegLocation;
  }    
  else if(strcmp(prop, NS_XPCOM_XPTI_REGISTRY_FILE) == 0 && gXPTIDatLocation)
  {
    fileLocation = gXPTIDatLocation;
  }
  else
    return NS_ERROR_FAILURE;

  rv = NS_NewNativeLocalFile(nsEmbedCString(fileLocation), PR_TRUE, getter_AddRefs(localFile));  
  if (NS_FAILED(rv)) return rv;

  return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
}

int startup_xpcom()
{
  nsresult rv;

  if (gXPCOMLocation) {
    int len = strlen(gXPCOMLocation);
    char* xpcomPath = (char*) malloc(len + sizeof(XPCOM_DLL) + sizeof(XPCOM_FILE_PATH_SEPARATOR) + 1);
    sprintf(xpcomPath, "%s" XPCOM_FILE_PATH_SEPARATOR XPCOM_DLL, gXPCOMLocation);

    rv = XPCOMGlueStartup(xpcomPath);

    free(xpcomPath);

    const char* path = PR_GetEnv(XPCOM_SEARCH_KEY);
    if (!path) {
      path = "";
    }

    if (gPathEnvString)
      PR_smprintf_free(gPathEnvString);

    gPathEnvString = PR_smprintf("%s=%s;%s",
                                 XPCOM_SEARCH_KEY,
                                 gXPCOMLocation,
                                 path);

    if (gXPCOMLocation)
      PR_SetEnv(gPathEnvString);
  }
  else 
  {
    rv = XPCOMGlueStartup(nsnull);
  }

  if (NS_FAILED(rv)) 
  {
    printf("Can not initialize XPCOM Glue\n");
    return -1;
  }

  DirectoryServiceProvider *provider = new DirectoryServiceProvider();
  if ( !provider )
  {
    NS_WARNING("GRE_Startup failed");
    XPCOMGlueShutdown();
    return -1;
  }

  nsCOMPtr<nsILocalFile> file;
  if (gXPCOMLocation) 
  {
    rv = NS_NewNativeLocalFile(nsEmbedCString(gXPCOMLocation), 
                               PR_TRUE, 
                               getter_AddRefs(file));
  }

  NS_ADDREF(provider);
  rv = NS_InitXPCOM2(nsnull, file, provider);
  NS_RELEASE(provider);
    
  if (NS_FAILED(rv)) {
    printf("Can not initialize XPCOM\n");
    XPCOMGlueShutdown();
    return -1;
  }

  return 0;
}

void shutdown_xpcom()
{
  nsresult rv;

  rv = NS_ShutdownXPCOM(nsnull);

  if (NS_FAILED(rv)) {
    printf("Can not shutdown XPCOM cleanly\n");
  }

  rv = XPCOMGlueShutdown();
  
  if (NS_FAILED(rv)) {
    printf("Can not shutdown XPCOM Glue cleanly\n");
  }
  if (gPathEnvString)
    PR_smprintf_free(gPathEnvString);
}


nsresult Register(const char *path) 
{ 
  startup_xpcom();

  nsresult rv;
  nsCOMPtr<nsILocalFile> spec;
  
  if (path) {
    rv = NS_NewNativeLocalFile(nsEmbedCString(path), 
                               PR_TRUE, 
                               getter_AddRefs(spec));
  }

  nsCOMPtr<nsIComponentRegistrar> registrar;
  rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
  if (NS_FAILED(rv)) {
    printf("Can not aquire component registrar\n");
    return rv;
  }

  if (gUnreg)
    rv = registrar->AutoUnregister(spec);
  else
    rv = registrar->AutoRegister(spec);

  spec = 0;
  registrar = 0;

  shutdown_xpcom();
  return rv;
}


void ReportSuccess(const char *file)
{
  if (gQuiet)
    return;

  if (gUnreg)
    printf("Unregistration successful for %s\n", file);
  else
    printf("Registration successful for %s\n", file);
}

void ReportError(nsresult err, const char *file)
{
  if (gUnreg)
    printf("Unregistration failed: (");
  else
    printf("Registration failed: (");
  
  switch (err) 
  {
    case NS_ERROR_FACTORY_NOT_LOADED:
      printf("Factory not loaded");
      break;
    case NS_NOINTERFACE:
      printf("No Interface");
      break;
    case NS_ERROR_NULL_POINTER:
      printf("Null pointer");
      break;
    case NS_ERROR_OUT_OF_MEMORY:
      printf("Out of memory");
      break;
    default:
      printf("%x", (unsigned)err);
  }
  
  printf(") %s\n", file);
}

void printHelp()
{
  printf(
"Mozilla regxpcom - a registration tool for xpcom components                    \n"
"                                                                               \n"
"Usage: regxpcom [options] [file-or-directory]                                  \n"
"                                                                               \n"
"Options:                                                                       \n"
"         -x path        Specifies the location of a directory containing the   \n"
"                        xpcom library which will be used when registering new  \n"
"                        component libraries.  This path will also be added to  \n"
"                        the \"load library\" path.  If not specified, the      \n"
"                        current working directory will be used.                \n"
"         -c path        Specifies the location of the compreg.dat file.  If    \n"
"                        not specifed, the compreg.dat file will be in its      \n"
"                        default location.                                      \n"
"         -d path        Specifies the location of the xpti.dat file.  If not   \n"
"                        specifed, the xpti.dat file will be in its default     \n"
"                        location.                                              \n"
"         -a             Option to register all files in the default component  \n"
"                        directories.  This is the default behavior if regxpcom \n"
"                        is called without any arguments.                       \n"
"         -h             Displays this help screen.  Must be the only option    \n"
"                        specified.                                             \n"
"         -u             Option to uninstall the files-or-directory instead of  \n"
"                        registering them.                                      \n"
"         -q             Quiets some of the output of regxpcom.                 \n\n");
}

int ProcessArgs(int argc, char *argv[])
{
  int i = 1, result = 0;
  nsresult res;

  while (i < argc) 
  {
    if (argv[i][0] == '-') 
    {
      int j;
      for (j = 1; argv[i][j] != '\0'; j++) 
      {
        switch (argv[i][j]) 
        {
          case 'h':
            printHelp();
            return 0;  // we are all done!

          case 'u':
            gUnreg = PR_TRUE;
            break;

          case 'q':
            gQuiet = PR_TRUE;
            break;

          case 'a':
          {
            res = Register(nsnull);
            if (NS_FAILED(res)) 
            {
              ReportError(res, "component directory");
              result = -1;
            } 
            else 
            {
              ReportSuccess("component directory");
            }
          }
          break;

          case 'x':
            gXPCOMLocation = argv[++i];
            j = strlen(gXPCOMLocation) - 1;
            break;

          case 'c':
            gCompRegLocation = argv[++i];
            j = strlen(gCompRegLocation) - 1;
            break;

          case 'd':
            gXPTIDatLocation = argv[++i];
            j = strlen(gXPTIDatLocation) - 1;
            break;

          default:
            printf("Unknown option '%c'\n", argv[i][j]);
        }
      }
    } 
    else
    {
      res = Register(argv[i]);
      
      if (NS_FAILED(res)) 
      {
        ReportError(res, argv[i]);
        result = -1;
      } 
      else 
      {
        ReportSuccess(argv[i]);
      }
    }
    i++;
  }
  return result;
}


int main(int argc, char *argv[])
{
  int ret;
  nsresult rv;

  /* With no arguments, regxpcom will autoregister */
  if (argc <= 1)
  {
    startup_xpcom();
    nsCOMPtr<nsIComponentRegistrar> registrar;
    rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
    if (NS_FAILED(rv)) {
      printf("Can not aquire component registrar\n");
      return -1;
    }
    rv = registrar->AutoRegister(nsnull);
    ret = (NS_FAILED(rv)) ? -1 : 0;
    registrar = 0;
    shutdown_xpcom();
  } else
    ret = ProcessArgs(argc, argv);

  return ret;
}
