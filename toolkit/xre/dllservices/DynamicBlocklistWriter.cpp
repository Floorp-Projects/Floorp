/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/DynamicBlocklist.h"
#include "mozilla/LauncherRegistryInfo.h"

#include "nsISafeOutputStream.h"
#include "nsNetUtil.h"

namespace mozilla {
#if ENABLE_TESTS
nsDependentString testEntryString(DynamicBlockList::kTestDll,
                                  DynamicBlockList::kTestDllBytes /
                                      sizeof(DynamicBlockList::kTestDll[0]));
#endif

bool ShouldWriteEntry(const nsAString& name) {
#if ENABLE_TESTS
  return name != testEntryString;
#else
  return true;
#endif
}

DynamicBlocklistWriter::DynamicBlocklistWriter(
    RefPtr<dom::Promise> aPromise,
    const nsTHashSet<nsStringCaseInsensitiveHashKey>& aBlocklist)
    : mPromise(aPromise), mArraySize(0), mStringBufferSize(0) {
  CheckedUint32 payloadSize;
  bool hasTestEntry = false;
  for (const nsAString& name : aBlocklist) {
    if (ShouldWriteEntry(name)) {
      payloadSize += name.Length() * sizeof(char16_t);
      if (!payloadSize.isValid()) {
        return;
      }
    } else {
      hasTestEntry = true;
    }
  }

  uint32_t entriesToWrite = aBlocklist.Count();
  if (hasTestEntry) {
    entriesToWrite -= 1;
  }

  mStringBufferSize = payloadSize.value();
  mArraySize = (entriesToWrite + 1) * sizeof(DllBlockInfo);

  payloadSize += mArraySize;
  if (!payloadSize.isValid()) {
    return;
  }

  mStringBuffer = MakeUnique<uint8_t[]>(mStringBufferSize);
  Unused << mArray.resize(entriesToWrite + 1);  // aBlockEntries + sentinel

  size_t currentStringOffset = 0;
  size_t i = 0;
  for (const nsAString& name : aBlocklist) {
    if (!ShouldWriteEntry(name)) {
      continue;
    }
    const uint32_t nameSize = name.Length() * sizeof(char16_t);

    mArray[i].mMaxVersion = DllBlockInfo::ALL_VERSIONS;
    mArray[i].mFlags = DllBlockInfoFlags::FLAGS_DEFAULT;

    // Copy the module's name to the string buffer and store its offset
    // in mName.Buffer
    memcpy(mStringBuffer.get() + currentStringOffset, name.BeginReading(),
           nameSize);
    mArray[i].mName.Buffer =
        reinterpret_cast<wchar_t*>(mArraySize + currentStringOffset);
    // Only keep mName.Length and leave mName.MaximumLength to be zero
    mArray[i].mName.Length = nameSize;

    currentStringOffset += nameSize;
    ++i;
  }
}

nsresult DynamicBlocklistWriter::WriteToFile(const nsAString& aName) const {
  nsCOMPtr<nsIFile> file;
  MOZ_TRY(NS_NewLocalFile(aName, true, getter_AddRefs(file)));

  nsCOMPtr<nsIOutputStream> stream;
  MOZ_TRY(NS_NewSafeLocalFileOutputStream(getter_AddRefs(stream), file));

  MOZ_TRY(WriteValue(stream, kSignature));
  MOZ_TRY(WriteValue(stream, kCurrentVersion));
  MOZ_TRY(WriteValue(stream,
                     static_cast<uint32_t>(mArraySize + mStringBufferSize)));
  MOZ_TRY(WriteBuffer(stream, mArray.begin(), mArraySize));
  MOZ_TRY(WriteBuffer(stream, mStringBuffer.get(), mStringBufferSize));

  nsresult rv;
  nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(stream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return safeStream->Finish();
}

void DynamicBlocklistWriter::Run() {
  MOZ_ASSERT(!NS_IsMainThread());

  nsresult rv = NS_ERROR_ABORT;

  LauncherRegistryInfo regInfo;
  LauncherResult<std::wstring> blocklistFile = regInfo.GetBlocklistFileName();
  if (blocklistFile.isOk()) {
    const wchar_t* rawBuffer = blocklistFile.inspect().c_str();
    rv = WriteToFile(nsDependentString(rawBuffer));
  }

  NS_DispatchToMainThread(
      // Don't capture mPromise by copy because we're not in the main thread
      NS_NewRunnableFunction(__func__, [promise = std::move(mPromise), rv]() {
        if (NS_SUCCEEDED(rv)) {
          promise->MaybeResolve(JS::NullHandleValue);
        } else {
          promise->MaybeReject(rv);
        }
      }));
}

void DynamicBlocklistWriter::Cancel() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mPromise) {
    mPromise->MaybeReject(NS_ERROR_ABORT);
  }
}

}  // namespace mozilla
