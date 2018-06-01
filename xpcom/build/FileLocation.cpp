/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileLocation.h"
#include "nsZipArchive.h"
#include "nsURLHelper.h"

namespace mozilla {

FileLocation::FileLocation() = default;

FileLocation::~FileLocation() = default;

FileLocation::FileLocation(nsIFile* aFile)
{
  Init(aFile);
}

FileLocation::FileLocation(nsIFile* aFile, const char* aPath)
{
  Init(aFile, aPath);
}

FileLocation::FileLocation(nsZipArchive* aZip, const char* aPath)
{
  Init(aZip, aPath);
}

FileLocation::FileLocation(const FileLocation& aOther)
  : mBaseFile(aOther.mBaseFile)
  , mBaseZip(aOther.mBaseZip)
  , mPath(aOther.mPath)
{
}

FileLocation::FileLocation(FileLocation&& aOther)
  : mBaseFile(std::move(aOther.mBaseFile))
  , mBaseZip(std::move(aOther.mBaseZip))
  , mPath(std::move(aOther.mPath))
{
  aOther.mPath.Truncate();
}

FileLocation::FileLocation(const FileLocation& aFile, const char* aPath)
{
  if (aFile.IsZip()) {
    if (aFile.mBaseFile) {
      Init(aFile.mBaseFile, aFile.mPath.get());
    }
    else {
      Init(aFile.mBaseZip, aFile.mPath.get());
    }
    if (aPath) {
      int32_t i = mPath.RFindChar('/');
      if (kNotFound == i) {
        mPath.Truncate(0);
      } else {
        mPath.Truncate(i + 1);
      }
      mPath += aPath;
    }
  } else {
    if (aPath) {
      nsCOMPtr<nsIFile> cfile;
      aFile.mBaseFile->GetParent(getter_AddRefs(cfile));

#if defined(XP_WIN)
      nsAutoCString pathStr(aPath);
      char* p;
      uint32_t len = pathStr.GetMutableData(&p);
      for (; len; ++p, --len) {
        if ('/' == *p) {
          *p = '\\';
        }
      }
      cfile->AppendRelativeNativePath(pathStr);
#else
      cfile->AppendRelativeNativePath(nsDependentCString(aPath));
#endif
      Init(cfile);
    } else {
      Init(aFile.mBaseFile);
    }
  }
}

void
FileLocation::Init(nsIFile* aFile)
{
  mBaseZip = nullptr;
  mBaseFile = aFile;
  mPath.Truncate();
}

void
FileLocation::Init(nsIFile* aFile, const char* aPath)
{
  mBaseZip = nullptr;
  mBaseFile = aFile;
  mPath = aPath;
}

void
FileLocation::Init(nsZipArchive* aZip, const char* aPath)
{
  mBaseZip = aZip;
  mBaseFile = nullptr;
  mPath = aPath;
}

void
FileLocation::GetURIString(nsACString& aResult) const
{
  if (mBaseFile) {
    net_GetURLSpecFromActualFile(mBaseFile, aResult);
  } else if (mBaseZip) {
    RefPtr<nsZipHandle> handler = mBaseZip->GetFD();
    handler->mFile.GetURIString(aResult);
  }
  if (IsZip()) {
    aResult.InsertLiteral("jar:", 0);
    aResult += "!/";
    aResult += mPath;
  }
}

already_AddRefed<nsIFile>
FileLocation::GetBaseFile()
{
  if (IsZip() && mBaseZip) {
    RefPtr<nsZipHandle> handler = mBaseZip->GetFD();
    if (handler) {
      return handler->mFile.GetBaseFile();
    }
    return nullptr;
  }

  nsCOMPtr<nsIFile> file = mBaseFile;
  return file.forget();
}

bool
FileLocation::Equals(const FileLocation& aFile) const
{
  if (mPath != aFile.mPath) {
    return false;
  }

  if (mBaseFile && aFile.mBaseFile) {
    bool eq;
    return NS_SUCCEEDED(mBaseFile->Equals(aFile.mBaseFile, &eq)) && eq;
  }

  const FileLocation* a = this;
  const FileLocation* b = &aFile;
  if (a->mBaseZip) {
    RefPtr<nsZipHandle> handler = a->mBaseZip->GetFD();
    a = &handler->mFile;
  }
  if (b->mBaseZip) {
    RefPtr<nsZipHandle> handler = b->mBaseZip->GetFD();
    b = &handler->mFile;
  }

  return a->Equals(*b);
}

nsresult
FileLocation::GetData(Data& aData)
{
  if (!IsZip()) {
    return mBaseFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &aData.mFd.rwget());
  }
  aData.mZip = mBaseZip;
  if (!aData.mZip) {
    aData.mZip = new nsZipArchive();
    aData.mZip->OpenArchive(mBaseFile);
  }
  aData.mItem = aData.mZip->GetItem(mPath.get());
  if (aData.mItem) {
    return NS_OK;
  }
  return NS_ERROR_FILE_UNRECOGNIZED_PATH;
}

nsresult
FileLocation::Data::GetSize(uint32_t* aResult)
{
  if (mFd) {
    PRFileInfo64 fileInfo;
    if (PR_SUCCESS != PR_GetOpenFileInfo64(mFd, &fileInfo)) {
      return NS_ErrorAccordingToNSPR();
    }

    if (fileInfo.size > int64_t(UINT32_MAX)) {
      return NS_ERROR_FILE_TOO_BIG;
    }

    *aResult = fileInfo.size;
    return NS_OK;
  }
  if (mItem) {
    *aResult = mItem->RealSize();
    return NS_OK;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

nsresult
FileLocation::Data::Copy(char* aBuf, uint32_t aLen)
{
  if (mFd) {
    for (uint32_t totalRead = 0; totalRead < aLen;) {
      int32_t read = PR_Read(mFd, aBuf + totalRead,
                             XPCOM_MIN(aLen - totalRead, uint32_t(INT32_MAX)));
      if (read < 0) {
        return NS_ErrorAccordingToNSPR();
      }
      totalRead += read;
    }
    return NS_OK;
  }
  if (mItem) {
    nsZipCursor cursor(mItem, mZip, reinterpret_cast<uint8_t*>(aBuf),
                       aLen, true);
    uint32_t readLen;
    cursor.Copy(&readLen);
    if (readLen != aLen) {
      nsZipArchive::sFileCorruptedReason = "FileLocation::Data: insufficient data";
      return NS_ERROR_FILE_CORRUPTED;
    }
    return NS_OK;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

} /* namespace mozilla */
