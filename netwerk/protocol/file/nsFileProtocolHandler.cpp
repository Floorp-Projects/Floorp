/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:ts=4 sw=2 sts=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIFile.h"
#include "nsFileProtocolHandler.h"
#include "nsFileChannel.h"
#include "nsStandardURL.h"
#include "nsURLHelper.h"
#include "nsIURIMutator.h"

#include "nsNetUtil.h"

#include "FileChannelChild.h"

#include "mozilla/ResultExtensions.h"

// URL file handling, copied and modified from
// xpfe/components/bookmarks/src/nsBookmarksService.cpp
#ifdef XP_WIN
#  include <shlobj.h>
#  include <intshcut.h>
#  include "nsIFileURL.h"
#  ifdef CompareString
#    undef CompareString
#  endif
#endif

// URL file handling for freedesktop.org
#ifdef XP_UNIX
#  include "nsINIParser.h"
#  define DESKTOP_ENTRY_SECTION "Desktop Entry"
#endif

//-----------------------------------------------------------------------------

nsresult nsFileProtocolHandler::Init() { return NS_OK; }

NS_IMPL_ISUPPORTS(nsFileProtocolHandler, nsIFileProtocolHandler,
                  nsIProtocolHandler, nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsIProtocolHandler methods:

#if defined(XP_WIN)
NS_IMETHODIMP
nsFileProtocolHandler::ReadURLFile(nsIFile* aFile, nsIURI** aURI) {
  nsAutoString path;
  nsresult rv = aFile->GetPath(path);
  if (NS_FAILED(rv)) return rv;

  if (path.Length() < 4) return NS_ERROR_NOT_AVAILABLE;
  if (!StringTail(path, 4).LowerCaseEqualsLiteral(".url"))
    return NS_ERROR_NOT_AVAILABLE;

  HRESULT result;

  rv = NS_ERROR_NOT_AVAILABLE;

  IUniformResourceLocatorW* urlLink = nullptr;
  result =
      ::CoCreateInstance(CLSID_InternetShortcut, nullptr, CLSCTX_INPROC_SERVER,
                         IID_IUniformResourceLocatorW, (void**)&urlLink);
  if (SUCCEEDED(result) && urlLink) {
    IPersistFile* urlFile = nullptr;
    result = urlLink->QueryInterface(IID_IPersistFile, (void**)&urlFile);
    if (SUCCEEDED(result) && urlFile) {
      result = urlFile->Load(path.get(), STGM_READ);
      if (SUCCEEDED(result)) {
        LPWSTR lpTemp = nullptr;

        // The URL this method will give us back seems to be already
        // escaped. Hence, do not do escaping of our own.
        result = urlLink->GetURL(&lpTemp);
        if (SUCCEEDED(result) && lpTemp) {
          rv = NS_NewURI(aURI, nsDependentString(lpTemp));
          // free the string that GetURL alloc'd
          CoTaskMemFree(lpTemp);
        }
      }
      urlFile->Release();
    }
    urlLink->Release();
  }
  return rv;
}

#elif defined(XP_UNIX)
NS_IMETHODIMP
nsFileProtocolHandler::ReadURLFile(nsIFile* aFile, nsIURI** aURI) {
  // We only support desktop files that end in ".desktop" like the spec says:
  // http://standards.freedesktop.org/desktop-entry-spec/latest/ar01s02.html
  nsAutoCString leafName;
  nsresult rv = aFile->GetNativeLeafName(leafName);
  if (NS_FAILED(rv) || !StringEndsWith(leafName, ".desktop"_ns)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool isFile = false;
  rv = aFile->IsFile(&isFile);
  if (NS_FAILED(rv) || !isFile) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsINIParser parser;
  rv = parser.Init(aFile);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString type;
  parser.GetString(DESKTOP_ENTRY_SECTION, "Type", type);
  if (!type.EqualsLiteral("Link")) return NS_ERROR_NOT_AVAILABLE;

  nsAutoCString url;
  rv = parser.GetString(DESKTOP_ENTRY_SECTION, "URL", url);
  if (NS_FAILED(rv) || url.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  return NS_NewURI(aURI, url);
}

#else   // other platforms
NS_IMETHODIMP
nsFileProtocolHandler::ReadURLFile(nsIFile* aFile, nsIURI** aURI) {
  return NS_ERROR_NOT_AVAILABLE;
}
#endif  // ReadURLFile()

NS_IMETHODIMP
nsFileProtocolHandler::ReadShellLink(nsIFile* aFile, nsIURI** aURI) {
#if defined(XP_WIN)
  nsAutoString path;
  MOZ_TRY(aFile->GetPath(path));

  if (path.Length() < 4 ||
      !StringTail(path, 4).LowerCaseEqualsLiteral(".lnk")) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<IPersistFile> persistFile;
  RefPtr<IShellLinkW> shellLink;
  WCHAR lpTemp[MAX_PATH];
  // Get a pointer to the IPersistFile interface.
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IShellLinkW, getter_AddRefs(shellLink))) ||
      FAILED(shellLink->QueryInterface(IID_IPersistFile,
                                       getter_AddRefs(persistFile))) ||
      FAILED(persistFile->Load(path.get(), STGM_READ)) ||
      FAILED(shellLink->Resolve(nullptr, SLR_NO_UI)) ||
      FAILED(shellLink->GetPath(lpTemp, MAX_PATH, nullptr, SLGP_UNCPRIORITY))) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIFile> linkedFile;
  MOZ_TRY(NS_NewLocalFile(nsDependentString(lpTemp), false,
                          getter_AddRefs(linkedFile)));
  return NS_NewFileURI(aURI, linkedFile);
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsFileProtocolHandler::GetScheme(nsACString& result) {
  result.AssignLiteral("file");
  return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetDefaultPort(int32_t* result) {
  *result = -1;  // no port for file: URLs
  return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetProtocolFlags(uint32_t* result) {
  *result = URI_NOAUTH | URI_IS_LOCAL_FILE | URI_IS_LOCAL_RESOURCE |
            URI_IS_POTENTIALLY_TRUSTWORTHY;
  return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::NewChannel(nsIURI* uri, nsILoadInfo* aLoadInfo,
                                  nsIChannel** result) {
  nsresult rv;

  RefPtr<nsFileChannel> chan;
  if (IsNeckoChild()) {
    chan = new mozilla::net::FileChannelChild(uri);
  } else {
    chan = new nsFileChannel(uri);
  }

  // set the loadInfo on the new channel ; must do this
  // before calling Init() on it, since it needs the load
  // info be already set.
  rv = chan->SetLoadInfo(aLoadInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = chan->Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  chan.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::AllowPort(int32_t port, const char* scheme,
                                 bool* result) {
  // don't override anything.
  *result = false;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIFileProtocolHandler methods:

NS_IMETHODIMP
nsFileProtocolHandler::NewFileURI(nsIFile* aFile, nsIURI** aResult) {
  NS_ENSURE_ARG_POINTER(aFile);

  RefPtr<nsIFile> file(aFile);
  // NOTE: the origin charset is assigned the value of the platform
  // charset by the SetFile method.
  return NS_MutateURI(new nsStandardURL::Mutator())
      .Apply(NS_MutatorMethod(&nsIFileURLMutator::SetFile, file))
      .Finalize(aResult);
}

NS_IMETHODIMP
nsFileProtocolHandler::NewFileURIMutator(nsIFile* aFile,
                                         nsIURIMutator** aResult) {
  NS_ENSURE_ARG_POINTER(aFile);
  nsresult rv;

  nsCOMPtr<nsIURIMutator> mutator = new nsStandardURL::Mutator();
  nsCOMPtr<nsIFileURLMutator> fileMutator = do_QueryInterface(mutator, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // NOTE: the origin charset is assigned the value of the platform
  // charset by the SetFile method.
  rv = fileMutator->SetFile(aFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mutator.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetURLSpecFromFile(nsIFile* file, nsACString& result) {
  NS_ENSURE_ARG_POINTER(file);
  return net_GetURLSpecFromFile(file, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetURLSpecFromActualFile(nsIFile* file,
                                                nsACString& result) {
  NS_ENSURE_ARG_POINTER(file);
  return net_GetURLSpecFromActualFile(file, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetURLSpecFromDir(nsIFile* file, nsACString& result) {
  NS_ENSURE_ARG_POINTER(file);
  return net_GetURLSpecFromDir(file, result);
}

NS_IMETHODIMP
nsFileProtocolHandler::GetFileFromURLSpec(const nsACString& spec,
                                          nsIFile** result) {
  return net_GetFileFromURLSpec(spec, result);
}
