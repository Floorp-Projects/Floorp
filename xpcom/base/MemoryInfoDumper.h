/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=50 et cin tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MemoryInfoDumper_h
#define mozilla_MemoryInfoDumper_h

#include "nsString.h"
#include "mozilla/StandardInteger.h"
class nsIGZFileWriter;

namespace mozilla {

/**
 * This class facilitates dumping information about our memory usage to disk.
 *
 * Its cpp file also has Linux-only code which watches various OS signals and
 * dumps memory info upon receiving a signal.  You can activate these listeners
 * by calling Initialize().
 */
class MemoryInfoDumper
{
public:
  static void Initialize();

  /**
   * DumpMemoryReportsToFile dumps the memory reports for this process and
   * possibly our child processes (and their children, recursively) to a file in
   * the tmp directory called memory-reports-<identifier>-<pid>.json.gz (or
   * something similar, such as memory-reports-<identifier>-<pid>-1.json.gz; no
   * existing file will be overwritten).
   *
   * @param identifier this identifier will appear in the filename of our
   *   about:memory dump and those of our children (if dumpChildProcesses is
   *   true).
   *
   *   If the identifier is empty, the dumpMemoryReportsToFile implementation
   *   may set it arbitrarily and use that new value for its own dump and the
   *   dumps of its child processes.  For example, the dumpMemoryReportsToFile
   *   implementation may set |identifier| to the number of seconds since the
   *   epoch.
   *
   * @param minimizeMemoryUsage indicates whether we should run a series of
   *   gc/cc's in an attempt to reduce our memory usage before collecting our
   *   memory report.
   *
   * @param dumpChildProcesses indicates whether we should call
   *   dumpMemoryReportsToFile in our child processes.  If so, the child
   *   processes will also dump their children, and so on.
   *
   *
   * Sample output:
   *
   * {
   *   "hasMozMallocUsableSize":true,
   *   "reports": [
   *     {"process":"", "path":"explicit/foo/bar", "kind":1, "units":0,
   *      "amount":2000000, "description":"Foo bar."},
   *     {"process":"", "path":"heap-allocated", "kind":1, "units":0,
   *      "amount":3000000, "description":"Heap allocated."},
   *     {"process":"", "path":"vsize", "kind":1, "units":0,
   *      "amount":10000000, "description":"Vsize."}
   *   ]
   * }
   *
   * JSON schema for the output.
   *
   * {
   *   "properties": {
   *     "hasMozMallocUsableSize": {
   *       "type": "boolean",
   *       "description": "nsIMemoryReporterManager::hasMozMallocUsableSize",
   *       "required": true
   *     },
   *     "reports": {
   *       "type": "array",
   *       "description": "The memory reports.",
   *       "required": true
   *       "minItems": 1,
   *       "items": {
   *         "type": "object",
   *         "properties": {
   *           "process": {
   *             "type": "string",
   *             "description": "nsIMemoryReporter::process",
   *             "required": true
   *           },
   *           "path": {
   *             "type": "string",
   *             "description": "nsIMemoryReporter::path",
   *             "required": true,
   *             "minLength": 1
   *           },
   *           "kind": {
   *             "type": "integer",
   *             "description": "nsIMemoryReporter::kind",
   *             "required": true
   *           },
   *           "units": {
   *             "type": "integer",
   *             "description": "nsIMemoryReporter::units",
   *             "required": true
   *           },
   *           "amount": {
   *             "type": "integer",
   *             "description": "nsIMemoryReporter::amount",
   *             "required": true
   *           },
   *           "description": {
   *             "type": "string",
   *             "description": "nsIMemoryReporter::description",
   *             "required": true
   *           }
   *         }
   *       }
   *     }
   *   }
   * }
   */
  static void
  DumpMemoryReportsToFile(const nsAString& aIdentifier,
                          bool aMinimizeMemoryUsage,
                          bool aDumpChildProcesses);

  /**
   * Dump GC and CC logs to files in the OS's temp directory (or in
   * $MOZ_CC_LOG_DIRECTORY, if that environment variable is specified).
   *
   * @param aIdentifier If aIdentifier is non-empty, this string will appear in
   *   the filenames of the logs we create (both for this process and, if
   *   aDumpChildProcesses is true, for our child processes).
   *
   *   If aIdentifier is empty, the implementation may set it to an
   *   arbitrary value; for example, it may set aIdentifier to the number
   *   of seconds since the epoch.
   *
   * @param aDumpChildProcesses indicates whether we should call
   *   DumpGCAndCCLogsToFile in our child processes.  If so, the child processes
   *   will dump their children, and so on.
   */
  static void
  DumpGCAndCCLogsToFile(const nsAString& aIdentifier,
                        bool aDumpChildProcesses);

private:
  static nsresult
  DumpMemoryReportsToFileImpl(const nsAString& aIdentifier);
};

} // namespace mozilla
#endif
