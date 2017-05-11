/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonManagerStartup.h"
#include "AddonManagerStartup-inlines.h"

#include "jsapi.h"
#include "js/TracingAPI.h"
#include "xpcpublic.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Compression.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsIAddonInterposition.h"
#include "nsXULAppAPI.h"

#include <stdlib.h>

namespace mozilla {

using Compression::LZ4;

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

/**
 * Reads the contents of a LZ4-compressed file, as stored by the OS.File
 * module, and stores the decompressed contents in result.
 *
 * Returns true on success, or false on failure. A nonexistent or empty file
 * is treated as success. A corrupt or non-LZ4 file is treated as failure.
 */
static bool
ReadFileLZ4(const char* path, nsCString& result)
{
  static const char MAGIC_NUMBER[] = "mozLz40";
  constexpr auto HEADER_SIZE = sizeof(MAGIC_NUMBER) + 4;

  nsCString lz4 = ReadFile(path);
  if (lz4.IsEmpty()) {
    result.Truncate();
    return true;
  }

  // Note: We want to include the null terminator here.
  nsDependentCSubstring magic(MAGIC_NUMBER, sizeof(MAGIC_NUMBER));

  if (lz4.Length() < HEADER_SIZE || StringHead(lz4, magic.Length()) != magic) {
    return false;
  }

  auto size = LittleEndian::readUint32(lz4.get() + magic.Length());

  if (!result.SetLength(size, fallible) ||
      !LZ4::decompress(lz4.get() + HEADER_SIZE, result.BeginWriting(), size)) {
    result.Truncate();
    return false;
  }

  return true;
}


static bool
ParseJSON(JSContext* cx, nsACString& jsonData, JS::MutableHandleValue result)
{
  NS_ConvertUTF8toUTF16 str(jsonData);
  jsonData.Truncate();

  return JS_ParseJSON(cx, str.Data(), str.Length(), result);
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


  already_AddRefed<nsIFile> FullPath();

  NSLocationType LocationType();

  bool UpdateLastModifiedTime();


private:
  nsString mId;
  InstallLocation& mLocation;
};

already_AddRefed<nsIFile>
Addon::FullPath()
{
  nsString path = mLocation.Path();

  nsCOMPtr<nsIFile> file;
  NS_NewLocalFile(path, false, getter_AddRefs(file));
  MOZ_RELEASE_ASSERT(file);

  path = Path();
  file->AppendRelativePath(path);

  return file.forget();
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

bool
Addon::UpdateLastModifiedTime()
{
  nsCOMPtr<nsIFile> file = FullPath();

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

void
AddonManagerStartup::AddInstallLocation(Addon& addon)
{
  nsCOMPtr<nsIFile> file = addon.FullPath();

  nsString path;
  if (NS_FAILED(file->GetPath(path))) {
    return;
  }

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
}

nsresult
AddonManagerStartup::ReadStartupData(JSContext* cx, JS::MutableHandleValue locations)
{
  nsresult rv;

  locations.set(JS::UndefinedValue());

  nsCOMPtr<nsIFile> file = CloneAndAppend(ProfileDir(), "addonStartup.json.lz4");

  nsCString path;
  rv = file->GetNativePath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString data;
  if (!ReadFileLZ4(path.get(), data)) {
    return NS_ERROR_FAILURE;
  }

  if (data.IsEmpty() || !ParseJSON(cx, data, locations)) {
    return NS_OK;
  }

  if (!locations.isObject()) {
    return NS_ERROR_UNEXPECTED;
  }

  JS::RootedObject locs(cx, &locations.toObject());
  for (auto& e1 : PropertyIter(cx, locs)) {
    InstallLocation loc(e1);

    if (!loc.ShouldCheckStartupModifications()) {
      continue;
    }

    for (auto& e2 : loc.Addons()) {
      Addon addon(e2);

      if (addon.Enabled() && addon.UpdateLastModifiedTime()) {
        loc.SetChanged(true);
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
  for (auto& e1 : PropertyIter(cx, locs)) {
    InstallLocation loc(e1);

    for (auto& e2 : loc.Addons()) {
      Addon addon(e2);

      if (!addon.Bootstrapped()) {
        AddInstallLocation(addon);
      }

      if (enableInterpositions && addon.ShimsEnabled()) {
        EnableShims(addon.Id());
      }
    }
  }

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
