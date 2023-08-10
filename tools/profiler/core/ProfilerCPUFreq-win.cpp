/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerCPUFreq.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#ifdef DEBUG
#  include "nsPrintfCString.h"
#endif

#include <stdio.h>
#include <strsafe.h>
#include <winperf.h>

#pragma comment(lib, "advapi32.lib")

using namespace mozilla;

ProfilerCPUFreq::ProfilerCPUFreq() {
  // Query the size of the text data so you can allocate the buffer.
  DWORD dwBufferSize = 0;
  LONG status = RegQueryValueEx(HKEY_PERFORMANCE_DATA, L"Counter 9", NULL, NULL,
                                NULL, &dwBufferSize);
  if (ERROR_SUCCESS != status) {
    NS_WARNING(nsPrintfCString("RegQueryValueEx failed getting required buffer "
                               "size. Error is 0x%lx.\n",
                               status)
                   .get());
    return;
  }

  // Allocate the text buffer and query the text.
  LPWSTR pBuffer = (LPWSTR)malloc(dwBufferSize);
  if (!pBuffer) {
    NS_WARNING("failed to allocate buffer");
    return;
  }
  status = RegQueryValueEx(HKEY_PERFORMANCE_DATA, L"Counter 9", NULL, NULL,
                           (LPBYTE)pBuffer, &dwBufferSize);
  if (ERROR_SUCCESS != status) {
    NS_WARNING(
        nsPrintfCString("RegQueryValueEx failed with 0x%lx.\n", status).get());
    free(pBuffer);
    return;
  }

  LPWSTR pwszCounterText = pBuffer;  // Used to cycle through the Counter text
  // Ignore first pair.
  pwszCounterText += (wcslen(pwszCounterText) + 1);
  pwszCounterText += (wcslen(pwszCounterText) + 1);

  for (; *pwszCounterText; pwszCounterText += (wcslen(pwszCounterText) + 1)) {
    // Keep a pointer to the counter index, to read the index later if the name
    // is the one we are looking for.
    LPWSTR counterIndex = pwszCounterText;
    pwszCounterText += (wcslen(pwszCounterText) + 1);  // Skip past index value

    if (!wcscmp(L"Processor Information", pwszCounterText)) {
      mBlockIndex = _wcsdup(counterIndex);
    } else if (!wcscmp(L"% Processor Performance", pwszCounterText)) {
      mCounterNameIndex = _wtoi(counterIndex);
      if (mBlockIndex) {
        // We have found all the indexes we were looking for.
        break;
      }
    }
  }
  free(pBuffer);

  if (!mBlockIndex) {
    NS_WARNING("index of the performance counter block not found");
    return;
  }

  mBuffer = (LPBYTE)malloc(mBufferSize);
  if (!mBuffer) {
    NS_WARNING("failed to allocate initial buffer");
    return;
  }
  dwBufferSize = mBufferSize;

  // Typically RegQueryValueEx will set the size variable to the required size.
  // But this does not work when querying object index values, and the buffer
  // size has to be increased in a loop until RegQueryValueEx no longer returns
  // ERROR_MORE_DATA.
  while (ERROR_MORE_DATA ==
         (status = RegQueryValueEx(HKEY_PERFORMANCE_DATA, mBlockIndex, NULL,
                                   NULL, mBuffer, &dwBufferSize))) {
    mBufferSize *= 2;
    auto* oldBuffer = mBuffer;
    mBuffer = (LPBYTE)realloc(mBuffer, mBufferSize);
    if (!mBuffer) {
      NS_WARNING("failed to reallocate buffer");
      free(oldBuffer);
      return;
    }
    dwBufferSize = mBufferSize;
  }

  if (ERROR_SUCCESS != status) {
    NS_WARNING(nsPrintfCString("RegQueryValueEx failed getting required buffer "
                               "size. Error is 0x%lx.\n",
                               status)
                   .get());
    free(mBuffer);
    mBuffer = nullptr;
    return;
  }

  PERF_DATA_BLOCK* dataBlock = (PERF_DATA_BLOCK*)mBuffer;
  LPBYTE pObject = mBuffer + dataBlock->HeaderLength;
  PERF_OBJECT_TYPE* object = (PERF_OBJECT_TYPE*)pObject;
  PERF_COUNTER_DEFINITION* counter = nullptr;
  {
    PERF_COUNTER_DEFINITION* pCounter =
        (PERF_COUNTER_DEFINITION*)(pObject + object->HeaderLength);
    for (DWORD i = 0; i < object->NumCounters; i++) {
      if (mCounterNameIndex == pCounter->CounterNameTitleIndex) {
        counter = pCounter;
        break;
      }
      pCounter++;
    }
  }
  if (!counter || !mCPUCounters.resize(GetNumberOfProcessors())) {
    NS_WARNING("failing to find counter or resize the mCPUCounters vector");
    free(mBuffer);
    mBuffer = nullptr;
    return;
  }

  MOZ_ASSERT(counter->CounterType == PERF_AVERAGE_BULK);
  PERF_COUNTER_DEFINITION* baseCounter = counter + 1;
  MOZ_ASSERT((baseCounter->CounterType & PERF_COUNTER_BASE) ==
             PERF_COUNTER_BASE);

  PERF_INSTANCE_DEFINITION* instanceDef =
      (PERF_INSTANCE_DEFINITION*)(pObject + object->DefinitionLength);
  for (LONG i = 0; i < object->NumInstances; i++) {
    PERF_COUNTER_BLOCK* counterBlock =
        (PERF_COUNTER_BLOCK*)((LPBYTE)instanceDef + instanceDef->ByteLength);

    LPWSTR name = (LPWSTR)(((LPBYTE)instanceDef) + instanceDef->NameOffset);
    unsigned int cpuId, coreId;
    if (swscanf(name, L"%u,%u", &cpuId, &coreId) == 2 && cpuId == 0 &&
        coreId < mCPUCounters.length()) {
      auto& CPUCounter = mCPUCounters[coreId];
      CPUCounter.data = *(UNALIGNED ULONGLONG*)((LPBYTE)counterBlock +
                                                counter->CounterOffset);
      CPUCounter.base =
          *(DWORD*)((LPBYTE)counterBlock + baseCounter->CounterOffset);

      // Now get the nominal core frequency.
      HKEY key;
      nsAutoString keyName(
          L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\");
      keyName.AppendInt(coreId);

      if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName.get(), 0, KEY_QUERY_VALUE,
                       &key) == ERROR_SUCCESS) {
        DWORD data, len;
        len = sizeof(data);

        if (RegQueryValueEx(key, L"~Mhz", 0, 0, reinterpret_cast<LPBYTE>(&data),
                            &len) == ERROR_SUCCESS) {
          CPUCounter.nominalFrequency = data;
        }
      }
    }
    instanceDef = (PERF_INSTANCE_DEFINITION*)((LPBYTE)counterBlock +
                                              counterBlock->ByteLength);
  }
}

ProfilerCPUFreq::~ProfilerCPUFreq() {
  RegCloseKey(HKEY_PERFORMANCE_DATA);
  free(mBlockIndex);
  mBlockIndex = nullptr;
  free(mBuffer);
  mBuffer = nullptr;
}

void ProfilerCPUFreq::Sample() {
  DWORD dwBufferSize = mBufferSize;
  if (!mBuffer ||
      (ERROR_SUCCESS != RegQueryValueEx(HKEY_PERFORMANCE_DATA, mBlockIndex,
                                        NULL, NULL, mBuffer, &dwBufferSize))) {
    NS_WARNING("failed to query performance data");
    return;
  }

  PERF_DATA_BLOCK* dataBlock = (PERF_DATA_BLOCK*)mBuffer;
  LPBYTE pObject = mBuffer + dataBlock->HeaderLength;
  PERF_OBJECT_TYPE* object = (PERF_OBJECT_TYPE*)pObject;
  PERF_COUNTER_DEFINITION* counter = nullptr;
  {
    PERF_COUNTER_DEFINITION* pCounter =
        (PERF_COUNTER_DEFINITION*)(pObject + object->HeaderLength);
    for (DWORD i = 0; i < object->NumCounters; i++) {
      if (mCounterNameIndex == pCounter->CounterNameTitleIndex) {
        counter = pCounter;
        break;
      }
      pCounter++;
    }
  }
  if (!counter) {
    NS_WARNING("failed to find counter");
    return;
  }

  MOZ_ASSERT(counter->CounterType == PERF_AVERAGE_BULK);
  PERF_COUNTER_DEFINITION* baseCounter = counter + 1;
  MOZ_ASSERT((baseCounter->CounterType & PERF_COUNTER_BASE) ==
             PERF_COUNTER_BASE);

  PERF_INSTANCE_DEFINITION* instanceDef =
      (PERF_INSTANCE_DEFINITION*)(pObject + object->DefinitionLength);
  for (LONG i = 0; i < object->NumInstances; i++) {
    PERF_COUNTER_BLOCK* counterBlock =
        (PERF_COUNTER_BLOCK*)((LPBYTE)instanceDef + instanceDef->ByteLength);

    LPWSTR name = (LPWSTR)(((LPBYTE)instanceDef) + instanceDef->NameOffset);
    unsigned int cpuId, coreId;
    if (swscanf(name, L"%u,%u", &cpuId, &coreId) == 2 && cpuId == 0 &&
        coreId < mCPUCounters.length()) {
      auto& CPUCounter = mCPUCounters[coreId];
      ULONGLONG prevData = CPUCounter.data;
      DWORD prevBase = CPUCounter.base;
      CPUCounter.data = *(UNALIGNED ULONGLONG*)((LPBYTE)counterBlock +
                                                counter->CounterOffset);
      CPUCounter.base =
          *(DWORD*)((LPBYTE)counterBlock + baseCounter->CounterOffset);
      if (prevBase && prevBase != CPUCounter.base) {
        CPUCounter.freq = CPUCounter.nominalFrequency *
                          (CPUCounter.data - prevData) /
                          (CPUCounter.base - prevBase) / 1000 * 10;
      }
    }
    instanceDef = (PERF_INSTANCE_DEFINITION*)((LPBYTE)counterBlock +
                                              counterBlock->ByteLength);
  }
}
