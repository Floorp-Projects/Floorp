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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Dagley <sdagley@netscape.com>
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

#include "nsInternetConfigService.h"
#include "nsCOMPtr.h"
#include "nsIMIMEInfo.h"
#include "nsMIMEInfoMac.h"
#include "nsAutoPtr.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsILocalFileMac.h"
#include "nsMimeTypes.h"
#include <TextUtils.h>
#include <CodeFragments.h>
#include <Processes.h>
#include <Gestalt.h>
#include <CFURL.h>
#include <Finder.h>
#include <LaunchServices.h>


// helper converter function.....
static void ConvertCharStringToStr255(const char* inString, Str255& outString)
{
  if (inString == NULL)
    return;
  
  PRInt32 len = strlen(inString);
  NS_ASSERTION(len <= 255 , " String is too big");
  if (len> 255)
  {
    len = 255;
  }
  memcpy(&outString[1], inString, len);
  outString[0] = len;
}

/* Define Class IDs */

nsInternetConfigService::nsInternetConfigService()
{
  long  version;
  OSErr err;
  mRunningOSX = ((err = ::Gestalt(gestaltSystemVersion, &version)) == noErr && version >= 0x00001000);
  mRunningJaguar = (err == noErr && version >= 0x00001020);
}

nsInternetConfigService::~nsInternetConfigService()
{
}


/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ISUPPORTS1(nsInternetConfigService, nsIInternetConfigService)

// void LaunchURL (in string url);
// Given a url string, call ICLaunchURL using url
// Under OS X use LaunchServices instead of IC
NS_IMETHODIMP nsInternetConfigService::LaunchURL(const char *url)
{
  nsresult rv = NS_ERROR_FAILURE;
  
#if TARGET_CARBON
  if (mRunningOSX && ((UInt32)LSOpenCFURLRef != (UInt32)kUnresolvedCFragSymbolAddress))
  {
    CFURLRef myURLRef = ::CFURLCreateWithBytes(
                                              kCFAllocatorDefault,
                                              (const UInt8*)url,
                                              strlen(url),
                                              kCFStringEncodingUTF8, NULL);
    if (myURLRef)
    {
      rv = ::LSOpenCFURLRef(myURLRef, NULL);
      ::CFRelease(myURLRef);
    }
  }
  else
#endif
  {
    size_t len = strlen(url);
    long selStart = 0, selEnd = len;
    ICInstance inst = nsInternetConfig::GetInstance();
    
    if (inst)
    {
      if (::ICLaunchURL(inst, "\p", (Ptr)url, (long)len, &selStart, &selEnd) == noErr)
        rv = NS_OK;
    }
  }
  return rv;
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
// returns NS_ERROR_NOT_AVAILABLE if the current application is registered for as the
// protocol handler for protocol
NS_IMETHODIMP nsInternetConfigService::HasProtocolHandler(const char *protocol, PRBool *_retval)
{
  *_retval = PR_FALSE;            // Presume failure
  nsresult rv = NS_ERROR_FAILURE; // Ditto
  
#if TARGET_CARBON
  // Use LaunchServices directly when we're running under OS X to avoid the problem of some protocols
  // apparently not being reflected into the IC mappings (webcal for one).  Even better it seems
  // LaunchServices under 10.1.x will often fail to find an app when using LSGetApplicationForURL
  // so we only use it for 10.2 or later.
  
  // Since protocol comes in with _just_ the protocol we have to add a ':' to the end of it or
  // LaunchServices will be very unhappy with the CFURLRef created from it (crashes trying to look
  // up a handler for it with LSGetApplicationForURL, at least under 10.2.1)
  if (mRunningJaguar)
  {
    nsCAutoString scheme(protocol);
    scheme += ":";
    CFURLRef myURLRef = ::CFURLCreateWithBytes(
                                                kCFAllocatorDefault,
                                                (const UInt8 *)scheme.get(),
                                                scheme.Length(),
                                                kCFStringEncodingUTF8, NULL);
    if (myURLRef)
    {
      FSRef appFSRef;
      
      if (::LSGetApplicationForURL(myURLRef, kLSRolesAll, &appFSRef, NULL) == noErr)
      { // Now see if the FSRef for the found app == the running app
        ProcessSerialNumber psn;
        if (::GetCurrentProcess(&psn) == noErr)
        {
          FSRef runningAppFSRef;
          if (::GetProcessBundleLocation(&psn, &runningAppFSRef) == noErr)
          {
            if (::FSCompareFSRefs(&appFSRef, &runningAppFSRef) == noErr)
            { // Oops, the current app is the handler which would cause infinite recursion
              rv = NS_ERROR_NOT_AVAILABLE;
            }
            else
            {
              *_retval = PR_TRUE;
              rv = NS_OK;
            }
          }
        }
      }
      ::CFRelease(myURLRef);
    }
  }
  else
#endif
  {
    // look for IC pref "\pHelper¥<protocol>"
    Str255 pref = kICHelper;

    if (nsCRT::strlen(protocol) > 248)
      rv = NS_ERROR_OUT_OF_MEMORY;
    else
    {
      memcpy(pref + pref[0] + 1, protocol, nsCRT::strlen(protocol));
      pref[0] = pref[0] + nsCRT::strlen(protocol);
      
      ICInstance instance = nsInternetConfig::GetInstance();
      if (instance)
      {
        OSStatus  err;
        ICAttr    junk;
        ICAppSpec spec;
        long      ioSize = sizeof(ICAppSpec);
        err = ::ICGetPref(instance, pref, &junk, (void *)&spec, &ioSize);
      
        if (err == noErr)
        {
          // check if registered protocol helper is us
          // if so, return PR_FALSE because we'll go into infinite recursion
          // continually launching back into ourselves
          ProcessSerialNumber psn;
          OSErr oserr = ::GetCurrentProcess(&psn);
          if (oserr == noErr)
          {
            ProcessInfoRec info;
            info.processInfoLength = sizeof(ProcessInfoRec);
            info.processName = nsnull;
            info.processAppSpec = nsnull;
            err = ::GetProcessInformation(&psn, &info);
            if (err == noErr)
            {
              if (info.processSignature != spec.fCreator)
              {
                *_retval = PR_TRUE;
                rv = NS_OK;
              }
              else
                rv = NS_ERROR_NOT_AVAILABLE;
            }
          }
        }
      }
    }
  }
  return rv;
}

// This method does the dirty work of traipsing through IC mappings database
// looking for a mapping for mimetype
nsresult nsInternetConfigService::GetMappingForMIMEType(const char *mimetype, const char *fileextension, ICMapEntry *entry)
{
  ICInstance  inst = nsInternetConfig::GetInstance();
  OSStatus    err = noErr;
  ICAttr      attr;
  Handle      prefH;
  PRBool      domimecheck = PR_TRUE;
  PRBool      gotmatch = PR_FALSE;
  ICMapEntry  ent;
  
  // if mime type is "unknown" or "octet stream" *AND* we have a file extension,
  // then disable match on mime type
  if (((nsCRT::strcasecmp(mimetype, UNKNOWN_CONTENT_TYPE) == 0) ||
       (nsCRT::strcasecmp(mimetype, APPLICATION_OCTET_STREAM) == 0)) &&
       fileextension)
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
            for (long i = 1; i <= count; ++i)
            {
              err = ::ICGetIndMapEntry(inst, prefH, i, &pos, &ent);
              if (err == noErr)
              {
                // first, do mime type check
                if (domimecheck)
                {
                  nsCAutoString temp((char *)&ent.MIMEType[1], (int)ent.MIMEType[0]);
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
                  if (ent.extension[0]) // check for non-empty pascal string
                  {
                    nsCAutoString temp((char *)&ent.extension[1], (int)ent.extension[0]);
                    if (temp.EqualsIgnoreCase(fileextension))
                    {
                      // mime type and file extension match, we're outta here
                      gotmatch = PR_TRUE;
                      // copy over ICMapEntry
                      *entry = ent;
                      break;
                    }
                  }
                }
                else if(domimecheck)
                {
                  // at this point, we've got our match because
                  // domimecheck is true, the mime strings match, and fileextension isn't passed in
                  // bad thing is we'll stop on first match, but what can you do?
                  gotmatch = PR_TRUE;
                  // copy over ICMapEntry
                  *entry = ent;
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
      if (err == noErr && gotmatch == PR_FALSE)
      {
        err = fnfErr; // return SOME kind of error
      }
    }
  }
  
  if (err != noErr)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

nsresult nsInternetConfigService::FillMIMEInfoForICEntry(ICMapEntry& entry, nsIMIMEInfo ** mimeinfo)
{
  // create a mime info object and we'll fill it in based on the values from IC mapping entry
  nsresult  rv = NS_OK;
  nsRefPtr<nsMIMEInfoMac> info (new nsMIMEInfoMac());
  if (info)
  {
    nsCAutoString mimetype ((char *)&entry.MIMEType[1], entry.MIMEType[0]);
    // check if entry.MIMEType is empty, if so, set mime type to APPLICATION_OCTET_STREAM
    if (entry.MIMEType[0])
      info->SetMIMEType(mimetype);
    else
    { // The IC mappings seem to not be very agressive about determining the mime type if
      // all we have is a type or creator code.  This is a bandaid approach for when we
      // get a file of type 'TEXT' with no mime type mapping so that we'll display the
      // file rather than trying to download it.
      if (entry.fileType == 'TEXT')
        info->SetMIMEType(NS_LITERAL_CSTRING(TEXT_PLAIN));
      else
        info->SetMIMEType(NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM));
    }
    
    // convert entry.extension which is a Str255 
    // don't forget to remove the '.' in front of the file extension....
    nsCAutoString temp((char *)&entry.extension[2], entry.extension[0] > 0 ? (int)entry.extension[0]-1 : 0);
    info->AppendExtension(temp);
    info->SetMacType(entry.fileType);
    info->SetMacCreator(entry.fileCreator);
    temp.Assign((char *) &entry.entryName[1], entry.entryName[0]);
    info->SetDescription(NS_ConvertASCIItoUCS2(temp));
    
    temp.Assign((char *) &entry.postAppName[1], entry.postAppName[0]);
    info->SetDefaultDescription(NS_ConvertASCIItoUCS2(temp));
    
    if (entry.flags & kICMapPostMask)
    {
      // there is a post processor app
      info->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
      nsCOMPtr<nsILocalFileMac> file (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
      if (file)
      {
        rv = file->InitToAppWithCreatorCode(entry.postCreator);
        if (rv == NS_OK)
        {
          nsCOMPtr<nsIFile> nsfile = do_QueryInterface(file, &rv);
          if (rv == NS_OK)
            info->SetDefaultApplication(nsfile);
        }
      }
    }
    else
    {
      // there isn't a post processor app so set the preferred action to be save to disk.
      info->SetPreferredAction(nsIMIMEInfo::saveToDisk);
    }
    
    *mimeinfo = info;
    NS_IF_ADDREF(*mimeinfo);
  }
  else // we failed to allocate the info object...
    rv = NS_ERROR_FAILURE;
   
  return rv;
}

/* void FillInMIMEInfo (in string mimetype, in string fileExtension, out nsIMIMEInfo mimeinfo); */
NS_IMETHODIMP nsInternetConfigService::FillInMIMEInfo(const char *mimetype, const char * aFileExtension, nsIMIMEInfo **mimeinfo)
{
  nsresult    rv;
  ICMapEntry  entry;
  
  NS_ENSURE_ARG_POINTER(mimeinfo);
  *mimeinfo = nsnull;

  if (aFileExtension)
  {
    nsCAutoString fileExtension;
    fileExtension.Assign(".");  
    fileExtension.Append(aFileExtension);
    rv = GetMappingForMIMEType(mimetype, fileExtension.get(), &entry);
  }
  else
  {
    rv = GetMappingForMIMEType(mimetype, nsnull, &entry);
  }
  
  if (rv == NS_OK)
    rv = FillMIMEInfoForICEntry(entry, mimeinfo);
  else
    rv = NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP nsInternetConfigService::GetMIMEInfoFromExtension(const char *aFileExt, nsIMIMEInfo **_retval)
{
  nsresult    rv = NS_ERROR_FAILURE;
  ICInstance  instance = nsInternetConfig::GetInstance();
  if (instance)
  {
    nsCAutoString filename("foobar.");
    filename += aFileExt;
    Str255  pFileName;
    ConvertCharStringToStr255(filename.get(), pFileName);
    ICMapEntry  entry;
    OSStatus  err = ::ICMapFilename(instance, pFileName, &entry);
    if (err == noErr)
    {
      rv = FillMIMEInfoForICEntry(entry, _retval);
    }
  }   
  return rv;
}


NS_IMETHODIMP nsInternetConfigService::GetMIMEInfoFromTypeCreator(PRUint32 aType, PRUint32 aCreator, const char *aFileExt, nsIMIMEInfo **_retval)
{
  nsresult    rv = NS_ERROR_FAILURE;
  ICInstance  instance = nsInternetConfig::GetInstance();
  if (instance)
  {
    nsCAutoString filename("foobar.");
    filename += aFileExt;
    Str255  pFileName;
    ConvertCharStringToStr255(filename.get(), pFileName);
    ICMapEntry  entry;
    OSStatus  err = ::ICMapTypeCreator(instance, aType, aCreator, pFileName, &entry);
    if (err == noErr)
      rv = FillMIMEInfoForICEntry(entry,_retval);
  }
  return rv;
}


NS_IMETHODIMP nsInternetConfigService::GetFileMappingFlags(FSSpec* fsspec, PRBool lookupByExtensionFirst, PRInt32 *_retval)
{
  nsresult  rv = NS_ERROR_FAILURE;
  OSStatus  err = noErr;

  NS_ENSURE_ARG(_retval);
  *_retval = -1;
  
  ICInstance instance = nsInternetConfig::GetInstance();
  if (instance)
  {
    ICMapEntry  entry;
    
    if (lookupByExtensionFirst)
      err = ::ICMapFilename(instance, fsspec->name, &entry);
  
    if (!lookupByExtensionFirst || err != noErr)
    {
      FInfo info;
      err = FSpGetFInfo(fsspec, &info);
      if (err == noErr)
        err = ::ICMapTypeCreator(instance, info.fdType, info.fdCreator, fsspec->name, &entry);
    }

    if (err == noErr)
      *_retval = entry.flags;
     
     rv = NS_OK;
  }
  return rv;
}


/* void GetDownloadFolder (out FSSpec fsspec); */
NS_IMETHODIMP nsInternetConfigService::GetDownloadFolder(FSSpec *fsspec)
{
  ICInstance  inst = nsInternetConfig::GetInstance();
  OSStatus    err;
  Handle      prefH;
  nsresult    rv = NS_ERROR_FAILURE;
  
  NS_ENSURE_ARG_POINTER(fsspec);
  
  if (inst)
  {
    err = ::ICBegin(inst, icReadOnlyPerm);
    if (err == noErr)
    {
      prefH = ::NewHandle(256); // ICFileSpec ~= 112 bytes + variable, 256 bytes hopefully is sufficient
      if (prefH)
      {
        ICAttr  attr;
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
            else
            { // ResolveAlias for the DownloadFolder failed - try grabbing the FSSpec
              err = ::ICFindPrefHandle(inst, kICDownloadFolder, &attr, prefH);
              if (err == noErr)
              { // Use FSMakeFSSpec to verify the saved FSSpec is still valid
                FSSpec  tempSpec = (*(ICFileSpecHandle)prefH)->fss;
                err = ::FSMakeFSSpec(tempSpec.vRefNum, tempSpec.parID, tempSpec.name, fsspec);
                if (err == noErr)
                  rv = NS_OK;
              }
            }
          }
        }
        // Best not to leave that handle laying around
        DisposeHandle(prefH);
      }
      err = ::ICEnd(inst);
    }
  }
  return rv;
}

nsresult nsInternetConfigService::GetICKeyPascalString(PRUint32 inIndex, const unsigned char*& outICKey)
{
  nsresult  rv = NS_OK;

  switch (inIndex)
  {
    case eICColor_WebBackgroundColour: outICKey = kICWebBackgroundColour; break;
    case eICColor_WebReadColor:        outICKey = kICWebReadColor;        break;
    case eICColor_WebTextColor:        outICKey = kICWebTextColor;        break;
    case eICColor_WebUnreadColor:      outICKey = kICWebUnreadColor;      break;

    case eICBoolean_WebUnderlineLinks: outICKey = kICWebUnderlineLinks;  break;
    case eICBoolean_UseFTPProxy:       outICKey = kICUseFTPProxy;        break;
    case eICBoolean_UsePassiveFTP:     outICKey = kICUsePassiveFTP;      break;
    case eICBoolean_UseHTTPProxy:      outICKey = kICUseHTTPProxy;       break;
    case eICBoolean_NewMailDialog:     outICKey = kICNewMailDialog;      break;
    case eICBoolean_NewMailFlashIcon:  outICKey = kICNewMailFlashIcon;   break;
    case eICBoolean_NewMailPlaySound:  outICKey = kICNewMailPlaySound;   break;
    case eICBoolean_UseGopherProxy:    outICKey = kICUseGopherProxy;     break;
    case eICBoolean_UseSocks:          outICKey = kICUseSocks;           break;

    case eICString_WWWHomePage:        outICKey = kICWWWHomePage;        break;
    case eICString_WebSearchPagePrefs: outICKey = kICWebSearchPagePrefs; break;
    case eICString_MacSearchHost:      outICKey = kICMacSearchHost;      break;
    case eICString_FTPHost:            outICKey = kICFTPHost;            break;
    case eICString_FTPProxyUser:       outICKey = kICFTPProxyUser;       break;
    case eICString_FTPProxyAccount:    outICKey = kICFTPProxyAccount;    break;
    case eICString_FTPProxyHost:       outICKey = kICFTPProxyHost;       break;
    case eICString_FTPProxyPassword:   outICKey = kICFTPProxyPassword;   break;
    case eICString_HTTPProxyHost:      outICKey = kICHTTPProxyHost;      break;
    case eICString_LDAPSearchbase:     outICKey = kICLDAPSearchbase;     break;
    case eICString_LDAPServer:         outICKey = kICLDAPServer;         break;
    case eICString_SMTPHost:           outICKey = kICSMTPHost;           break;
    case eICString_Email:              outICKey = kICEmail;              break;
    case eICString_MailAccount:        outICKey = kICMailAccount;        break;
    case eICString_MailPassword:       outICKey = kICMailPassword;       break;
    case eICString_NewMailSoundName:   outICKey = kICNewMailSoundName;   break;
    case eICString_NNTPHost:           outICKey = kICNNTPHost;           break;
    case eICString_NewsAuthUsername:   outICKey = kICNewsAuthUsername;   break;
    case eICString_NewsAuthPassword:   outICKey = kICNewsAuthPassword;   break;
    case eICString_InfoMacPreferred:   outICKey = kICInfoMacPreferred;   break;
    case eICString_Organization:       outICKey = kICOrganization;       break;
    case eICString_QuotingString:      outICKey = kICQuotingString;      break;
    case eICString_RealName:           outICKey = kICRealName;           break;
    case eICString_FingerHost:         outICKey = kICFingerHost;         break;
    case eICString_GopherHost:         outICKey = kICGopherHost;         break;
    case eICString_GopherProxy:        outICKey = kICGopherProxy;        break;
    case eICString_SocksHost:          outICKey = kICSocksHost;          break;
    case eICString_TelnetHost:         outICKey = kICTelnetHost;         break;
    case eICString_IRCHost:            outICKey = kICIRCHost;            break;
    case eICString_UMichPreferred:     outICKey = kICUMichPreferred;     break;
    case eICString_WAISGateway:        outICKey = kICWAISGateway;        break;
    case eICString_WhoisHost:          outICKey = kICWhoisHost;          break;
    case eICString_PhHost:             outICKey = kICPhHost;             break;
    case eICString_NTPHost:            outICKey = kICNTPHost;            break;
    case eICString_ArchiePreferred:    outICKey = kICArchiePreferred;    break;
    
    case eICText_MailHeaders:          outICKey = kICMailHeaders;        break;
    case eICText_Signature:            outICKey = kICSignature;          break;
    case eICText_NewsHeaders:          outICKey = kICNewsHeaders;        break;
    case eICText_SnailMailAddress:     outICKey = kICSnailMailAddress;   break;
    case eICText_Plan:                 outICKey = kICPlan;               break;

    default:
      rv = NS_ERROR_INVALID_ARG;
  }
  return rv;
}


nsresult nsInternetConfigService::GetICPreference(PRUint32 inKey, 
                                                  void *outData, long *ioSize)
{
  const unsigned char *icKey;
  nsresult  rv = GetICKeyPascalString(inKey, icKey);
  if (rv == NS_OK)
  {
    ICInstance  instance = nsInternetConfig::GetInstance();
    if (instance)
    {
      OSStatus  err;
      ICAttr    junk;
      err = ::ICGetPref(instance, icKey, &junk, outData, ioSize);
      if (err != noErr)
        rv = NS_ERROR_UNEXPECTED;
    }
    else
      rv = NS_ERROR_FAILURE;
  }
  return rv;
}


NS_IMETHODIMP nsInternetConfigService::GetString(PRUint32 inKey, nsACString& value)
{
  long      size = 256;
  char      buffer[256];
  nsresult  rv = GetICPreference(inKey, (void *)&buffer, &size);
  if (rv == NS_OK)
  {
    if (size == 0)
    {
      value = "";
      rv = NS_ERROR_UNEXPECTED;
    }
    else
    { // Buffer is a Pascal string so adjust for length byte when assigning
      value.Assign(&buffer[1], (unsigned char)buffer[0]);
    }
  }
  return rv;
}


NS_IMETHODIMP nsInternetConfigService::GetColor(PRUint32 inKey, PRUint32 *outColor)
{
// We're 'borrowing' this macro from nscolor.h so that uriloader doesn't depend on gfx.
// Make a color out of r,g,b values. This assumes that the r,g,b values are
// properly constrained to 0-255. This also assumes that a is 255.

  #define MAKE_NS_RGB(_r,_g,_b) \
    ((PRUint32) ((255 << 24) | ((_b)<<16) | ((_g)<<8) | (_r)))

  RGBColor  buffer;
  long      size = sizeof(RGBColor);
  nsresult  rv = GetICPreference(inKey, &buffer, &size);
  if (rv == NS_OK)
  {
    if (size != sizeof(RGBColor))
    { // default to white if we didn't get the right size
      *outColor = MAKE_NS_RGB(0xff, 0xff, 0xff);
    }
    else
    { // convert to a web color
      *outColor = MAKE_NS_RGB(buffer.red >> 8, buffer.green >> 8, buffer.blue >> 8);
    }
  }
  return rv;
}


NS_IMETHODIMP nsInternetConfigService::GetBoolean(PRUint32 inKey, PRBool *outFlag)
{
  Boolean   buffer;
  long      size = sizeof(Boolean);
  nsresult  rv = GetICPreference(inKey, (void *)&buffer, &size);
  if (rv == NS_OK)
  {
    if ((size_t)size < sizeof(Boolean))
      *outFlag = PR_FALSE;  // default to false if we didn't get the right amount of data
    else
      *outFlag = buffer;
  }
  return rv;
}
