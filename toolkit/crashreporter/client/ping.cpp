/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "crashreporter.h"

#include <cstring>
#include <string>

#if defined(XP_LINUX)
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#elif defined(XP_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(XP_WIN)
#include <objbase.h>
#endif

#include "json/json.h"

using std::string;

namespace CrashReporter {

struct UUID {
    uint32_t m0;
    uint16_t m1;
    uint16_t m2;
    uint8_t  m3[8];
};

// Generates an UUID; the code here is mostly copied from nsUUIDGenerator.cpp
static string
GenerateUUID()
{
  UUID id = {};

#if defined(XP_WIN) // Windows
  HRESULT hr = CoCreateGuid((GUID*)&id);
  if (FAILED(hr)) {
    return "";
  }
#elif defined(XP_MACOSX) // MacOS X
  CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
  if (!uuid) {
    return "";
  }

  CFUUIDBytes bytes = CFUUIDGetUUIDBytes(uuid);
  memcpy(&id, &bytes, sizeof(UUID));

  CFRelease(uuid);
#elif defined(HAVE_ARC4RANDOM_BUF) // Android, BSD, ...
  arc4random_buf(id, sizeof(UUID));
#else // Linux
  int fd = open("/dev/urandom", O_RDONLY);

  if (fd == -1) {
    return "";
  }

  if (read(fd, &id, sizeof(UUID)) != sizeof(UUID)) {
    close(fd);
    return "";
  }

  close(fd);
#endif

  /* Put in the version */
  id.m2 &= 0x0fff;
  id.m2 |= 0x4000;

  /* Put in the variant */
  id.m3[0] &= 0x3f;
  id.m3[0] |= 0x80;

  const char* kUUIDFormatString =
    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x";
  const size_t kUUIDFormatStringLength = 36;
  char str[kUUIDFormatStringLength + 1] = { '\0' };

  int num = snprintf(str, kUUIDFormatStringLength + 1, kUUIDFormatString,
                     id.m0, id.m1, id.m2, id.m3[0], id.m3[1], id.m3[2],
                     id.m3[3], id.m3[4], id.m3[5], id.m3[6], id.m3[7]);

  if (num != kUUIDFormatStringLength) {
    return "";
  }

  return str;
}

const char kISO8601Date[] = "%F";
const char kISO8601DateHours[] = "%FT%H:00:00.000Z";

// Return the current date as a string in the specified format, the following
// constants are provided:
// - kISO8601Date, the ISO 8601 date format, YYYY-MM-DD
// - kISO8601DateHours, the ISO 8601 full date format, YYYY-MM-DDTHH:00:00.000Z
static string
CurrentDate(string format)
{
  time_t now;
  time(&now);
  char buf[64]; // This should be plenty
  strftime(buf, sizeof buf, format.c_str(), gmtime(&now));
  return buf;
}

const char kTelemetryClientId[]  = "TelemetryClientId";
const char kTelemetryUrl[]       = "TelemetryServerURL";
const char kTelemetrySessionId[] = "TelemetrySessionId";
const int  kTelemetryVersion     = 4;

// Create the payload.metadata node of the crash ping using fields extracted
// from the .extra file
static Json::Value
CreateMetadataNode(StringTable& strings)
{
  // The following list should be kept in sync with the one in CrashManager.jsm
  const char *entries[] = {
    "AsyncShutdownTimeout",
    "AvailablePageFile",
    "AvailablePhysicalMemory",
    "AvailableVirtualMemory",
    "BlockedDllList",
    "BlocklistInitFailed",
    "BuildID",
    "ContainsMemoryReport",
    "CrashTime",
    "EventLoopNestingLevel",
    "ipc_channel_error",
    "IsGarbageCollecting",
    "LowCommitSpaceEvents",
    "MozCrashReason",
    "OOMAllocationSize",
    "ProductID",
    "ProductName",
    "ReleaseChannel",
    "RemoteType",
    "SecondsSinceLastCrash",
    "ShutdownProgress",
    "StartupCrash",
    "SystemMemoryUsePercentage",
    "TextureUsage",
    "TotalPageFile",
    "TotalPhysicalMemory",
    "TotalVirtualMemory",
    "UptimeTS",
    "User32BeforeBlocklist",
    "Version",
  };

  Json::Value node;

  for (auto entry : entries) {
    if ((strings.find(entry) != strings.end()) && !strings[entry].empty()) {
      node[entry] = strings[entry];
    }
  }

  return node;
}

// Create the payload node of the crash ping
static Json::Value
CreatePayloadNode(StringTable& strings, const string& aHash,
                  const string& aSessionId)
{
  Json::Value payload;

  payload["sessionId"] = aSessionId;
  payload["version"] = 1;
  payload["crashDate"] = CurrentDate(kISO8601Date);
  payload["crashTime"] = CurrentDate(kISO8601DateHours);
  payload["hasCrashEnvironment"] = true;
  payload["crashId"] = GetDumpLocalID();
  payload["minidumpSha256Hash"] = aHash;
  payload["processType"] = "main"; // This is always a main crash

  // Parse the stack traces
  Json::Value stackTracesValue;
  Json::Reader reader;

  if (reader.parse(strings["StackTraces"], stackTracesValue,
                   /* collectComments */ false)) {
    payload["stackTraces"] = stackTracesValue;
  }

  // Assemble the payload metadata
  payload["metadata"] = CreateMetadataNode(strings);

  return payload;
}

// Create the application node of the crash ping
static Json::Value
CreateApplicationNode(const string& aVendor, const string& aName,
                      const string& aVersion, const string& aDisplayVersion,
                      const string& aPlatformVersion, const string& aChannel,
                      const string& aBuildId, const string& aArchitecture,
                      const string& aXpcomAbi)
{
  Json::Value application;

  application["vendor"] = aVendor;
  application["name"] = aName;
  application["buildId"] = aBuildId;
  application["displayVersion"] = aDisplayVersion;
  application["platformVersion"] = aPlatformVersion;
  application["version"] = aVersion;
  application["channel"] = aChannel;
  if (!aArchitecture.empty()) {
    application["architecture"] = aArchitecture;
  }
  if (!aXpcomAbi.empty()) {
    application["xpcomAbi"] = aXpcomAbi;
  }

  return application;
}

// Create the root node of the crash ping
static Json::Value
CreateRootNode(StringTable& strings, const string& aUuid, const string& aHash,
               const string& aClientId, const string& aSessionId,
               const string& aName, const string& aVersion,
               const string& aChannel, const string& aBuildId)
{
  Json::Value root;
  root["type"] = "crash"; // This is a crash ping
  root["id"] = aUuid;
  root["version"] = kTelemetryVersion;
  root["creationDate"] = CurrentDate(kISO8601DateHours);
  root["clientId"] = aClientId;

  // Parse the telemetry environment
  Json::Value environment;
  Json::Reader reader;
  string architecture;
  string xpcomAbi;
  string displayVersion;
  string platformVersion;

  if (reader.parse(strings["TelemetryEnvironment"], environment,
                   /* collectComments */ false)) {
    if (environment.isMember("build") && environment["build"].isObject()) {
      Json::Value build = environment["build"];
      if (build.isMember("architecture") && build["architecture"].isString()) {
        architecture = build["architecture"].asString();
      }
      if (build.isMember("xpcomAbi") && build["xpcomAbi"].isString()) {
        xpcomAbi = build["xpcomAbi"].asString();
      }
      if (build.isMember("displayVersion") && build["displayVersion"].isString()) {
        displayVersion = build["displayVersion"].asString();
      }
      if (build.isMember("platformVersion") && build["platformVersion"].isString()) {
        platformVersion = build["platformVersion"].asString();
      }
    }

    root["environment"] = environment;
  }

  root["payload"] = CreatePayloadNode(strings, aHash, aSessionId);
  root["application"] = CreateApplicationNode(strings["Vendor"], aName,
                                              aVersion, displayVersion,
                                              platformVersion,
                                              aChannel, aBuildId,
                                              architecture, xpcomAbi);

  return root;
}

// Generates the URL used to submit the crash ping, see TelemetrySend.jsm
string
GenerateSubmissionUrl(const string& aUrl, const string& aId,
                      const string& aName, const string& aVersion,
                      const string& aChannel, const string& aBuildId)
{
  return aUrl + "/submit/telemetry/" + aId + "/crash/" + aName + "/" +
         aVersion + "/" + aChannel + "/" + aBuildId +
         "?v=" + std::to_string(kTelemetryVersion);
}

// Write out the ping into the specified file.
//
// Returns true if the ping was written out successfully, false otherwise.
static bool
WritePing(const string& aPath, const string& aPing)
{
  ofstream* f = UIOpenWrite(aPath.c_str());
  bool success = false;

  if (f->is_open()) {
    *f << aPing;
    f->close();
    success = f->good();
  }

  delete f;
  return success;
}

// Assembles the crash ping using the strings extracted from the .extra file
// and sends it using the crash sender. All the telemetry specific data but the
// environment will be stripped from the annotations so that it won't be sent
// together with the crash report.
//
// Note that the crash ping sender is invoked in a fire-and-forget way so this
// won't block waiting for the ping to be delivered.
//
// Returns true if the ping was assembled and handed over to the pingsender
// correctly, false otherwise and populates the aUUID field with the ping UUID.
bool
SendCrashPing(StringTable& strings, const string& aHash, string& pingUuid,
              const string& pingDir)
{
  string clientId    = strings[kTelemetryClientId];
  string serverUrl   = strings[kTelemetryUrl];
  string sessionId   = strings[kTelemetrySessionId];

  // Remove the telemetry-related data from the crash annotations
  strings.erase(kTelemetryClientId);
  strings.erase(kTelemetryUrl);
  strings.erase(kTelemetrySessionId);

  string buildId     = strings["BuildID"];
  string channel     = strings["ReleaseChannel"];
  string name        = strings["ProductName"];
  string version     = strings["Version"];
  string uuid        = GenerateUUID();
  string url         = GenerateSubmissionUrl(serverUrl, uuid, name, version,
                                             channel, buildId);

  if (serverUrl.empty() || uuid.empty()) {
    return false;
  }

  Json::Value root = CreateRootNode(strings, uuid, aHash, clientId, sessionId,
                                    name, version, channel, buildId);

  // Write out the result to the pending pings directory
  Json::FastWriter writer;
  string ping = writer.write(root);
  string pingPath = pingDir + UI_DIR_SEPARATOR + uuid + ".json";

  if (!WritePing(pingPath, ping)) {
    return false;
  }

  // Hand over the ping to the sender
  vector<string> args = { url, pingPath };
  if (UIRunProgram(GetProgramPath(UI_PING_SENDER_FILENAME), args)) {
    pingUuid = uuid;
    return true;
  } else {
    return false;
  }
}

} // namespace crashreporter

