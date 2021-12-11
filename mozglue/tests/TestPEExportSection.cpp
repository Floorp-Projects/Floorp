/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// This test makes sure mozilla::nt::PEExportSection can parse the export
// section of a local process, and a remote process even though it's
// modified by an external code.

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/NativeNt.h"
#include "nsWindowsDllInterceptor.h"

#include <stdio.h>
#include <windows.h>

#define EXPORT_FUNCTION_EQ(name, func) \
  (GetProcAddress(imageBase, name) == reinterpret_cast<void*>(func))

#define VERIFY_EXPORT_FUNCTION(tables, name, expected, errorMessage)        \
  do {                                                                      \
    if (tables.GetProcAddress(name) != reinterpret_cast<void*>(expected)) { \
      printf("TEST-FAILED | TestPEExportSection | %s", errorMessage);       \
      return kTestFail;                                                     \
    }                                                                       \
  } while (0)

using namespace mozilla::nt;
using mozilla::interceptor::MMPolicyInProcess;
using mozilla::interceptor::MMPolicyOutOfProcess;
using LocalPEExportSection = PEExportSection<MMPolicyInProcess>;
using RemotePEExportSection = PEExportSection<MMPolicyOutOfProcess>;

constexpr DWORD kEventTimeoutinMs = 5000;
const wchar_t kProcessControlEventName[] =
    L"TestPEExportSection.Process.Control.Event";

enum TestResult : int {
  kTestSuccess = 0,
  kTestFail,
  kTestSkip,
};

// These strings start with the same keyword to make sure we don't do substring
// match.  Moreover, kSecretFunctionInvalid is purposely longer than the
// combination of the other two strings and located in between the other two
// strings to effectively test binary search.
const char kSecretFunction[] = "Secret";
const char kSecretFunctionInvalid[] = "Secret invalid long name";
const char kSecretFunctionWithSuffix[] = "Secret2";

const wchar_t* kNoModification = L"--NoModification";
const wchar_t* kNoExport = L"--NoExport";
const wchar_t* kModifyTableEntry = L"--ModifyTableEntry";
const wchar_t* kModifyTable = L"--ModifyTable";
const wchar_t* kModifyDirectoryEntry = L"--ModifyDirectoryEntry";
const wchar_t* kExportByOrdinal = L"--ExportByOrdinal";

// Use the global variable to pass the child process's error status to the
// parent process.  We don't use a process's exit code to keep the test simple.
int gChildProcessStatus = 0;

// These functions are exported by linker or export section tampering at
// runtime.  Each of function bodies needs to be different to avoid ICF.
extern "C" __declspec(dllexport) int Export1() { return 0; }
extern "C" __declspec(dllexport) int Export2() { return 1; }
int SecretFunction1() { return 100; }
int SecretFunction2() { return 101; }

// This class allocates a writable region downstream of the mapped image
// and prepares it as a valid export section.
class ExportDirectoryPatcher final {
  static constexpr int kRegionAllocationTryLimit = 100;
  static constexpr int kNumOfTableEntries = 2;
  // VirtualAlloc sometimes fails if a desired base address is too small.
  // Define a minimum desired base to reduce the number of allocation tries.
  static constexpr uintptr_t kMinimumAllocationPoint = 0x8000000;

  struct ExportDirectory {
    IMAGE_EXPORT_DIRECTORY mDirectoryHeader;
    DWORD mExportAddressTable[kNumOfTableEntries];
    DWORD mExportNameTable[kNumOfTableEntries];
    WORD mExportOrdinalTable[kNumOfTableEntries];
    char mNameBuffer1[sizeof(kSecretFunction)];
    char mNameBuffer2[sizeof(kSecretFunctionWithSuffix)];

    template <typename T>
    static DWORD PtrToRVA(T aPtr, uintptr_t aBase) {
      return reinterpret_cast<uintptr_t>(aPtr) - aBase;
    }

    explicit ExportDirectory(uintptr_t aImageBase) : mDirectoryHeader{} {
      mDirectoryHeader.Base = 1;
      mExportAddressTable[0] = PtrToRVA(SecretFunction1, aImageBase);
      mExportAddressTable[1] = PtrToRVA(SecretFunction2, aImageBase);
      mExportNameTable[0] = PtrToRVA(mNameBuffer1, aImageBase);
      mExportNameTable[1] = PtrToRVA(mNameBuffer2, aImageBase);
      mExportOrdinalTable[0] = 0;
      mExportOrdinalTable[1] = 1;
      strcpy(mNameBuffer1, kSecretFunction);
      strcpy(mNameBuffer2, kSecretFunctionWithSuffix);
    }
  };

  uintptr_t mImageBase;
  ExportDirectory* mNewExportDirectory;

  DWORD PtrToRVA(const void* aPtr) const {
    return reinterpret_cast<uintptr_t>(aPtr) - mImageBase;
  }

 public:
  explicit ExportDirectoryPatcher(HMODULE aModule)
      : mImageBase(PEHeaders::HModuleToBaseAddr<uintptr_t>(aModule)),
        mNewExportDirectory(nullptr) {
    SYSTEM_INFO si = {};
    ::GetSystemInfo(&si);

    int numPagesRequired = ((sizeof(ExportDirectory) - 1) / si.dwPageSize) + 1;

    uintptr_t desiredBase = mImageBase + si.dwAllocationGranularity;
    desiredBase = std::max(desiredBase, kMinimumAllocationPoint);

    for (int i = 0; i < kRegionAllocationTryLimit; ++i) {
      void* allocated =
          ::VirtualAlloc(reinterpret_cast<void*>(desiredBase),
                         numPagesRequired * si.dwPageSize,
                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      if (allocated) {
        // Use the end of a allocated page as ExportDirectory in order to test
        // the boundary between a commit page and a non-commited page.
        allocated = reinterpret_cast<uint8_t*>(allocated) +
                    (numPagesRequired * si.dwPageSize) -
                    sizeof(ExportDirectory);
        mNewExportDirectory = new (allocated) ExportDirectory(mImageBase);
        return;
      }

      desiredBase += si.dwAllocationGranularity;
    }

    gChildProcessStatus = kTestSkip;
    printf(
        "TEST-SKIP | TestPEExportSection | "
        "Giving up finding an allocatable space following the mapped image.\n");
  }

  ~ExportDirectoryPatcher() {
    // Intentionally leave mNewExportDirectory leaked to keep a patched data
    // available until the process is terminated.
  }

  explicit operator bool() const { return !!mNewExportDirectory; }

  void PopulateDirectory(IMAGE_EXPORT_DIRECTORY& aOutput) const {
    aOutput.NumberOfFunctions = aOutput.NumberOfNames = kNumOfTableEntries;
    aOutput.AddressOfFunctions =
        PtrToRVA(mNewExportDirectory->mExportAddressTable);
    aOutput.AddressOfNames = PtrToRVA(mNewExportDirectory->mExportNameTable);
    aOutput.AddressOfNameOrdinals =
        PtrToRVA(mNewExportDirectory->mExportOrdinalTable);
  }

  void PopulateDirectoryEntry(IMAGE_DATA_DIRECTORY& aOutput) const {
    PopulateDirectory(mNewExportDirectory->mDirectoryHeader);
    aOutput.VirtualAddress = PtrToRVA(&mNewExportDirectory->mDirectoryHeader);
    aOutput.Size = sizeof(ExportDirectory);
  }
};

// This exports SecretFunction1 as "Export1" by replacing an entry of the
// export address table.
void ModifyExportAddressTableEntry() {
  MMPolicyInProcess policy;
  HMODULE imageBase = ::GetModuleHandleW(nullptr);
  auto ourExe = LocalPEExportSection::Get(imageBase, policy);

  auto addressTableEntry =
      const_cast<DWORD*>(ourExe.FindExportAddressTableEntry("Export1"));
  if (!addressTableEntry) {
    gChildProcessStatus = kTestFail;
    return;
  }

  mozilla::AutoVirtualProtect protection(
      addressTableEntry, sizeof(*addressTableEntry), PAGE_READWRITE);
  if (!protection) {
    gChildProcessStatus = kTestFail;
    return;
  }

  *addressTableEntry = reinterpret_cast<uintptr_t>(SecretFunction1) -
                       PEHeaders::HModuleToBaseAddr<uintptr_t>(imageBase);

  if (!EXPORT_FUNCTION_EQ("Export1", SecretFunction1) ||
      !EXPORT_FUNCTION_EQ("Export2", Export2)) {
    gChildProcessStatus = kTestFail;
  }
}

// This switches the entire address table into one exporting SecretFunction1
// and SecretFunction2.
void ModifyExportAddressTable() {
  MMPolicyInProcess policy;
  HMODULE imageBase = ::GetModuleHandleW(nullptr);
  auto ourExe = LocalPEExportSection::Get(imageBase, policy);

  auto exportDirectory = ourExe.GetExportDirectory();
  if (!exportDirectory) {
    gChildProcessStatus = kTestFail;
    return;
  }

  mozilla::AutoVirtualProtect protection(
      exportDirectory, sizeof(*exportDirectory), PAGE_READWRITE);
  if (!protection) {
    gChildProcessStatus = kTestFail;
    return;
  }

  ExportDirectoryPatcher patcher(imageBase);
  if (!patcher) {
    return;
  }

  patcher.PopulateDirectory(*exportDirectory);

  if (GetProcAddress(imageBase, "Export1") ||
      GetProcAddress(imageBase, "Export2") ||
      !EXPORT_FUNCTION_EQ(kSecretFunction, SecretFunction1) ||
      !EXPORT_FUNCTION_EQ(kSecretFunctionWithSuffix, SecretFunction2)) {
    gChildProcessStatus = kTestFail;
  }
}

// This hides all export functions by setting the table size to 0.
void HideExportSection() {
  HMODULE imageBase = ::GetModuleHandleW(nullptr);
  PEHeaders ourExe(imageBase);

  auto sectionTable =
      ourExe.GetImageDirectoryEntryPtr(IMAGE_DIRECTORY_ENTRY_EXPORT);

  mozilla::AutoVirtualProtect protection(sectionTable, sizeof(*sectionTable),
                                         PAGE_READWRITE);
  if (!protection) {
    gChildProcessStatus = kTestFail;
    return;
  }

  sectionTable->VirtualAddress = sectionTable->Size = 0;

  if (GetProcAddress(imageBase, "Export1") ||
      GetProcAddress(imageBase, "Export2")) {
    gChildProcessStatus = kTestFail;
  }
}

// This makes the export directory entry point to a new export section
// which exports SecretFunction1 and SecretFunction2.
void ModifyExportDirectoryEntry() {
  HMODULE imageBase = ::GetModuleHandleW(nullptr);
  PEHeaders ourExe(imageBase);

  auto sectionTable =
      ourExe.GetImageDirectoryEntryPtr(IMAGE_DIRECTORY_ENTRY_EXPORT);

  mozilla::AutoVirtualProtect protection(sectionTable, sizeof(*sectionTable),
                                         PAGE_READWRITE);
  if (!protection) {
    gChildProcessStatus = kTestFail;
    return;
  }

  ExportDirectoryPatcher patcher(imageBase);
  if (!patcher) {
    return;
  }

  patcher.PopulateDirectoryEntry(*sectionTable);

  if (GetProcAddress(imageBase, "Export1") ||
      GetProcAddress(imageBase, "Export2") ||
      !EXPORT_FUNCTION_EQ(kSecretFunction, SecretFunction1) ||
      !EXPORT_FUNCTION_EQ(kSecretFunctionWithSuffix, SecretFunction2)) {
    gChildProcessStatus = kTestFail;
  }
}

// This exports functions only by Ordinal by hiding the export name table.
void ExportByOrdinal() {
  ModifyExportDirectoryEntry();
  if (gChildProcessStatus != kTestSuccess) {
    return;
  }

  MMPolicyInProcess policy;
  HMODULE imageBase = ::GetModuleHandleW(nullptr);
  auto ourExe = LocalPEExportSection::Get(imageBase, policy);

  auto exportDirectory = ourExe.GetExportDirectory();
  if (!exportDirectory) {
    gChildProcessStatus = kTestFail;
    return;
  }

  exportDirectory->NumberOfNames = 0;

  if (GetProcAddress(imageBase, "Export1") ||
      GetProcAddress(imageBase, "Export2") ||
      GetProcAddress(imageBase, kSecretFunction) ||
      GetProcAddress(imageBase, kSecretFunctionWithSuffix) ||
      !EXPORT_FUNCTION_EQ(MAKEINTRESOURCE(1), SecretFunction1) ||
      !EXPORT_FUNCTION_EQ(MAKEINTRESOURCE(2), SecretFunction2)) {
    gChildProcessStatus = kTestFail;
  }
}

class ChildProcess final {
  nsAutoHandle mChildProcess;
  nsAutoHandle mChildMainThread;

 public:
  static int Main(const nsAutoHandle& aEvent, const wchar_t* aOption) {
    if (wcscmp(aOption, kNoModification) == 0) {
      ;
    } else if (wcscmp(aOption, kNoExport) == 0) {
      HideExportSection();
    } else if (wcscmp(aOption, kModifyTableEntry) == 0) {
      ModifyExportAddressTableEntry();
    } else if (wcscmp(aOption, kModifyTable) == 0) {
      ModifyExportAddressTable();
    } else if (wcscmp(aOption, kModifyDirectoryEntry) == 0) {
      ModifyExportDirectoryEntry();
    } else if (wcscmp(aOption, kExportByOrdinal) == 0) {
      ExportByOrdinal();
    }

    // Letting the parent process know the child process is ready.
    ::SetEvent(aEvent);

    // The child process does not exit itself.  It's force terminated by
    // the parent process when all tests are done.
    for (;;) {
      ::Sleep(100);
    }
    return 0;
  }

  ChildProcess(const wchar_t* aExecutable, const wchar_t* aOption,
               const nsAutoHandle& aEvent, const nsAutoHandle& aJob) {
    const wchar_t* childArgv[] = {aExecutable, aOption};
    auto cmdLine(
        mozilla::MakeCommandLine(mozilla::ArrayLength(childArgv), childArgv));

    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    BOOL ok = ::CreateProcessW(aExecutable, cmdLine.get(), nullptr, nullptr,
                               FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!ok) {
      printf(
          "TEST-FAILED | TestPEExportSection | "
          "CreateProcessW falied - %08lx.\n",
          GetLastError());
      return;
    }

    if (aJob && !::AssignProcessToJobObject(aJob, pi.hProcess)) {
      printf(
          "TEST-FAILED | TestPEExportSection | "
          "AssignProcessToJobObject falied - %08lx.\n",
          GetLastError());
      ::TerminateProcess(pi.hProcess, 1);
      return;
    }

    // Wait until requested modification is done in the child process.
    if (::WaitForSingleObject(aEvent, kEventTimeoutinMs) != WAIT_OBJECT_0) {
      printf(
          "TEST-FAILED | TestPEExportSection | "
          "Child process was not ready in time.\n");
      return;
    }

    mChildProcess.own(pi.hProcess);
    mChildMainThread.own(pi.hThread);
  }

  ~ChildProcess() { ::TerminateProcess(mChildProcess, 0); }

  operator HANDLE() const { return mChildProcess; }

  TestResult GetStatus() const {
    TestResult status = kTestSuccess;
    if (!::ReadProcessMemory(mChildProcess, &gChildProcessStatus, &status,
                             sizeof(status), nullptr)) {
      status = kTestFail;
      printf(
          "TEST-FAILED | TestPEExportSection | "
          "ReadProcessMemory failed - %08lx\n",
          GetLastError());
    }
    return status;
  }
};

template <typename MMPolicy>
TestResult BasicTest(const MMPolicy& aMMPolicy) {
  const bool isAppHelpLoaded = ::GetModuleHandleW(L"apphelp.dll");

  // Use ntdll.dll because it does not have any forwarder RVA.
  HMODULE ntdllImageBase = ::GetModuleHandleW(L"ntdll.dll");
  auto ntdllExports = PEExportSection<MMPolicy>::Get(ntdllImageBase, aMMPolicy);

  auto exportDir = ntdllExports.GetExportDirectory();
  auto tableOfNames =
      ntdllExports.template RVAToPtr<const PDWORD>(exportDir->AddressOfNames);
  for (DWORD i = 0; i < exportDir->NumberOfNames; ++i) {
    const auto name =
        ntdllExports.template RVAToPtr<const char*>(tableOfNames[i]);

    if (isAppHelpLoaded && strcmp(name, "NtdllDefWindowProc_W") == 0) {
      // In this case, GetProcAddress will return
      // apphelp!DWM8AND16BitHook_DefWindowProcW.
      continue;
    }

    auto funcEntry = ntdllExports.FindExportAddressTableEntry(name);
    if (ntdllExports.template RVAToPtr<const void*>(*funcEntry) !=
        ::GetProcAddress(ntdllImageBase, name)) {
      printf(
          "TEST-FAILED | TestPEExportSection | "
          "FindExportAddressTableEntry did not resolve ntdll!%s.\n",
          name);
      return kTestFail;
    }
  }

  for (DWORD i = 0; i < 0x10000; i += 0x10) {
    if (ntdllExports.GetProcAddress(MAKEINTRESOURCE(i)) !=
        ::GetProcAddress(ntdllImageBase, MAKEINTRESOURCE(i))) {
      printf(
          "TEST-FAILED | TestPEExportSection | "
          "GetProcAddress did not resolve ntdll!Ordinal#%lu.\n",
          i);
      return kTestFail;
    }
  }

  // Test a known forwarder RVA.
  auto k32Exports = PEExportSection<MMPolicy>::Get(
      ::GetModuleHandleW(L"kernel32.dll"), aMMPolicy);
  if (k32Exports.FindExportAddressTableEntry("HeapAlloc")) {
    printf(
        "TEST-FAILED | TestPEExportSection | "
        "kernel32!HeapAlloc should be forwarded to ntdll!RtlAllocateHeap.\n");
    return kTestFail;
  }

  // Test invalid names.
  if (k32Exports.FindExportAddressTableEntry("Invalid name") ||
      k32Exports.FindExportAddressTableEntry("")) {
    printf(
        "TEST-FAILED | TestPEExportSection | "
        "FindExportAddressTableEntry should return "
        "nullptr for a non-existent name.\n");
    return kTestFail;
  }

  return kTestSuccess;
}

TestResult RunChildProcessTest(
    const wchar_t* aExecutable, const wchar_t* aOption,
    const nsAutoHandle& aEvent, const nsAutoHandle& aJob,
    TestResult (*aTestCallback)(const RemotePEExportSection&)) {
  ChildProcess childProcess(aExecutable, aOption, aEvent, aJob);
  if (!childProcess) {
    return kTestFail;
  }

  auto result = childProcess.GetStatus();
  if (result != kTestSuccess) {
    return result;
  }

  MMPolicyOutOfProcess policy(childProcess);

  // One time is enough to run BasicTest in the child process.
  static TestResult oneTimeResult = BasicTest<MMPolicyOutOfProcess>(policy);
  if (oneTimeResult != kTestSuccess) {
    return oneTimeResult;
  }

  auto exportTableChild =
      RemotePEExportSection::Get(::GetModuleHandleW(nullptr), policy);
  return aTestCallback(exportTableChild);
}

mozilla::LauncherResult<nsReturnRef<HANDLE>> CreateJobToLimitProcessLifetime() {
  uint64_t version;
  PEHeaders ntdllHeaders(::GetModuleHandleW(L"ntdll.dll"));
  if (!ntdllHeaders.GetVersionInfo(version)) {
    printf(
        "TEST-FAILED | TestPEExportSection | "
        "Unable to obtain version information from ntdll.dll\n");
    return LAUNCHER_ERROR_FROM_LAST();
  }

  constexpr uint64_t kWin8 = 0x60002ull << 32;
  nsAutoHandle job;

  if (version < kWin8) {
    // Since a process can be associated only with a single job in Win7 or
    // older and this test program is already assigned with a job by
    // infrastructure, we cannot use a job.
    return job.out();
  }

  job.own(::CreateJobObject(nullptr, nullptr));
  if (!job) {
    printf(
        "TEST-FAILED | TestPEExportSection | "
        "CreateJobObject falied - %08lx.\n",
        GetLastError());
    return LAUNCHER_ERROR_FROM_LAST();
  }

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
  jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

  if (!::SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                 &jobInfo, sizeof(jobInfo))) {
    printf(
        "TEST-FAILED | TestPEExportSection | "
        "SetInformationJobObject falied - %08lx.\n",
        GetLastError());
    return LAUNCHER_ERROR_FROM_LAST();
  }

  return job.out();
}

extern "C" int wmain(int argc, wchar_t* argv[]) {
  nsAutoHandle controlEvent(
      ::CreateEventW(nullptr, FALSE, FALSE, kProcessControlEventName));

  if (argc == 2) {
    return ChildProcess::Main(controlEvent, argv[1]);
  }

  if (argc != 1) {
    printf(
        "TEST-FAILED | TestPEExportSection | "
        "Invalid arguments.\n");
    return kTestFail;
  }

  MMPolicyInProcess policy;
  if (BasicTest<MMPolicyInProcess>(policy)) {
    return kTestFail;
  }

  auto exportTableSelf =
      LocalPEExportSection::Get(::GetModuleHandleW(nullptr), policy);
  if (!exportTableSelf) {
    printf(
        "TEST-FAILED | TestPEExportSection | "
        "LocalPEExportSection::Get failed.\n");
    return kTestFail;
  }

  VERIFY_EXPORT_FUNCTION(exportTableSelf, "Export1", Export1,
                         "Local | Export1 was not exported.\n");
  VERIFY_EXPORT_FUNCTION(exportTableSelf, "Export2", Export2,
                         "Local | Export2 was not exported.\n");
  VERIFY_EXPORT_FUNCTION(
      exportTableSelf, "Invalid name", 0,
      "Local | GetProcAddress should return nullptr for an invalid name.\n");

  // We'll add the child process to a job so that, in the event of a failure in
  // this parent process, the child process will be automatically terminated.
  auto probablyJob = CreateJobToLimitProcessLifetime();
  if (probablyJob.isErr()) {
    return kTestFail;
  }

  nsAutoHandle job(probablyJob.unwrap());

  auto result = RunChildProcessTest(
      argv[0], kNoModification, controlEvent, job,
      [](const RemotePEExportSection& aTables) {
        VERIFY_EXPORT_FUNCTION(aTables, "Export1", Export1,
                               "NoModification | Export1 was not exported.\n");
        VERIFY_EXPORT_FUNCTION(aTables, "Export2", Export2,
                               "NoModification | Export2 was not exported.\n");
        return kTestSuccess;
      });
  if (result == kTestFail) {
    return result;
  }

  result = RunChildProcessTest(
      argv[0], kNoExport, controlEvent, job,
      [](const RemotePEExportSection& aTables) {
        VERIFY_EXPORT_FUNCTION(aTables, "Export1", 0,
                               "NoExport | Export1 was exported.\n");
        VERIFY_EXPORT_FUNCTION(aTables, "Export2", 0,
                               "NoExport | Export2 was exported.\n");
        return kTestSuccess;
      });
  if (result == kTestFail) {
    return result;
  }

  result = RunChildProcessTest(
      argv[0], kModifyTableEntry, controlEvent, job,
      [](const RemotePEExportSection& aTables) {
        VERIFY_EXPORT_FUNCTION(
            aTables, "Export1", SecretFunction1,
            "ModifyTableEntry | SecretFunction1 was not exported.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, "Export2", Export2,
            "ModifyTableEntry | Export2 was not exported.\n");
        return kTestSuccess;
      });
  if (result == kTestFail) {
    return result;
  }

  result = RunChildProcessTest(
      argv[0], kModifyTable, controlEvent, job,
      [](const RemotePEExportSection& aTables) {
        VERIFY_EXPORT_FUNCTION(aTables, "Export1", 0,
                               "ModifyTable | Export1 was exported.\n");
        VERIFY_EXPORT_FUNCTION(aTables, "Export2", 0,
                               "ModifyTable | Export2 was exported.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, kSecretFunction, SecretFunction1,
            "ModifyTable | SecretFunction1 was not exported.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, kSecretFunctionWithSuffix, SecretFunction2,
            "ModifyTable | SecretFunction2 was not exported.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, kSecretFunctionInvalid, 0,
            "ModifyTable | kSecretFunctionInvalid was exported.\n");
        return kTestSuccess;
      });
  if (result == kTestFail) {
    return result;
  }

  result = RunChildProcessTest(
      argv[0], kModifyDirectoryEntry, controlEvent, job,
      [](const RemotePEExportSection& aTables) {
        VERIFY_EXPORT_FUNCTION(
            aTables, "Export1", 0,
            "ModifyDirectoryEntry | Export1 was exported.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, "Export2", 0,
            "ModifyDirectoryEntry | Export2 was exported.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, kSecretFunction, SecretFunction1,
            "ModifyDirectoryEntry | SecretFunction1 was not exported.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, kSecretFunctionWithSuffix, SecretFunction2,
            "ModifyDirectoryEntry | SecretFunction2 was not exported.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, kSecretFunctionInvalid, 0,
            "ModifyDirectoryEntry | kSecretFunctionInvalid was exported.\n");
        return kTestSuccess;
      });
  if (result == kTestFail) {
    return result;
  }

  result = RunChildProcessTest(
      argv[0], kExportByOrdinal, controlEvent, job,
      [](const RemotePEExportSection& aTables) {
        VERIFY_EXPORT_FUNCTION(aTables, "Export1", 0,
                               "ExportByOrdinal | Export1 was exported.\n");
        VERIFY_EXPORT_FUNCTION(aTables, "Export2", 0,
                               "ExportByOrdinal | Export2 was exported.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, kSecretFunction, 0,
            "ModifyDirectoryEntry | kSecretFunction was exported by name.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, kSecretFunctionWithSuffix, 0,
            "ModifyDirectoryEntry | "
            "kSecretFunctionWithSuffix was exported by name.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, MAKEINTRESOURCE(1), SecretFunction1,
            "ModifyDirectoryEntry | "
            "kSecretFunction was not exported by ordinal.\n");
        VERIFY_EXPORT_FUNCTION(
            aTables, MAKEINTRESOURCE(2), SecretFunction2,
            "ModifyDirectoryEntry | "
            "kSecretFunctionWithSuffix was not exported by ordinal.\n");
        return kTestSuccess;
      });
  if (result == kTestFail) {
    return result;
  }

  return kTestSuccess;
}
