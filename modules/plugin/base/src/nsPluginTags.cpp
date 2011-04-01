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
 *   Sean Echevarria <sean@beatnik.com>
 *   Håkan Waara <hwaara@chello.se>
 *   Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsPluginTags.h"

#include "prlink.h"
#include "plstr.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIDocument.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsPluginsDir.h"
#include "nsPluginHost.h"
#include "nsIUnicodeDecoder.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsPluginLogging.h"
#include "nsICategoryManager.h"
#include "nsNPAPIPlugin.h"
#include "mozilla/TimeStamp.h"

using mozilla::TimeStamp;

inline char* new_str(const char* str)
{
  if (str == nsnull)
    return nsnull;
  
  char* result = new char[strlen(str) + 1];
  if (result != nsnull)
    return strcpy(result, str);
  return result;
}

/* nsPluginTag */

nsPluginTag::nsPluginTag(nsPluginTag* aPluginTag)
: mPluginHost(nsnull),
mName(aPluginTag->mName),
mDescription(aPluginTag->mDescription),
mVariants(aPluginTag->mVariants),
mMimeTypeArray(nsnull),
mMimeDescriptionArray(aPluginTag->mMimeDescriptionArray),
mExtensionsArray(nsnull),
mLibrary(nsnull),
mCanUnloadLibrary(PR_TRUE),
mIsJavaPlugin(aPluginTag->mIsJavaPlugin),
mIsNPRuntimeEnabledJavaPlugin(aPluginTag->mIsNPRuntimeEnabledJavaPlugin),
mIsFlashPlugin(aPluginTag->mIsFlashPlugin),
mFileName(aPluginTag->mFileName),
mFullPath(aPluginTag->mFullPath),
mVersion(aPluginTag->mVersion),
mLastModifiedTime(0),
mFlags(NS_PLUGIN_FLAG_ENABLED)
{
  if (aPluginTag->mMimeTypeArray != nsnull) {
    mMimeTypeArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++)
      mMimeTypeArray[i] = new_str(aPluginTag->mMimeTypeArray[i]);
  }
  
  if (aPluginTag->mExtensionsArray != nsnull) {
    mExtensionsArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++)
      mExtensionsArray[i] = new_str(aPluginTag->mExtensionsArray[i]);
  }
}

nsPluginTag::nsPluginTag(nsPluginInfo* aPluginInfo)
: mPluginHost(nsnull),
mName(aPluginInfo->fName),
mDescription(aPluginInfo->fDescription),
mVariants(aPluginInfo->fVariantCount),
mMimeTypeArray(nsnull),
mExtensionsArray(nsnull),
mLibrary(nsnull),
#ifdef XP_MACOSX
mCanUnloadLibrary(!aPluginInfo->fBundle),
#else
mCanUnloadLibrary(PR_TRUE),
#endif
mIsJavaPlugin(PR_FALSE),
mIsNPRuntimeEnabledJavaPlugin(PR_FALSE),
mIsFlashPlugin(PR_FALSE),
mFileName(aPluginInfo->fFileName),
mFullPath(aPluginInfo->fFullPath),
mVersion(aPluginInfo->fVersion),
mLastModifiedTime(0),
mFlags(NS_PLUGIN_FLAG_ENABLED)
{
  PRInt32 javaSentinelVariant = -1;

  if (aPluginInfo->fMimeTypeArray) {
    mMimeTypeArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++) {
      char* currentMIMEType = aPluginInfo->fMimeTypeArray[i];
      if (!currentMIMEType) {
        continue;
      }

      if (mIsJavaPlugin) {
        if (strcmp(currentMIMEType, "application/x-java-vm-npruntime") == 0) {
          // This "magic MIME type" should not be exposed, but is just a signal
          // to the browser that this is new-style java.
          // Remove it and its associated MIME description from our arrays.
          mIsNPRuntimeEnabledJavaPlugin = PR_TRUE;
          javaSentinelVariant = i;
        }
      }

      mMimeTypeArray[i] = new_str(currentMIMEType);

      if (nsPluginHost::IsJavaMIMEType(mMimeTypeArray[i])) {
        mIsJavaPlugin = PR_TRUE;
      }
      else if (strcmp(currentMIMEType, "application/x-shockwave-flash") == 0) {
        mIsFlashPlugin = PR_TRUE;
      }
    }
  }

  if (aPluginInfo->fMimeDescriptionArray) {
    for (int i = 0; i < mVariants; i++) {
      // we should cut off the list of suffixes which the mime
      // description string may have, see bug 53895
      // it is usually in form "some description (*.sf1, *.sf2)"
      // so we can search for the opening round bracket
      char cur = '\0';
      char pre = '\0';
      char * p = PL_strrchr(aPluginInfo->fMimeDescriptionArray[i], '(');
      if (p && (p != aPluginInfo->fMimeDescriptionArray[i])) {
        if ((p - 1) && *(p - 1) == ' ') {
          pre = *(p - 1);
          *(p - 1) = '\0';
        } else {
          cur = *p;
          *p = '\0';
        }
        
      }
      mMimeDescriptionArray.AppendElement(
                                          aPluginInfo->fMimeDescriptionArray[i]);
      // restore the original string
      if (cur != '\0')
        *p = cur;
      if (pre != '\0')
        *(p - 1) = pre;
    }
  } else {
    mMimeDescriptionArray.SetLength(mVariants);
  }
  
  if (aPluginInfo->fExtensionArray != nsnull) {
    mExtensionsArray = new char*[mVariants];
    for (int i = 0; i < mVariants; i++)
      mExtensionsArray[i] = new_str(aPluginInfo->fExtensionArray[i]);
  }
  
  RemoveJavaSentinel(javaSentinelVariant);

  EnsureMembersAreUTF8();
}

nsPluginTag::nsPluginTag(const char* aName,
                         const char* aDescription,
                         const char* aFileName,
                         const char* aFullPath,
                         const char* aVersion,
                         const char* const* aMimeTypes,
                         const char* const* aMimeDescriptions,
                         const char* const* aExtensions,
                         PRInt32 aVariants,
                         PRInt64 aLastModifiedTime,
                         PRBool aCanUnload,
                         PRBool aArgsAreUTF8)
: mPluginHost(nsnull),
mName(aName),
mDescription(aDescription),
mVariants(aVariants),
mMimeTypeArray(nsnull),
mExtensionsArray(nsnull),
mLibrary(nsnull),
mCanUnloadLibrary(aCanUnload),
mIsJavaPlugin(PR_FALSE),
mIsNPRuntimeEnabledJavaPlugin(PR_FALSE),
mFileName(aFileName),
mFullPath(aFullPath),
mVersion(aVersion),
mLastModifiedTime(aLastModifiedTime),
mFlags(0) // Caller will read in our flags from cache
{
  PRInt32 javaSentinelVariant = -1;

  if (aVariants) {
    mMimeTypeArray        = new char*[mVariants];
    mExtensionsArray      = new char*[mVariants];
    
    for (PRInt32 i = 0; i < aVariants; ++i) {
      if (mIsJavaPlugin && aMimeTypes[i] &&
          strcmp(aMimeTypes[i], "application/x-java-vm-npruntime") == 0) {
        mIsNPRuntimeEnabledJavaPlugin = PR_TRUE;
        javaSentinelVariant = i;
      }
      
      mMimeTypeArray[i]        = new_str(aMimeTypes[i]);
      mMimeDescriptionArray.AppendElement(aMimeDescriptions[i]);
      mExtensionsArray[i]      = new_str(aExtensions[i]);
      if (nsPluginHost::IsJavaMIMEType(mMimeTypeArray[i]))
        mIsJavaPlugin = PR_TRUE;
    }
  }

  RemoveJavaSentinel(javaSentinelVariant);

  if (!aArgsAreUTF8)
    EnsureMembersAreUTF8();
}

nsPluginTag::~nsPluginTag()
{
  NS_ASSERTION(!mNext, "Risk of exhausting the stack space, bug 486349");
  
  if (mMimeTypeArray) {
    for (int i = 0; i < mVariants; i++)
      delete[] mMimeTypeArray[i];
    
    delete[] (mMimeTypeArray);
    mMimeTypeArray = nsnull;
  }
  
  if (mExtensionsArray) {
    for (int i = 0; i < mVariants; i++)
      delete[] mExtensionsArray[i];
    
    delete[] (mExtensionsArray);
    mExtensionsArray = nsnull;
  }
}

NS_IMPL_ISUPPORTS1(nsPluginTag, nsIPluginTag)

static nsresult ConvertToUTF8(nsIUnicodeDecoder *aUnicodeDecoder,
                              nsAFlatCString& aString)
{
  PRInt32 numberOfBytes = aString.Length();
  PRInt32 outUnicodeLen;
  nsAutoString buffer;
  nsresult rv = aUnicodeDecoder->GetMaxLength(aString.get(), numberOfBytes,
                                              &outUnicodeLen);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!EnsureStringLength(buffer, outUnicodeLen))
    return NS_ERROR_OUT_OF_MEMORY;
  rv = aUnicodeDecoder->Convert(aString.get(), &numberOfBytes,
                                buffer.BeginWriting(), &outUnicodeLen);
  NS_ENSURE_SUCCESS(rv, rv);
  buffer.SetLength(outUnicodeLen);
  CopyUTF16toUTF8(buffer, aString);
  
  return NS_OK;
}

nsresult nsPluginTag::EnsureMembersAreUTF8()
{
#if defined(XP_WIN) || defined(XP_MACOSX)
  return NS_OK;
#else
  nsresult rv;
  
  nsCOMPtr<nsIPlatformCharset> pcs =
  do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIUnicodeDecoder> decoder;
  nsCOMPtr<nsICharsetConverterManager> ccm =
  do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCAutoString charset;
  rv = pcs->GetCharset(kPlatformCharsetSel_FileName, charset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!charset.LowerCaseEqualsLiteral("utf-8")) {
    rv = ccm->GetUnicodeDecoderRaw(charset.get(), getter_AddRefs(decoder));
    NS_ENSURE_SUCCESS(rv, rv);
    
    ConvertToUTF8(decoder, mFileName);
    ConvertToUTF8(decoder, mFullPath);
  }
  
  // The description of the plug-in and the various MIME type descriptions
  // should be encoded in the standard plain text file encoding for this system.
  // XXX should we add kPlatformCharsetSel_PluginResource?
  rv = pcs->GetCharset(kPlatformCharsetSel_PlainTextInFile, charset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!charset.LowerCaseEqualsLiteral("utf-8")) {
    rv = ccm->GetUnicodeDecoderRaw(charset.get(), getter_AddRefs(decoder));
    NS_ENSURE_SUCCESS(rv, rv);
    
    ConvertToUTF8(decoder, mName);
    ConvertToUTF8(decoder, mDescription);
    for (PRUint32 i = 0; i < mMimeDescriptionArray.Length(); ++i) {
      ConvertToUTF8(decoder, mMimeDescriptionArray[i]);
    }
  }
  return NS_OK;
#endif
}

void nsPluginTag::SetHost(nsPluginHost * aHost)
{
  mPluginHost = aHost;
}

NS_IMETHODIMP
nsPluginTag::GetDescription(nsACString& aDescription)
{
  aDescription = mDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetFilename(nsACString& aFileName)
{
  aFileName = mFileName;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetFullpath(nsACString& aFullPath)
{
  aFullPath = mFullPath;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetVersion(nsACString& aVersion)
{
  aVersion = mVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetName(nsACString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetDisabled(PRBool* aDisabled)
{
  *aDisabled = !HasFlag(NS_PLUGIN_FLAG_ENABLED);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::SetDisabled(PRBool aDisabled)
{
  if (HasFlag(NS_PLUGIN_FLAG_ENABLED) == !aDisabled)
    return NS_OK;
  
  if (aDisabled)
    UnMark(NS_PLUGIN_FLAG_ENABLED);
  else
    Mark(NS_PLUGIN_FLAG_ENABLED);
  
  mPluginHost->UpdatePluginInfo(this);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetBlocklisted(PRBool* aBlocklisted)
{
  *aBlocklisted = HasFlag(NS_PLUGIN_FLAG_BLOCKLISTED);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::SetBlocklisted(PRBool aBlocklisted)
{
  if (HasFlag(NS_PLUGIN_FLAG_BLOCKLISTED) == aBlocklisted)
    return NS_OK;
  
  if (aBlocklisted)
    Mark(NS_PLUGIN_FLAG_BLOCKLISTED);
  else
    UnMark(NS_PLUGIN_FLAG_BLOCKLISTED);
  
  mPluginHost->UpdatePluginInfo(nsnull);
  return NS_OK;
}

void
nsPluginTag::RegisterWithCategoryManager(PRBool aOverrideInternalTypes,
                                         nsPluginTag::nsRegisterType aType)
{
  if (!mMimeTypeArray)
    return;
  
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
             ("nsPluginTag::RegisterWithCategoryManager plugin=%s, removing = %s\n",
              mFileName.get(), aType == ePluginUnregister ? "yes" : "no"));
  
  nsCOMPtr<nsICategoryManager> catMan = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan)
    return;
  
  const char *contractId = "@mozilla.org/content/plugin/document-loader-factory;1";
  
  nsCOMPtr<nsIPrefBranch> psvc(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!psvc)
    return; // NS_ERROR_OUT_OF_MEMORY
  
  // A preference controls whether or not the full page plugin is disabled for
  // a particular type. The string must be in the form:
  //   type1,type2,type3,type4
  // Note: need an actual interface to control this and subsequent disabling 
  // (and other plugin host settings) so applications can reliably disable 
  // plugins - without relying on implementation details such as prefs/category
  // manager entries.
  nsXPIDLCString overrideTypes;
  nsCAutoString overrideTypesFormatted;
  if (aType != ePluginUnregister) {
    psvc->GetCharPref("plugin.disable_full_page_plugin_for_types", getter_Copies(overrideTypes));
    overrideTypesFormatted.Assign(',');
    overrideTypesFormatted += overrideTypes;
    overrideTypesFormatted.Append(',');
  }
  
  nsACString::const_iterator start, end;
  for (int i = 0; i < mVariants; i++) {
    if (aType == ePluginUnregister) {
      nsXPIDLCString value;
      if (NS_SUCCEEDED(catMan->GetCategoryEntry("Gecko-Content-Viewers",
                                                mMimeTypeArray[i],
                                                getter_Copies(value)))) {
        // Only delete the entry if a plugin registered for it
        if (strcmp(value, contractId) == 0) {
          catMan->DeleteCategoryEntry("Gecko-Content-Viewers",
                                      mMimeTypeArray[i],
                                      PR_TRUE);
        }
      }
    } else {
      overrideTypesFormatted.BeginReading(start);
      overrideTypesFormatted.EndReading(end);
      
      nsDependentCString mimeType(mMimeTypeArray[i]);
      nsCAutoString commaSeparated; 
      commaSeparated.Assign(',');
      commaSeparated += mimeType;
      commaSeparated.Append(',');
      if (!FindInReadable(commaSeparated, start, end)) {
        catMan->AddCategoryEntry("Gecko-Content-Viewers",
                                 mMimeTypeArray[i],
                                 contractId,
                                 PR_FALSE, /* persist: broken by bug 193031 */
                                 aOverrideInternalTypes, /* replace if we're told to */
                                 nsnull);
      }
    }
    
    PLUGIN_LOG(PLUGIN_LOG_NOISY,
               ("nsPluginTag::RegisterWithCategoryManager mime=%s, plugin=%s\n",
                mMimeTypeArray[i], mFileName.get()));
  }
}

void nsPluginTag::Mark(PRUint32 mask)
{
  PRBool wasEnabled = IsEnabled();
  mFlags |= mask;
  // Update entries in the category manager if necessary.
  if (mPluginHost && wasEnabled != IsEnabled()) {
    if (wasEnabled)
      RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginUnregister);
    else
      RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginRegister);
  }
}

void nsPluginTag::UnMark(PRUint32 mask)
{
  PRBool wasEnabled = IsEnabled();
  mFlags &= ~mask;
  // Update entries in the category manager if necessary.
  if (mPluginHost && wasEnabled != IsEnabled()) {
    if (wasEnabled)
      RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginUnregister);
    else
      RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginRegister);
  }
}

PRBool nsPluginTag::HasFlag(PRUint32 flag)
{
  return (mFlags & flag) != 0;
}

PRUint32 nsPluginTag::Flags()
{
  return mFlags;
}

PRBool nsPluginTag::IsEnabled()
{
  return HasFlag(NS_PLUGIN_FLAG_ENABLED) && !HasFlag(NS_PLUGIN_FLAG_BLOCKLISTED);
}

PRBool nsPluginTag::Equals(nsPluginTag *aPluginTag)
{
  NS_ENSURE_TRUE(aPluginTag, PR_FALSE);
  
  if ((!mName.Equals(aPluginTag->mName)) ||
      (!mDescription.Equals(aPluginTag->mDescription)) ||
      (mVariants != aPluginTag->mVariants))
    return PR_FALSE;
  
  if (mVariants && mMimeTypeArray && aPluginTag->mMimeTypeArray) {
    for (PRInt32 i = 0; i < mVariants; i++) {
      if (PL_strcmp(mMimeTypeArray[i], aPluginTag->mMimeTypeArray[i]) != 0)
        return PR_FALSE;
    }
  }
  return PR_TRUE;
}

void nsPluginTag::TryUnloadPlugin()
{
  if (mEntryPoint) {
    mEntryPoint->Shutdown();
    mEntryPoint = nsnull;
  }
  
  // before we unload check if we are allowed to, see bug #61388
  if (mLibrary && mCanUnloadLibrary) {
    // unload the plugin asynchronously by posting a PLEvent
    nsPluginHost::PostPluginUnloadEvent(mLibrary);
  }
  
  // we should zero it anyway, it is going to be unloaded by
  // CleanUnsedLibraries before we need to call the library
  // again so the calling code should not be fooled and reload
  // the library fresh
  mLibrary = nsnull;
  
  // Remove mime types added to the category manager
  // only if we were made 'active' by setting the host
  if (mPluginHost) {
    RegisterWithCategoryManager(PR_FALSE, nsPluginTag::ePluginUnregister);
  }
}

void
nsPluginTag::RemoveJavaSentinel(PRInt32 sentinelIndex)
{
  if (sentinelIndex == -1)
    return;

  delete[] mMimeTypeArray[sentinelIndex];
  mMimeDescriptionArray.RemoveElementAt(sentinelIndex);
  if (mExtensionsArray)
    delete[] mExtensionsArray[sentinelIndex];

  // Move the subsequent entries in the arrays.
  if (mVariants > sentinelIndex + 1) {
    memmove(mMimeTypeArray + sentinelIndex,
            mMimeTypeArray + sentinelIndex + 1,
            (mVariants - sentinelIndex - 1) * sizeof(mMimeTypeArray[0]));

    if (mExtensionsArray) {
      memmove(mExtensionsArray + sentinelIndex,
              mExtensionsArray + sentinelIndex + 1,
              (mVariants - sentinelIndex - 1) * sizeof(mExtensionsArray[0]));
    }
  }
  --mVariants;

  mMimeTypeArray[mVariants] = NULL;
  if (mExtensionsArray)
    mExtensionsArray[mVariants] = NULL;
}
