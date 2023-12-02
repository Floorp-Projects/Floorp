/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BackgroundTasksRunner.h"

#include "base/process_util.h"
#include "mozilla/StaticPrefs_datareporting.h"
#include "mozilla/StaticPrefs_telemetry.h"
#include "mozilla/StaticPrefs_toolkit.h"
#include "nsIFile.h"

#ifdef XP_WIN
#  include "mozilla/AssembleCmdLine.h"
#endif

#include "mozilla/ResultVariant.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(BackgroundTasksRunner, nsIBackgroundTasksRunner);

NS_IMETHODIMP BackgroundTasksRunner::RunInDetachedProcess(
    const nsACString& aTaskName, const nsTArray<nsCString>& aArgs) {
  nsCOMPtr<nsIFile> lf;
  nsresult rv = XRE_GetBinaryPath(getter_AddRefs(lf));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString exePath;
#if !defined(XP_WIN)
  rv = lf->GetNativePath(exePath);
#else
  rv = lf->GetNativeTarget(exePath);
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  base::LaunchOptions options;
#ifdef XP_WIN
  options.start_independent = true;

  nsTArray<const char*> argv = {exePath.Data(), "--backgroundtask",
                                aTaskName.Data()};
  for (const nsCString& str : aArgs) {
    argv.AppendElement(str.get());
  }
  argv.AppendElement(nullptr);

  wchar_t* assembledCmdLine = nullptr;
  if (assembleCmdLine(argv.Elements(), &assembledCmdLine, CP_UTF8) == -1) {
    return NS_ERROR_FAILURE;
  }

  if (base::LaunchApp(assembledCmdLine, options, nullptr).isErr()) {
    return NS_ERROR_FAILURE;
  }
#else
  std::vector<std::string> argv = {exePath.Data(), "--backgroundtask",
                                   aTaskName.Data()};
  for (const nsCString& str : aArgs) {
    argv.push_back(str.get());
  }

  if (base::LaunchApp(argv, std::move(options), nullptr).isErr()) {
    return NS_ERROR_FAILURE;
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP BackgroundTasksRunner::RemoveDirectoryInDetachedProcess(
    const nsACString& aParentDirPath, const nsACString& aChildDirName,
    const nsACString& aSecondsToWait, const nsACString& aOtherFoldersSuffix,
    const nsACString& aMetricsId) {
  nsTArray<nsCString> argv = {aParentDirPath + ""_ns, aChildDirName + ""_ns,
                              aSecondsToWait + ""_ns,
                              aOtherFoldersSuffix + ""_ns};

  uint32_t testingSleepMs =
      StaticPrefs::toolkit_background_tasks_remove_directory_testing_sleep_ms();
  if (testingSleepMs > 0) {
    argv.AppendElement("--test-sleep");
    nsAutoCString sleep;
    sleep.AppendInt(testingSleepMs);
    argv.AppendElement(sleep);
  }

  bool telemetryEnabled =
      StaticPrefs::datareporting_healthreport_uploadEnabled() &&
      // Talos set this to not send telemetry but still enable the code path.
      // But in this case we just disable it since this telemetry happens
      // independently from the main process and thus shouldn't be relevant to
      // performance tests.
      StaticPrefs::telemetry_fog_test_localhost_port() != -1;

  if (!aMetricsId.IsEmpty() && telemetryEnabled) {
    argv.AppendElement("--metrics-id");
    argv.AppendElement(aMetricsId);
  }

#ifdef DEBUG
  argv.AppendElement("--attach-console");
#endif

  return RunInDetachedProcess("removeDirectory"_ns, argv);
}

}  // namespace mozilla
