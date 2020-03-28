/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cerrno>
#include <climits>
#include <cmath>
#include <fstream>
#include <mutex>
#include <prinrval.h>
#include <type_traits>
#ifdef _WINDOWS
#  include <process.h>
#  define getpid _getpid
#else
#  include <unistd.h>
#endif
#include "base/string_util.h"
#include "FuzzingMutate.h"
#include "FuzzingTraits.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/file_descriptor_set_posix.h"
#include "mozilla/ipc/Faulty.h"
#include "mozilla/TypeTraits.h"
#include "nsComponentManagerUtils.h"
#include "nsNetCID.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "nsLocalFile.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include "prenv.h"

#ifdef IsLoggingEnabled
// This is defined in the Windows SDK urlmon.h
#  undef IsLoggingEnabled
#endif

namespace mozilla {
namespace ipc {

using namespace mozilla::fuzzing;

/**
 * FuzzIntegralType mutates an incercepted integral type of a pickled message.
 */
template <typename T>
void FuzzIntegralType(T* v, bool largeValues) {
  static_assert(std::is_integral_v<T> == true, "T must be an integral type");
  switch (FuzzingTraits::Random(6)) {
    case 0:
      if (largeValues) {
        (*v) = RandomInteger<T>();
        break;
      }
      [[fallthrough]];
    case 1:
      if (largeValues) {
        (*v) = RandomNumericLimit<T>();
        break;
      }
      [[fallthrough]];
    case 2:
      if (largeValues) {
        (*v) = RandomIntegerRange<T>(std::numeric_limits<T>::min(),
                                     std::numeric_limits<T>::max());
        break;
      }
      [[fallthrough]];
    default:
      switch (FuzzingTraits::Random(2)) {
        case 0:
          // Prevent underflow
          if (*v != std::numeric_limits<T>::min()) {
            (*v)--;
            break;
          }
          [[fallthrough]];
        case 1:
          // Prevent overflow
          if (*v != std::numeric_limits<T>::max()) {
            (*v)++;
            break;
          }
      }
  }
}

/**
 * FuzzFloatingPointType mutates an incercepted floating-point type of a
 * pickled message.
 */
template <typename T>
void FuzzFloatingPointType(T* v, bool largeValues) {
  static_assert(std::is_floating_point_v<T> == true,
                "T must be a floating point type");
  switch (FuzzingTraits::Random(6)) {
    case 0:
      if (largeValues) {
        (*v) = RandomNumericLimit<T>();
        break;
      }
      [[fallthrough]];
    case 1:
      if (largeValues) {
        (*v) = RandomFloatingPointRange<T>(std::numeric_limits<T>::min(),
                                           std::numeric_limits<T>::max());
        break;
      }
      [[fallthrough]];
    default:
      (*v) = RandomFloatingPoint<T>();
  }
}

/**
 * FuzzStringType mutates an incercepted string type of a pickled message.
 */
template <typename T>
void FuzzStringType(T& v, const T& literal1, const T& literal2) {
  switch (FuzzingTraits::Random(5)) {
    case 4:
      v = v + v;
      [[fallthrough]];
    case 3:
      v = v + v;
      [[fallthrough]];
    case 2:
      v = v + v;
      break;
    case 1:
      v += literal1;
      break;
    case 0:
      v = literal2;
      break;
  }
}

Faulty::Faulty()
    // Mutate messages as a blob.
    : mFuzzMessages(!!PR_GetEnv("FAULTY_MESSAGES"))
      // Enables the strategy for fuzzing pipes.
      ,
      mFuzzPipes(!!PR_GetEnv("FAULTY_PIPE"))
      // Enables the strategy for fuzzing pickled messages.
      ,
      mFuzzPickle(!!PR_GetEnv("FAULTY_PICKLE"))
      // Uses very large values while fuzzing pickled messages.
      // This may cause a high amount of malloc_abort() / NS_ABORT_OOM crashes.
      ,
      mUseLargeValues(!!PR_GetEnv("FAULTY_LARGE_VALUES"))
      // Use the provided blacklist as whitelist.
      ,
      mUseAsWhitelist(!!PR_GetEnv("FAULTY_AS_WHITELIST"))
      // Sets up our target process.
      ,
      mIsValidProcessType(IsValidProcessType()) {
  if (mIsValidProcessType) {
    FAULTY_LOG("Initializing for new process of type '%s' with pid %u.",
               XRE_GetProcessTypeString(), getpid());

    /* Setup random seed. */
    const char* userSeed = PR_GetEnv("FAULTY_SEED");
    unsigned long randomSeed = static_cast<unsigned long>(PR_IntervalNow());
    if (userSeed) {
      long n = std::strtol(userSeed, nullptr, 10);
      if (n != 0) {
        randomSeed = static_cast<unsigned long>(n);
      }
    }
    FuzzingTraits::Rng().seed(randomSeed);

    /* Setup directory for dumping messages. */
    mMessagePath = PR_GetEnv("FAULTY_MESSAGE_PATH");
    if (mMessagePath && *mMessagePath) {
      if (CreateOutputDirectory(mMessagePath) != NS_OK) {
        mMessagePath = nullptr;
      }
    }

    /* Set IPC messages blacklist. */
    mBlacklistPath = PR_GetEnv("FAULTY_BLACKLIST");
    if (mBlacklistPath && *mBlacklistPath) {
      FAULTY_LOG("* Using message blacklist    = %s", mBlacklistPath);
    }

    FAULTY_LOG("* Fuzzing strategy: messages = %s",
               mFuzzMessages ? "enabled" : "disabled");
    FAULTY_LOG("* Fuzzing strategy: pickle   = %s",
               mFuzzPickle ? "enabled" : "disabled");
    FAULTY_LOG("* Fuzzing strategy: pipe     = %s",
               mFuzzPipes ? "enabled" : "disabled");
    FAULTY_LOG("* Fuzzing probability        = %u", DefaultProbability());
    FAULTY_LOG("* Fuzzing mutation factor    = %u", MutationFactor());
    FAULTY_LOG("* RNG seed                   = %lu", randomSeed);

    sMsgCounter = 0;
  }
}

// static
bool Faulty::IsValidProcessType(void) {
  bool isValidProcessType;
  const bool targetChildren = !!PR_GetEnv("FAULTY_CHILDREN");
  const bool targetParent = !!PR_GetEnv("FAULTY_PARENT");
  const bool isParent = XRE_IsParentProcess();

  if (targetChildren && !targetParent) {
    // Fuzz every child process type but not the parent process.
    isValidProcessType = isParent;
  } else if (!targetChildren && targetParent && !isParent) {
    // Fuzz inside any of the above child process only.
    isValidProcessType = true;
  } else if (targetChildren && targetParent) {
    // Fuzz every process type.
    isValidProcessType = true;
  } else {
    // Fuzz no process type at all.
    isValidProcessType = false;
  }

  if (!isValidProcessType) {
    FAULTY_LOG("Disabled for this process of type '%s' with pid %d.",
               XRE_GetProcessTypeString(), getpid());
  }

  return isValidProcessType;
}

// static
unsigned int Faulty::DefaultProbability() {
  static std::once_flag flag;
  static unsigned probability;

  std::call_once(flag, [&] {
    probability = FAULTY_DEFAULT_PROBABILITY;
    // Defines the likelihood of fuzzing a message.
    if (const char* p = PR_GetEnv("FAULTY_PROBABILITY")) {
      long n = std::strtol(p, nullptr, 10);
      if (n != 0) {
        probability = n;
      }
    }
  });

  return probability;
}

// static
bool Faulty::IsLoggingEnabled(void) {
  static bool enabled;
  static std::once_flag flag;
  std::call_once(flag, [&] { enabled = !!PR_GetEnv("FAULTY_ENABLE_LOGGING"); });
  return enabled;
}

// static
uint32_t Faulty::MutationFactor() {
  static uint64_t sPropValue = FAULTY_DEFAULT_MUTATION_FACTOR;
  static bool sInitialized = false;

  if (sInitialized) {
    return sPropValue;
  }
  sInitialized = true;

  const char* factor = PR_GetEnv("FAULTY_MUTATION_FACTOR");
  if (factor) {
    long n = strtol(factor, nullptr, 10);
    if (n != 0) {
      sPropValue = n;
      return sPropValue;
    }
  }
  return sPropValue;
}

// static
Faulty& Faulty::instance() {
  static Faulty faulty;
  return faulty;
}

//
// Strategy: Pipes
//

void Faulty::MaybeCollectAndClosePipe(int aPipe, unsigned int aProbability) {
#ifndef _WINDOWS
  if (!mFuzzPipes) {
    return;
  }

  if (aPipe > -1) {
    FAULTY_LOG("Collecting pipe %d to bucket of pipes (count: %zu)", aPipe,
               mFds.size());
    mFds.insert(aPipe);
  }

  if (mFds.size() > 0 && FuzzingTraits::Sometimes(aProbability)) {
    std::set<int>::iterator it(mFds.begin());
    std::advance(it, FuzzingTraits::Random(mFds.size()));
    FAULTY_LOG("Trying to close collected pipe: %d", *it);
    errno = 0;
    while ((close(*it) == -1 && (errno == EINTR))) {
      ;
    }
    FAULTY_LOG("Pipe status after attempt to close: %d", errno);
    mFds.erase(it);
  }
#endif
}

//
// Strategy: Pickle
//

void Faulty::MutateBool(bool* aValue) { *aValue = !(*aValue); }

void Faulty::FuzzBool(bool* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      bool oldValue = *aValue;
      MutateBool(aValue);
      FAULTY_LOG("Message field |bool| of value: %d mutated to: %d",
                 (int)oldValue, (int)*aValue);
    }
  }
}

void Faulty::MutateChar(char* aValue) { FuzzIntegralType<char>(aValue, true); }

void Faulty::FuzzChar(char* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      char oldValue = *aValue;
      MutateChar(aValue);
      FAULTY_LOG("Message field |char| of value: %c mutated to: %c", oldValue,
                 *aValue);
    }
  }
}

void Faulty::MutateUChar(unsigned char* aValue) {
  FuzzIntegralType<unsigned char>(aValue, true);
}

void Faulty::FuzzUChar(unsigned char* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      unsigned char oldValue = *aValue;
      MutateUChar(aValue);
      FAULTY_LOG("Message field |unsigned char| of value: %u mutated to: %u",
                 oldValue, *aValue);
    }
  }
}

void Faulty::MutateInt16(int16_t* aValue) {
  FuzzIntegralType<int16_t>(aValue, true);
}

void Faulty::FuzzInt16(int16_t* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      int16_t oldValue = *aValue;
      MutateInt16(aValue);
      FAULTY_LOG("Message field |int16| of value: %d mutated to: %d", oldValue,
                 *aValue);
    }
  }
}

void Faulty::MutateUInt16(uint16_t* aValue) {
  FuzzIntegralType<uint16_t>(aValue, true);
}

void Faulty::FuzzUInt16(uint16_t* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      uint16_t oldValue = *aValue;
      MutateUInt16(aValue);
      FAULTY_LOG("Message field |uint16| of value: %d mutated to: %d", oldValue,
                 *aValue);
    }
  }
}

void Faulty::MutateInt(int* aValue) {
  FuzzIntegralType<int>(aValue, mUseLargeValues);
}

void Faulty::FuzzInt(int* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      int oldValue = *aValue;
      MutateInt(aValue);
      FAULTY_LOG("Message field |int| of value: %d mutated to: %d", oldValue,
                 *aValue);
    }
  }
}

void Faulty::MutateUInt32(uint32_t* aValue) {
  FuzzIntegralType<uint32_t>(aValue, mUseLargeValues);
}

void Faulty::FuzzUInt32(uint32_t* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      uint32_t oldValue = *aValue;
      MutateUInt32(aValue);
      FAULTY_LOG("Message field |uint32| of value: %u mutated to: %u", oldValue,
                 *aValue);
    }
  }
}

void Faulty::MutateLong(long* aValue) {
  FuzzIntegralType<long>(aValue, mUseLargeValues);
}

void Faulty::FuzzLong(long* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      long oldValue = *aValue;
      MutateLong(aValue);
      FAULTY_LOG("Message field |long| of value: %ld mutated to: %ld", oldValue,
                 *aValue);
    }
  }
}

void Faulty::MutateULong(unsigned long* aValue) {
  FuzzIntegralType<unsigned long>(aValue, mUseLargeValues);
}

void Faulty::FuzzULong(unsigned long* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      unsigned long oldValue = *aValue;
      MutateULong(aValue);
      FAULTY_LOG("Message field |unsigned long| of value: %lu mutated to: %lu",
                 oldValue, *aValue);
    }
  }
}

void Faulty::MutateUInt64(uint64_t* aValue) {
  FuzzIntegralType<uint64_t>(aValue, mUseLargeValues);
}

void Faulty::FuzzUInt64(uint64_t* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      uint64_t oldValue = *aValue;
      MutateUInt64(aValue);
      FAULTY_LOG("Message field |uint64| of value: %" PRIu64
                 " mutated to: %" PRIu64,
                 oldValue, *aValue);
    }
  }
}

void Faulty::MutateInt64(int64_t* aValue) {
  FuzzIntegralType<int64_t>(aValue, mUseLargeValues);
}

void Faulty::FuzzInt64(int64_t* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      int64_t oldValue = *aValue;
      MutateInt64(aValue);
      FAULTY_LOG("Message field |int64| of value: %" PRIu64
                 " mutated to: %" PRIu64,
                 oldValue, *aValue);
    }
  }
}

void Faulty::MutateDouble(double* aValue) {
  FuzzFloatingPointType<double>(aValue, mUseLargeValues);
}

void Faulty::FuzzDouble(double* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      double oldValue = *aValue;
      MutateDouble(aValue);
      FAULTY_LOG("Message field |double| of value: %f mutated to: %f", oldValue,
                 *aValue);
    }
  }
}

void Faulty::MutateFloat(float* aValue) {
  FuzzFloatingPointType<float>(aValue, mUseLargeValues);
}

void Faulty::FuzzFloat(float* aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      float oldValue = *aValue;
      MutateFloat(aValue);
      FAULTY_LOG("Message field |float| of value: %f mutated to: %f", oldValue,
                 *aValue);
    }
  }
}

void Faulty::FuzzString(std::string& aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      std::string oldValue = aValue;
      FuzzStringType<std::string>(aValue, "xoferiF", std::string());
      FAULTY_LOG("Message field |string| of value: %s mutated to: %s",
                 oldValue.c_str(), aValue.c_str());
    }
  }
}

void Faulty::FuzzWString(std::wstring& aValue, unsigned int aProbability) {
  if (mIsValidProcessType) {
    if (mFuzzPickle && FuzzingTraits::Sometimes(aProbability)) {
      std::wstring oldValue = aValue;
      FAULTY_LOG("Message field |wstring|");
      FuzzStringType<std::wstring>(aValue, L"xoferiF", std::wstring());
    }
  }
}

// static
nsresult Faulty::CreateOutputDirectory(const char* aPathname) {
  nsCOMPtr<nsIFile> path;
  bool exists;
  nsresult rv;

  rv = NS_NewNativeLocalFile(nsDependentCString(aPathname), true,
                             getter_AddRefs(path));

  rv = path->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    rv = path->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

/* static */
nsresult Faulty::ReadFile(const char* aPathname, nsTArray<nsCString>& aArray) {
  nsresult rv;
  nsCOMPtr<nsIFile> file;

  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(aPathname), true,
                       getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists = false;
  rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv)) || !exists) {
    return rv;
  }

  nsCOMPtr<nsIFileInputStream> fileStream(
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = fileStream->Init(file, -1, -1, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(fileStream, &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString line;
  bool more = true;
  do {
    rv = lineStream->ReadLine(line, &more);
    if (line.IsEmpty()) {
      continue;
    }
    if (line.CharAt(0) == '#') {
      /* Ignore comments. */
      continue;
    }
    aArray.AppendElement(line);
  } while (more);

  return NS_OK;
}

bool Faulty::IsMessageNameBlacklisted(const char* aMessageName) {
  static bool sFileLoaded = false;
  static nsTArray<nsCString> sMessageBlacklist;

  if (!sFileLoaded && mBlacklistPath) {
    /* Run ReadFile() on the main thread to prevent
       MOZ_ASSERT(NS_IsMainThread()) in nsStandardURL via nsNetStartup(). */
    nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableFunction("Fuzzer::ReadBlacklistOnMainThread", [&]() {
          if (Faulty::ReadFile(mBlacklistPath, sMessageBlacklist) != NS_OK) {
            sFileLoaded = false;
          } else {
            sFileLoaded = true;
          }
        });
    NS_DispatchToMainThread(r.forget(), NS_DISPATCH_SYNC);
  }

  if (!sFileLoaded) {
    return false;
  }

  if (sMessageBlacklist.Length() == 0) {
    return false;
  }

  return sMessageBlacklist.Contains(aMessageName);
}

// static
std::vector<uint8_t> Faulty::GetDataFromIPCMessage(IPC::Message* aMsg) {
  const Pickle::BufferList& buffers = aMsg->Buffers();
  std::vector<uint8_t> data;
  data.reserve(buffers.Size());

  Pickle::BufferList::IterImpl i = buffers.Iter();
  while (!i.Done()) {
    size_t s = i.RemainingInSegment();
    data.insert(data.end(), i.Data(), i.Data() + s);

    i.Advance(buffers, s);
  }

  return data;
}

// static
void Faulty::CopyFDs(IPC::Message* aDstMsg, IPC::Message* aSrcMsg) {
#ifndef _WINDOWS
  FileDescriptorSet* dstFdSet = aDstMsg->file_descriptor_set();
  FileDescriptorSet* srcFdSet = aSrcMsg->file_descriptor_set();
  for (size_t i = 0; i < srcFdSet->size(); i++) {
    int fd = srcFdSet->GetDescriptorAt(i);
    dstFdSet->Add(fd);
  }
#endif
}

IPC::Message* Faulty::MutateIPCMessage(const char* aChannel, IPC::Message* aMsg,
                                       unsigned int aProbability) {
  if (!mIsValidProcessType || !mFuzzMessages) {
    return aMsg;
  }

  sMsgCounter += 1;
  LogMessage(aChannel, aMsg);

  /* Skip immediately if we shall not try to fuzz this message. */
  if (!FuzzingTraits::Sometimes(aProbability)) {
    return aMsg;
  }

  const bool isMessageListed = IsMessageNameBlacklisted(aMsg->name());

  /* Check if this message is blacklisted and shall not get fuzzed. */
  if (isMessageListed && !mUseAsWhitelist) {
    FAULTY_LOG("BLACKLISTED: %s", aMsg->name());
    return aMsg;
  }

  /* Check if the message is whitelisted. */
  if (!isMessageListed && mUseAsWhitelist) {
    /* Silently skip this message. */
    return aMsg;
  }

  /* Retrieve BufferLists as data from original message. */
  std::vector<uint8_t> data(GetDataFromIPCMessage(aMsg));

  /* Check if there is enough data in the message to fuzz. */
  uint32_t headerSize = aMsg->HeaderSizeFromData(nullptr, nullptr);
  if (headerSize == data.size()) {
    FAULTY_LOG("IGNORING: %s", aMsg->name());
    return aMsg;
  }

  /* Mutate the message data. */
  size_t maxMutations = FuzzingTraits::Frequency(data.size(), MutationFactor());
  FAULTY_LOG("FUZZING (%zu bytes): %s", maxMutations, aMsg->name());
  while (maxMutations--) {
    /* Ignore the header data of the message. */
    uint32_t pos = RandomIntegerRange<uint32_t>(headerSize, data.size() - 1);
    switch (FuzzingTraits::Random(6)) {
      case 0:
        break;
      case 1:
        data.at(pos) = RandomIntegerRange<uint8_t>(0, 1);
        break;
      case 2:
        data.at(pos) ^= (1 << FuzzingTraits::Random(9));
        break;
      case 3:
        data.at(pos) = RandomIntegerRange<uint8_t>(254, 255);
        break;
      default:
        data.at(pos) = RandomIntegerRange<uint8_t>(0, 255);
    }
  }

  /* Build new message. */
  auto* mutatedMsg =
      new IPC::Message(reinterpret_cast<const char*>(data.data()), data.size());
  CopyFDs(mutatedMsg, aMsg);

  /* Dump original message for diff purposes. */
  DumpMessage(aChannel, aMsg, nsPrintfCString(".%zu.o", sMsgCounter).get());
  /* Dump mutated message for diff purposes. */
  DumpMessage(aChannel, mutatedMsg,
              nsPrintfCString(".%zu.m", sMsgCounter).get());

  delete aMsg;

  return mutatedMsg;
}

void Faulty::LogMessage(const char* aChannel, IPC::Message* aMsg) {
  if (!mIsValidProcessType) {
    return;
  }

  std::string fileName =
      nsPrintfCString("message.%u.%zu", getpid(), sMsgCounter).get();

  FAULTY_LOG("Process: %u | Size: %10zu | %-20s | %s => %s",
             XRE_GetProcessType(), aMsg->Buffers().Size(), fileName.c_str(),
             aChannel, aMsg->name());
}

void Faulty::DumpMessage(const char* aChannel, IPC::Message* aMsg,
                         std::string aAppendix) {
  if (!mIsValidProcessType || !mMessagePath) {
    return;
  }

  std::vector<uint8_t> data(GetDataFromIPCMessage(aMsg));
  std::string fileName;

  if (!aAppendix.empty()) {
    fileName = nsPrintfCString("%s/message.%u%s", mMessagePath, getpid(),
                               aAppendix.c_str())
                   .get();
  } else {
    fileName = nsPrintfCString("%s/%s", mMessagePath, fileName.c_str()).get();
  }

  std::fstream fp;
  fp.open(fileName, std::fstream::out | std::fstream::binary);
  fp.write(reinterpret_cast<const char*>(data.data()), data.size());
  fp.close();
}

}  // namespace ipc
}  // namespace mozilla
