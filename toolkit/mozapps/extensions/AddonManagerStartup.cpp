/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonManagerStartup.h"
#include "AddonManagerStartup-inlines.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/TracingAPI.h"
#include "xpcpublic.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Compression.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsContentUtils.h"
#include "nsIAddonInterposition.h"
#include "nsIIOService.h"
#include "nsIJARProtocolHandler.h"
#include "nsIStringEnumerator.h"
#include "nsIZipReader.h"
#include "nsReadableUtils.h"
#include "nsXULAppAPI.h"

#include <stdlib.h>

namespace mozilla {

template <>
class MOZ_MUST_USE_TYPE GenericErrorResult<nsresult>
{
  nsresult mErrorValue;

  template<typename V, typename E2> friend class Result;

public:
  explicit GenericErrorResult(nsresult aErrorValue) : mErrorValue(aErrorValue) {}

  operator nsresult() { return mErrorValue; }
};

static inline Result<Ok, nsresult>
WrapNSResult(PRStatus aRv)
{
    if (aRv != PR_SUCCESS) {
        return Err(NS_ERROR_FAILURE);
    }
    return Ok();
}

static inline Result<Ok, nsresult>
WrapNSResult(nsresult aRv)
{
    if (NS_FAILED(aRv)) {
        return Err(aRv);
    }
    return Ok();
}

#define NS_TRY(expr) MOZ_TRY(WrapNSResult(expr))


using Compression::LZ4;
using dom::ipc::StructuredCloneData;

#ifdef XP_WIN
#  define READ_BINARYMODE "rb"
#else
#  define READ_BINARYMODE "r"
#endif

AddonManagerStartup&
AddonManagerStartup::GetSingleton()
{
  static RefPtr<AddonManagerStartup> singleton;
  if (!singleton) {
    singleton = new AddonManagerStartup();
    ClearOnShutdown(&singleton);
  }
  return *singleton;
}

AddonManagerStartup::AddonManagerStartup()
  : mInitialized(false)
{}


nsIFile*
AddonManagerStartup::ProfileDir()
{
  if (!mProfileDir) {
    nsresult rv;

    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mProfileDir));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }

  return mProfileDir;
}

NS_IMPL_ISUPPORTS(AddonManagerStartup, amIAddonManagerStartup)


/*****************************************************************************
 * File utils
 *****************************************************************************/

static already_AddRefed<nsIFile>
CloneAndAppend(nsIFile* aFile, const char* name)
{
  nsCOMPtr<nsIFile> file;
  aFile->Clone(getter_AddRefs(file));
  file->AppendNative(nsDependentCString(name));
  return file.forget();
}

static bool
IsNormalFile(nsIFile* file)
{
  bool result;
  return NS_SUCCEEDED(file->IsFile(&result)) && result;
}

static nsCString
ReadFile(const char* path)
{
  nsCString result;

  FILE* fd = fopen(path, READ_BINARYMODE);
  if (!fd) {
    return result;
  }
  auto cleanup = MakeScopeExit([&] () {
    fclose(fd);
  });

  if (fseek(fd, 0, SEEK_END) != 0) {
    return result;
  }
  size_t len = ftell(fd);
  if (len <= 0 || fseek(fd, 0, SEEK_SET) != 0) {
    return result;
  }

  result.SetLength(len);
  size_t rd = fread(result.BeginWriting(), sizeof(char), len, fd);
  if (rd != len) {
    result.Truncate();
  }

  return result;
}

static const char STRUCTURED_CLONE_MAGIC[] = "mozJSSCLz40v001";

template <typename T>
static Result<nsCString, nsresult>
DecodeLZ4(const nsACString& lz4, const T& magicNumber)
{
  constexpr auto HEADER_SIZE = sizeof(magicNumber) + 4;

  // Note: We want to include the null terminator here.
  nsDependentCSubstring magic(magicNumber, sizeof(magicNumber));

  if (lz4.Length() < HEADER_SIZE || StringHead(lz4, magic.Length()) != magic) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  auto data = lz4.BeginReading() + magic.Length();
  auto size = LittleEndian::readUint32(data);
  data += 4;

  nsCString result;
  if (!result.SetLength(size, fallible) ||
      !LZ4::decompress(data, result.BeginWriting(), size)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  return result;
}

// Our zlib headers redefine this to MOZ_Z_compress, which breaks LZ4::compress
#undef compress

template <typename T>
static Result<nsCString, nsresult>
EncodeLZ4(const nsACString& data, const T& magicNumber)
{
  // Note: We want to include the null terminator here.
  nsDependentCSubstring magic(magicNumber, sizeof(magicNumber));

  nsAutoCString result;
  result.Append(magic);

  auto off = result.Length();
  result.SetLength(off + 4);

  LittleEndian::writeUint32(result.BeginWriting() + off, data.Length());
  off += 4;

  auto size = LZ4::maxCompressedSize(data.Length());
  result.SetLength(off + size);

  size = LZ4::compress(data.BeginReading(), data.Length(),
                       result.BeginWriting() + off);

  result.SetLength(off + size);
  return result;
}

static_assert(sizeof STRUCTURED_CLONE_MAGIC % 8 == 0,
              "Magic number should be an array of uint64_t");

/**
 * Reads the contents of a LZ4-compressed file, as stored by the OS.File
 * module, and returns the decompressed contents on success.
 *
 * A nonexistent or empty file is treated as success. A corrupt or non-LZ4
 * file is treated as failure.
 */
static Result<nsCString, nsresult>
ReadFileLZ4(const char* path)
{
  static const char MAGIC_NUMBER[] = "mozLz40";

  nsCString result;

  nsCString lz4 = ReadFile(path);
  if (lz4.IsEmpty()) {
    return result;
  }

  return DecodeLZ4(lz4, MAGIC_NUMBER);
}

static bool
ParseJSON(JSContext* cx, nsACString& jsonData, JS::MutableHandleValue result)
{
  NS_ConvertUTF8toUTF16 str(jsonData);
  jsonData.Truncate();

  return JS_ParseJSON(cx, str.Data(), str.Length(), result);
}

static Result<nsCOMPtr<nsIZipReaderCache>, nsresult>
GetJarCache()
{
  nsCOMPtr<nsIIOService> ios = services::GetIOService();
  NS_ENSURE_TRUE(ios, Err(NS_ERROR_FAILURE));

  nsCOMPtr<nsIProtocolHandler> jarProto;
  NS_TRY(ios->GetProtocolHandler("jar", getter_AddRefs(jarProto)));

  nsCOMPtr<nsIJARProtocolHandler> jar = do_QueryInterface(jarProto);
  MOZ_ASSERT(jar);

  nsCOMPtr<nsIZipReaderCache> zipCache;
  NS_TRY(jar->GetJARCache(getter_AddRefs(zipCache)));

  return Move(zipCache);
}


/*****************************************************************************
 * JSON data handling
 *****************************************************************************/

class MOZ_STACK_CLASS WrapperBase {
protected:
  WrapperBase(JSContext* cx, JSObject* object)
    : mCx(cx)
    , mObject(cx, object)
  {}

  WrapperBase(JSContext* cx, const JS::Value& value)
    : mCx(cx)
    , mObject(cx)
  {
    if (value.isObject()) {
      mObject = &value.toObject();
    } else {
      mObject = JS_NewPlainObject(cx);
    }
  }

protected:
  JSContext* mCx;
  JS::RootedObject mObject;

  bool GetBool(const char* name, bool defVal = false);

  double GetNumber(const char* name, double defVal = 0);

  nsString GetString(const char* name, const char* defVal = "");

  JSObject* GetObject(const char* name);
};

bool
WrapperBase::GetBool(const char* name, bool defVal)
{
  JS::RootedObject obj(mCx, mObject);

  JS::RootedValue val(mCx, JS::UndefinedValue());
  if (!JS_GetProperty(mCx, obj, name, &val)) {
    JS_ClearPendingException(mCx);
  }

  if (val.isBoolean()) {
    return val.toBoolean();
  }
  return defVal;
}

double
WrapperBase::GetNumber(const char* name, double defVal)
{
  JS::RootedObject obj(mCx, mObject);

  JS::RootedValue val(mCx, JS::UndefinedValue());
  if (!JS_GetProperty(mCx, obj, name, &val)) {
    JS_ClearPendingException(mCx);
  }

  if (val.isNumber()) {
    return val.toNumber();
  }
  return defVal;
}

nsString
WrapperBase::GetString(const char* name, const char* defVal)
{
  JS::RootedObject obj(mCx, mObject);

  JS::RootedValue val(mCx, JS::UndefinedValue());
  if (!JS_GetProperty(mCx, obj, name, &val)) {
    JS_ClearPendingException(mCx);
  }

  nsString res;
  if (val.isString()) {
    AssignJSString(mCx, res, val.toString());
  } else {
    res.AppendASCII(defVal);
  }
  return res;
}

JSObject*
WrapperBase::GetObject(const char* name)
{
  JS::RootedObject obj(mCx, mObject);

  JS::RootedValue val(mCx, JS::UndefinedValue());
  if (!JS_GetProperty(mCx, obj, name, &val)) {
    JS_ClearPendingException(mCx);
  }

  if (val.isObject()) {
    return &val.toObject();
  }
  return nullptr;
}


class MOZ_STACK_CLASS InstallLocation : public WrapperBase {
public:
  InstallLocation(JSContext* cx, const JS::Value& value);

  MOZ_IMPLICIT InstallLocation(PropertyIterElem& iter)
    : InstallLocation(iter.Cx(), iter.Value())
  {}

  InstallLocation(const InstallLocation& other)
    : InstallLocation(other.mCx, JS::ObjectValue(*other.mObject))
  {}

  void SetChanged(bool changed)
  {
    JS::RootedObject obj(mCx, mObject);

    JS::RootedValue val(mCx, JS::BooleanValue(changed));
    if (!JS_SetProperty(mCx, obj, "changed", val)) {
      JS_ClearPendingException(mCx);
    }
  }

  PropertyIter& Addons() { return mAddonsIter.ref(); }

  nsString Path() { return GetString("path"); }

  bool ShouldCheckStartupModifications() { return GetBool("checkStartupModifications"); }


private:
  JS::RootedObject mAddonsObj;
  Maybe<PropertyIter> mAddonsIter;
};


class MOZ_STACK_CLASS Addon : public WrapperBase {
public:
  Addon(JSContext* cx, InstallLocation& location, const nsAString& id, JSObject* object)
    : WrapperBase(cx, object)
    , mId(id)
    , mLocation(location)
  {}

  MOZ_IMPLICIT Addon(PropertyIterElem& iter)
    : WrapperBase(iter.Cx(), iter.Value())
    , mId(iter.Name())
    , mLocation(*static_cast<InstallLocation*>(iter.Context()))
  {}

  Addon(const Addon& other)
    : WrapperBase(other.mCx, other.mObject)
    , mId(other.mId)
    , mLocation(other.mLocation)
  {}

  const nsString& Id() { return mId; }

  nsString Path() { return GetString("path"); }

  bool Bootstrapped() { return GetBool("bootstrapped"); }

  bool Enabled() { return GetBool("enabled"); }

  bool ShimsEnabled() { return GetBool("enableShims"); }

  double LastModifiedTime() { return GetNumber("lastModifiedTime"); }


  Result<nsCOMPtr<nsIFile>, nsresult> FullPath();

  NSLocationType LocationType();

  Result<bool, nsresult> UpdateLastModifiedTime();


private:
  nsString mId;
  InstallLocation& mLocation;
};

Result<nsCOMPtr<nsIFile>, nsresult>
Addon::FullPath()
{
  nsString path = Path();

  // First check for an absolute path, in case we have a proxy file.
  nsCOMPtr<nsIFile> file;
  if (NS_SUCCEEDED(NS_NewLocalFile(path, false, getter_AddRefs(file)))) {
    return Move(file);
  }

  // If not an absolute path, fall back to a relative path from the location.
  NS_TRY(NS_NewLocalFile(mLocation.Path(), false, getter_AddRefs(file)));

  NS_TRY(file->AppendRelativePath(path));
  return Move(file);
}

NSLocationType
Addon::LocationType()
{
  nsString type = GetString("type", "extension");
  if (type.LowerCaseEqualsLiteral("theme")) {
    return NS_SKIN_LOCATION;
  }
  return NS_EXTENSION_LOCATION;
}

Result<bool, nsresult>
Addon::UpdateLastModifiedTime()
{
  nsCOMPtr<nsIFile> file;
  MOZ_TRY_VAR(file, FullPath());

  bool result;
  if (NS_FAILED(file->Exists(&result)) || !result) {
    return true;
  }

  PRTime time;

  nsCOMPtr<nsIFile> manifest = file;
  if (!IsNormalFile(manifest)) {
    manifest = CloneAndAppend(file, "install.rdf");
    if (!IsNormalFile(manifest)) {
      manifest = CloneAndAppend(file, "manifest.json");
      if (!IsNormalFile(manifest)) {
        return true;
      }
    }
  }

  if (NS_FAILED(manifest->GetLastModifiedTime(&time))) {
    return true;
  }

  JS::RootedObject obj(mCx, mObject);

  double lastModified = time;
  JS::RootedValue value(mCx, JS::NumberValue(lastModified));
  if (!JS_SetProperty(mCx, obj, "currentModifiedTime", value)) {
    JS_ClearPendingException(mCx);
  }

  return lastModified != LastModifiedTime();;
}


InstallLocation::InstallLocation(JSContext* cx, const JS::Value& value)
  : WrapperBase(cx, value)
  , mAddonsObj(cx)
  , mAddonsIter()
{
  mAddonsObj = GetObject("addons");
  if (!mAddonsObj) {
    mAddonsObj = JS_NewPlainObject(cx);
  }
  mAddonsIter.emplace(cx, mAddonsObj, this);
}


/*****************************************************************************
 * XPC interfacing
 *****************************************************************************/

static void
EnableShims(const nsAString& addonId)
{
  NS_ConvertUTF16toUTF8 id(addonId);

  nsCOMPtr<nsIAddonInterposition> interposition =
     do_GetService("@mozilla.org/addons/multiprocess-shims;1");

  if (!interposition || !xpc::SetAddonInterposition(id, interposition)) {
    return;
  }

  Unused << xpc::AllowCPOWsInAddon(id, true);
}

Result<Ok, nsresult>
AddonManagerStartup::AddInstallLocation(Addon& addon)
{
  nsCOMPtr<nsIFile> file;
  MOZ_TRY_VAR(file, addon.FullPath());

  nsString path;
  NS_TRY(file->GetPath(path));

  auto type = addon.LocationType();

  if (type == NS_SKIN_LOCATION) {
    mThemePaths.AppendElement(file);
  } else {
    mExtensionPaths.AppendElement(file);
  }

  if (StringTail(path, 4).LowerCaseEqualsLiteral(".xpi")) {
    XRE_AddJarManifestLocation(type, file);
  } else {
    nsCOMPtr<nsIFile> manifest = CloneAndAppend(file, "chrome.manifest");
    XRE_AddManifestLocation(type, manifest);
  }
  return Ok();
}

nsresult
AddonManagerStartup::ReadStartupData(JSContext* cx, JS::MutableHandleValue locations)
{
  locations.set(JS::UndefinedValue());

  nsCOMPtr<nsIFile> file = CloneAndAppend(ProfileDir(), "addonStartup.json.lz4");

  nsCString path;
  NS_TRY(file->GetNativePath(path));

  nsCString data;
  MOZ_TRY_VAR(data, ReadFileLZ4(path.get()));

  if (data.IsEmpty() || !ParseJSON(cx, data, locations)) {
    return NS_OK;
  }

  if (!locations.isObject()) {
    return NS_ERROR_UNEXPECTED;
  }

  JS::RootedObject locs(cx, &locations.toObject());
  for (auto e1 : PropertyIter(cx, locs)) {
    InstallLocation loc(e1);

    if (!loc.ShouldCheckStartupModifications()) {
      continue;
    }

    for (auto e2 : loc.Addons()) {
      Addon addon(e2);

      if (addon.Enabled()) {
        bool changed;
        MOZ_TRY_VAR(changed, addon.UpdateLastModifiedTime());
        if (changed) {
          loc.SetChanged(true);
        }
      }
    }
  }

  return NS_OK;
}

nsresult
AddonManagerStartup::InitializeExtensions(JS::HandleValue locations, JSContext* cx)
{
  NS_ENSURE_FALSE(mInitialized, NS_ERROR_UNEXPECTED);
  NS_ENSURE_TRUE(locations.isObject(), NS_ERROR_INVALID_ARG);

  mInitialized = true;

  if (!Preferences::GetBool("extensions.defaultProviders.enabled", true)) {
    return NS_OK;
  }

  bool enableInterpositions = Preferences::GetBool("extensions.interposition.enabled", false);

  JS::RootedObject locs(cx, &locations.toObject());
  for (auto e1 : PropertyIter(cx, locs)) {
    InstallLocation loc(e1);

    for (auto e2 : loc.Addons()) {
      Addon addon(e2);

      if (addon.Enabled() && !addon.Bootstrapped()) {
        Unused << AddInstallLocation(addon);

        if (enableInterpositions && addon.ShimsEnabled()) {
          EnableShims(addon.Id());
        }
      }
    }
  }

  return NS_OK;
}

nsresult
AddonManagerStartup::EncodeBlob(JS::HandleValue value, JSContext* cx, JS::MutableHandleValue result)
{
  StructuredCloneData holder;

  ErrorResult rv;
  holder.Write(cx, value, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  nsAutoCString scData;

  auto& data = holder.Data();
  auto iter = data.Iter();
  while (!iter.Done()) {
    scData.Append(nsDependentCSubstring(iter.Data(), iter.RemainingInSegment()));
    iter.Advance(data, iter.RemainingInSegment());
  }

  nsCString lz4;
  MOZ_TRY_VAR(lz4, EncodeLZ4(scData, STRUCTURED_CLONE_MAGIC));

  JS::RootedObject obj(cx);
  NS_TRY(nsContentUtils::CreateArrayBuffer(cx, lz4, &obj.get()));

  result.set(JS::ObjectValue(*obj));
  return NS_OK;
}

nsresult
AddonManagerStartup::DecodeBlob(JS::HandleValue value, JSContext* cx, JS::MutableHandleValue result)
{
  NS_ENSURE_TRUE(value.isObject() &&
                 JS_IsArrayBufferObject(&value.toObject()) &&
                 JS_ArrayBufferHasData(&value.toObject()),
                 NS_ERROR_INVALID_ARG);

  StructuredCloneData holder;

  nsCString data;
  {
    JS::AutoCheckCannotGC nogc;

    auto obj = &value.toObject();
    bool isShared;

    nsDependentCSubstring lz4(
      reinterpret_cast<char*>(JS_GetArrayBufferData(obj, &isShared, nogc)),
      JS_GetArrayBufferByteLength(obj));

    MOZ_TRY_VAR(data, DecodeLZ4(lz4, STRUCTURED_CLONE_MAGIC));
  }

  bool ok = holder.CopyExternalData(data.get(), data.Length());
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

  ErrorResult rv;
  holder.Read(cx, result, rv);
  return rv.StealNSResult();;
}

nsresult
AddonManagerStartup::EnumerateZipFile(nsIFile* file, const nsACString& pattern,
                                      uint32_t* countOut, char16_t*** entriesOut)
{
  NS_ENSURE_ARG_POINTER(file);
  NS_ENSURE_ARG_POINTER(countOut);
  NS_ENSURE_ARG_POINTER(entriesOut);

  nsCOMPtr<nsIZipReaderCache> zipCache;
  MOZ_TRY_VAR(zipCache, GetJarCache());

  nsCOMPtr<nsIZipReader> zip;
  NS_TRY(zipCache->GetZip(file, getter_AddRefs(zip)));

  nsCOMPtr<nsIUTF8StringEnumerator> entries;
  NS_TRY(zip->FindEntries(pattern, getter_AddRefs(entries)));

  nsTArray<nsString> results;
  bool hasMore;
  while (NS_SUCCEEDED(entries->HasMore(&hasMore)) && hasMore) {
    nsAutoCString name;
    NS_TRY(entries->GetNext(name));

    results.AppendElement(NS_ConvertUTF8toUTF16(name));
  }

  auto strResults = MakeUnique<char16_t*[]>(results.Length());
  for (uint32_t i = 0; i < results.Length(); i++) {
    strResults[i] = ToNewUnicode(results[i]);
  }

  *countOut = results.Length();
  *entriesOut = strResults.release();

  return NS_OK;
}

nsresult
AddonManagerStartup::Reset()
{
  MOZ_RELEASE_ASSERT(xpc::IsInAutomation());

  mInitialized = false;

  mExtensionPaths.Clear();
  mThemePaths.Clear();

  return NS_OK;
}

} // namespace mozilla
