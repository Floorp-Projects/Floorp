/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsInternetConfigService.h"
#include "nsCOMPtr.h"
#include "nsIMIMEInfo.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsLocalFileMac.h"
#include "nsIURL.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"
#include <TextUtils.h>

/* Define Class IDs */
static NS_DEFINE_CID(kICServiceCID, NS_INTERNETCONFIGSERVICE_CID);

nsInternetConfigService::nsInternetConfigService()
{
  NS_INIT_REFCNT();
}

nsInternetConfigService::~nsInternetConfigService()
{
}


/*
 * Implement the nsISupports methods...
 */
NS_IMPL_THREADSAFE_ADDREF(nsInternetConfigService)
NS_IMPL_THREADSAFE_RELEASE(nsInternetConfigService)

NS_INTERFACE_MAP_BEGIN(nsInternetConfigService)
  NS_INTERFACE_MAP_ENTRY(nsIInternetConfigService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInternetConfigService)
NS_INTERFACE_MAP_END_THREADSAFE

// void LaunchURL (in string url);
// given a url string, call ICLaunchURL using url
NS_IMETHODIMP nsInternetConfigService::LaunchURL(const char *url)
{
  nsresult result;
  size_t len = strlen(url);
  long selStart = 0, selEnd = len;
  ICInstance inst = nsInternetConfig::GetInstance();
  
  if (inst)
  {
    result = ::ICLaunchURL(inst, "\p", (Ptr)url, (long)len, &selStart, &selEnd);
    if (result == noErr)
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }
  else
  {
    return NS_ERROR_FAILURE;
  }
}

// boolean HasMappingForMIMEType (in string mimetype);
// given a mime type, search Internet Config database for a mapping for that mime type
NS_IMETHODIMP nsInternetConfigService::HasMappingForMIMEType(const char *mimetype, PRBool *_retval)
{
  ICMapEntry entry;
  nsresult rv = GetMappingForMIMEType(mimetype, nsnull, &entry);
  if (rv == noErr)
    *_retval = PR_TRUE;
  else
    *_retval = PR_FALSE;
  return rv;
}

/* boolean hasProtocalHandler (in string protocol); */
NS_IMETHODIMP nsInternetConfigService::HasProtocalHandler(const char *protocol, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// This method does the dirty work of traipsing through IC mappings database
// looking for a mapping for mimetype
OSStatus nsInternetConfigService::GetMappingForMIMEType(const char *mimetype, const char *fileextension, ICMapEntry *entry)
{
  ICInstance inst = nsInternetConfig::GetInstance();
  OSStatus err = noErr;
  ICAttr attr;
  Handle prefH;
  PRBool domimecheck = PR_TRUE;
  
  if ((strcmp(mimetype, UNKNOWN_CONTENT_TYPE) == 0) ||
      (strcmp(mimetype, APPLICATION_OCTET_STREAM) == 0))
    domimecheck = PR_FALSE;
  
  entry->totalLength = 0;
  if (inst)
  {
    err = ::ICBegin(inst, icReadOnlyPerm);
    if (err == noErr)
    {
      prefH = ::NewHandle(2048); // picked 2048 out of thin air
      if (prefH)
      {
        err = ::ICFindPrefHandle(inst, kICMapping, &attr, prefH);
        if (err == noErr)
        {
          long count;
          err = ::ICCountMapEntries(inst, prefH, &count);
          if (err == noErr)
          {
            long pos;
            for(long i = 1; i <= count; i++)
            {
              err = ::ICGetIndMapEntry(inst, prefH, i, &pos, entry);
              if (err == noErr)
              {
                // first, do mime type check
                if (domimecheck)
                {
                  nsCString temp((char *)&entry->MIMEType[1], (int)entry->MIMEType[0]);
                  if (!temp.EqualsIgnoreCase(mimetype))
                  {
                    // we need to do mime check, and check failed
                    // nothing here to see, move along
                    continue;
                  }
                }
                if (fileextension)
                {
                  // if fileextension was passed in, compare that also
                  if (entry->extension[0] != 0)
                  {
                    nsCString temp((char *)&entry->extension[1], (int)entry->extension[0]);
                    if (temp.EqualsIgnoreCase(fileextension))
                    {
                      // mime type and file extension match, we're outta here
                      break;
                    }
                  }
                }
                else if(domimecheck)
                {
                  // at this point, we've got our match because
                  // domimecheck is true, the mime strings match, and fileextension isn't passed in
                  // bad thing is we'll stop on first match, but what can you do?
                  break;
                }
              }
            }
          }
        }
        ::DisposeHandle(prefH);
      }
      else
      {
        err = memFullErr;
      }
      err = ::ICEnd(inst);
    }
  }
  return err;
}

/* void FillInMIMEInfo (in string mimetype, in nsIURI uri, out nsIMIMEInfo mimeinfo); */
NS_IMETHODIMP nsInternetConfigService::FillInMIMEInfo(const char *mimetype, nsIURL *url, nsIMIMEInfo **mimeinfo)
{
  nsresult rv;
  ICMapEntry entry;
  nsCAutoString fileExtension;
  
  NS_ENSURE_ARG_POINTER(mimeinfo);
  NS_ENSURE_ARG_POINTER(url);
  *mimeinfo = nsnull;

  // if url is passed in, use it to get file extension  
  if (url)
  {
    nsXPIDLCString extension;
    url->GetFileExtension(getter_Copies(extension));

    fileExtension.Assign(".");  
    fileExtension.Append(extension);
    rv = GetMappingForMIMEType(mimetype, fileExtension, &entry);
  }
  else
  {
    rv = GetMappingForMIMEType(mimetype, nsnull, &entry);
  }
  if (rv == NS_OK)
  {
    // create a mime info object and we'll fill it in based on the values from IC mapping entry
    nsCOMPtr<nsIMIMEInfo> info (do_CreateInstance(NS_MIMEINFO_CONTRACTID));

    if (info)
    {
      info->SetMIMEType(mimetype);
      // convert entry.extension which is a Str255
      nsCString temp((char *)&entry.extension[1], (int)entry.extension[0]);
      info->AppendExtension(temp);
      info->SetMacType(entry.fileType);
      info->SetMacCreator(entry.fileCreator);

      if (entry.flags & kICMapPostMask)
      {
        // there is a post processor app
        info->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
        nsCID cid = NS_LOCAL_FILE_CID;
        nsCOMPtr<nsILocalFileMac> file (do_CreateInstance(cid));
        if (file)
        {
          rv = file->InitFindingAppByCreatorCode(entry.postCreator);
          if (rv == NS_OK)
          {
            //info->SetAlwaysAskBeforeHandling(PR_FALSE);
            nsCOMPtr<nsIFile> nsfile = do_QueryInterface(file, &rv);
            if (rv == NS_OK)
              info->SetPreferredApplicationHandler(nsfile);
          }
        }
      }
      *mimeinfo = info;
      NS_IF_ADDREF(*mimeinfo);
    }
    else
    {
      rv = NS_ERROR_FAILURE;
    }
  }
  else
  {
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

/* void GetDownloadFolder (out FSSpec fsspec); */
NS_IMETHODIMP nsInternetConfigService::GetDownloadFolder(FSSpec *fsspec)
{
  ICInstance inst = nsInternetConfig::GetInstance();
  OSStatus err;
  Handle prefH;
  nsresult rv = NS_ERROR_FAILURE;
  
  NS_ENSURE_ARG_POINTER(fsspec);
  
  if (inst)
  {
    err = ::ICBegin(inst, icReadOnlyPerm);
    if (err == noErr)
    {
      prefH = ::NewHandle(256); // ICFileSpec ~= 112 bytes + variable, 256 bytes hopefully is sufficient
      if (prefH)
      {
        ICAttr attr;
        err = ::ICFindPrefHandle(inst, kICDownloadFolder, &attr, prefH);
        if (err == noErr)
        {
          err = ::Munger(prefH, 0, NULL, kICFileSpecHeaderSize, (Ptr)-1, 0);
          if (err == noErr)
          {
            Boolean wasChanged;
            err = ::ResolveAlias(NULL, (AliasHandle)prefH, fsspec, &wasChanged);
            if (err == noErr)
            {
              rv = NS_OK;
            }
          }
        }
      }
      err = ::ICEnd(inst);
    }
  }
  return rv;
}

/* void getString (in string key, out string value); */
NS_IMETHODIMP nsInternetConfigService::GetString(const char *key, char **value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
