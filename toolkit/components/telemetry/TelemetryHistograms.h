/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Taras Glek <tglek@mozilla.com>
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

/**
 * This file lists Telemetry histograms collected by Firefox.  The format is
 *
 *    HISTOGRAM(id, minium, maximum, bucket count, histogram kind,
 *              human-readable description for about:telemetry)
 *
 */

HISTOGRAM(CYCLE_COLLECTOR, 1, 10000, 50, EXPONENTIAL, "Time(ms) spent on cycle collection")
HISTOGRAM(TELEMETRY_PING, 1, 3000, 10, EXPONENTIAL, "Time(ms) taken to submit telemetry info")
HISTOGRAM(TELEMETRY_SUCCESS, 0, 1, 2, BOOLEAN,  "Success(No, Yes) rate of telemetry submissions")
HISTOGRAM(MEMORY_JS_GC_HEAP, 1024, 512 * 1024, 10, EXPONENTIAL, "Memory(MB) used by the JavaScript GC")
HISTOGRAM(MEMORY_RESIDENT, 32 * 1024, 1024 * 1024, 10, EXPONENTIAL, "Resident memory(MB) reported by OS")
HISTOGRAM(MEMORY_LAYOUT_ALL, 1024, 64 * 1024, 10, EXPONENTIAL, "Memory(MB) reported used by layout")
#if defined(XP_WIN)
HISTOGRAM(EARLY_GLUESTARTUP_READ_OPS, 1, 100, 12, LINEAR, "ProcessIoCounters.ReadOperationCount before glue startup")
HISTOGRAM(EARLY_GLUESTARTUP_READ_TRANSFER, 1, 50 * 1024, 12, EXPONENTIAL, "ProcessIoCounters.ReadTransferCount before glue startup (KB)")
HISTOGRAM(GLUESTARTUP_READ_OPS, 1, 100, 12, LINEAR, "ProcessIoCounters.ReadOperationCount after glue startup")
HISTOGRAM(GLUESTARTUP_READ_TRANSFER, 1, 50 * 1024, 12, EXPONENTIAL, "ProcessIoCounters.ReadTransferCount after glue startup (KB)")
#elif defined(XP_UNIX)
HISTOGRAM(EARLY_GLUESTARTUP_HARD_FAULTS, 1, 100, 12, LINEAR, "Hard faults count before glue startup")
HISTOGRAM(GLUESTARTUP_HARD_FAULTS, 1, 500, 12, EXPONENTIAL, "Hard faults count after glue startup")
HISTOGRAM(HARD_PAGE_FAULTS, 8, 64 * 1024, 13, EXPONENTIAL, "Hard page faults (since last telemetry ping)")
#endif
HISTOGRAM(ZIPARCHIVE_CRC, 0, 1, 2, BOOLEAN, "Zip item CRC check pass")
