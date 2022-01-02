/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonManagerStartup.h"
#include "AddonManagerStartup-inlines.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"  // JS::IsArrayObject
#include "js/ArrayBuffer.h"
#include "js/Exception.h"
#include "js/JSON.h"
#include "js/PropertyAndElement.h"  // JS_GetProperty, JS_SetProperty
#include "js/TracingAPI.h"
#include "xpcpublic.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Components.h"
#include "mozilla/Compression.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/URLPreloader.h"
#include "mozilla/Unused.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsAppRunner.h"
#include "nsContentUtils.h"
#include "nsChromeRegistry.h"
#include "nsIAppStartup.h"
#include "nsIDOMWindowUtils.h"  // for nsIJSRAIIHelper
#include "nsIFileURL.h"
#include "nsIIOService.h"
#include "nsIJARURI.h"
#include "nsIStringEnumerator.h"
#include "nsIZipReader.h"
#include "nsJARProtocolHandler.h"
#include "nsJSUtils.h"
#include "nsReadableUtils.h"
#include "nsXULAppAPI.h"

#include <stdlib.h>

namespace mozilla {

using Compression::LZ4;
using dom::ipc::StructuredCloneData;

AddonManagerStartup& AddonManagerStartup::GetSingleton() {
  static RefPtr<AddonManagerStartup> singleton;
  if (!singleton) {
    singleton = new AddonManagerStartup();
    ClearOnShutdown(&singleton);
  }
  return *singleton;
}

AddonManagerStartup::AddonManagerStartup() = default;

nsIFile* AddonManagerStartup::ProfileDir() {
  if (!mProfileDir) {
    nsresult rv;

    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(mProfileDir));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }

  return mProfileDir;
}

NS_IMPL_ISUPPORTS(AddonManagerStartup, amIAddonManagerStartup, nsIObserver)

/*****************************************************************************
 * URI utils
 *****************************************************************************/

static nsresult ParseJARURI(nsIJARURI* uri, nsIURI** jarFile,
                            nsCString& entry) {
  MOZ_TRY(uri->GetJARFile(jarFile));
  MOZ_TRY(uri->GetJAREntry(entry));

  // The entry portion of a jar: URI is required to begin with a '/', but for
  // nested JAR URIs, the leading / of the outer entry is currently stripped.
  // This is a bug which should be fixed in the JAR URI code, but...
  if (entry.IsEmpty() || entry[0] != '/') {
    entry.Insert('/', 0);
  }
  return NS_OK;
}

static nsresult ParseJARURI(nsIURI* uri, nsIURI** jarFile, nsCString& entry) {
  nsresult rv;
  nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(uri, &rv);
  MOZ_TRY(rv);

  return ParseJARURI(jarURI, jarFile, entry);
}

static Result<nsCOMPtr<nsIFile>, nsresult> GetFile(nsIURI* uri) {
  nsresult rv;
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(uri, &rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIFile> file;
  MOZ_TRY(fileURL->GetFile(getter_AddRefs(file)));
  MOZ_ASSERT(file);

  return std::move(file);
}

/*****************************************************************************
 * File utils
 *****************************************************************************/

static already_AddRefed<nsIFile> CloneAndAppend(nsIFile* aFile,
                                                const char* name) {
  nsCOMPtr<nsIFile> file;
  aFile->Clone(getter_AddRefs(file));
  file->AppendNative(nsDependentCString(name));
  return file.forget();
}

static bool IsNormalFile(nsIFile* file) {
  bool result;
  return NS_SUCCEEDED(file->IsFile(&result)) && result;
}

static const char STRUCTURED_CLONE_MAGIC[] = "mozJSSCLz40v001";

template <typename T>
static Result<nsCString, nsresult> DecodeLZ4(const nsACString& lz4,
                                             const T& magicNumber) {
  constexpr auto HEADER_SIZE = sizeof(magicNumber) + 4;

  // Note: We want to include the null terminator here.
  nsDependentCSubstring magic(magicNumber, sizeof(magicNumber));

  if (lz4.Length() < HEADER_SIZE || StringHead(lz4, magic.Length()) != magic) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  auto data = lz4.BeginReading() + magic.Length();
  auto size = LittleEndian::readUint32(data);
  data += 4;

  size_t dataLen = lz4.EndReading() - data;
  size_t outputSize;

  nsCString result;
  if (!result.SetLength(size, fallible) ||
      !LZ4::decompress(data, dataLen, result.BeginWriting(), size,
                       &outputSize)) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  MOZ_DIAGNOSTIC_ASSERT(size == outputSize);

  return std::move(result);
}

// Our zlib headers redefine this to MOZ_Z_compress, which breaks LZ4::compress
#undef compress

template <typename T>
static Result<nsCString, nsresult> EncodeLZ4(const nsACString& data,
                                             const T& magicNumber) {
  // Note: We want to include the null terminator here.
  nsDependentCSubstring magic(magicNumber, sizeof(magicNumber));

  nsAutoCString result;
  result.Append(magic);

  auto off = result.Length();
  if (!result.SetLength(off + 4, fallible)) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  LittleEndian::writeUint32(result.BeginWriting() + off, data.Length());
  off += 4;

  auto size = LZ4::maxCompressedSize(data.Length());
  if (!result.SetLength(off + size, fallible)) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }

  size = LZ4::compress(data.BeginReading(), data.Length(),
                       result.BeginWriting() + off);

  if (!result.SetLength(off + size, fallible)) {
    return Err(NS_ERROR_OUT_OF_MEMORY);
  }
  return std::move(result);
}

static_assert(sizeof STRUCTURED_CLONE_MAGIC % 8 == 0,
              "Magic number should be an array of uint64_t");

/**
 * Reads the contents of a LZ4-compressed file, as stored by the OS.File
 * module, and returns the decompressed contents on success.
 */
static Result<nsCString, nsresult> ReadFileLZ4(nsIFile* file) {
  static const char MAGIC_NUMBER[] = "mozLz40";

  nsCString lz4;
  MOZ_TRY_VAR(lz4, URLPreloader::ReadFile(file));

  if (lz4.IsEmpty()) {
    return lz4;
  }

  return DecodeLZ4(lz4, MAGIC_NUMBER);
}

static bool ParseJSON(JSContext* cx, nsACString& jsonData,
                      JS::MutableHandleValue result) {
  NS_ConvertUTF8toUTF16 str(jsonData);
  jsonData.Truncate();

  return JS_ParseJSON(cx, str.Data(), str.Length(), result);
}

static Result<nsCOMPtr<nsIZipReaderCache>, nsresult> GetJarCache() {
  nsCOMPtr<nsIIOService> ios = components::IO::Service();
  NS_ENSURE_TRUE(ios, Err(NS_ERROR_FAILURE));

  nsCOMPtr<nsIProtocolHandler> jarProto;
  MOZ_TRY(ios->GetProtocolHandler("jar", getter_AddRefs(jarProto)));

  auto jar = static_cast<nsJARProtocolHandler*>(jarProto.get());
  MOZ_ASSERT(jar);

  nsCOMPtr<nsIZipReaderCache> zipCache = jar->JarCache();
  return std::move(zipCache);
}

static Result<FileLocation, nsresult> GetFileLocation(nsIURI* uri) {
  FileLocation location;

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(uri);
  nsCOMPtr<nsIFile> file;
  if (fileURL) {
    MOZ_TRY(fileURL->GetFile(getter_AddRefs(file)));
    location.Init(file);
  } else {
    nsCOMPtr<nsIURI> fileURI;
    nsCString entry;
    MOZ_TRY(ParseJARURI(uri, getter_AddRefs(fileURI), entry));

    MOZ_TRY_VAR(file, GetFile(fileURI));

    location.Init(file, entry.get());
  }

  return std::move(location);
}

/*****************************************************************************
 * JSON data handling
 *****************************************************************************/

class MOZ_STACK_CLASS WrapperBase {
 protected:
  WrapperBase(JSContext* cx, JSObject* object) : mCx(cx), mObject(cx, object) {}

  WrapperBase(JSContext* cx, const JS::Value& value) : mCx(cx), mObject(cx) {
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

bool WrapperBase::GetBool(const char* name, bool defVal) {
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

double WrapperBase::GetNumber(const char* name, double defVal) {
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

nsString WrapperBase::GetString(const char* name, const char* defVal) {
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

JSObject* WrapperBase::GetObject(const char* name) {
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
      : InstallLocation(iter.Cx(), iter.Value()) {}

  InstallLocation(const InstallLocation& other)
      : InstallLocation(other.mCx, JS::ObjectValue(*other.mObject)) {}

  void SetChanged(bool changed) {
    JS::RootedObject obj(mCx, mObject);

    JS::RootedValue val(mCx, JS::BooleanValue(changed));
    if (!JS_SetProperty(mCx, obj, "changed", val)) {
      JS_ClearPendingException(mCx);
    }
  }

  PropertyIter& Addons() { return mAddonsIter.ref(); }

  nsString Path() { return GetString("path"); }

  bool ShouldCheckStartupModifications() {
    return GetBool("checkStartupModifications");
  }

 private:
  JS::RootedObject mAddonsObj;
  Maybe<PropertyIter> mAddonsIter;
};

class MOZ_STACK_CLASS Addon : public WrapperBase {
 public:
  Addon(JSContext* cx, InstallLocation& location, const nsAString& id,
        JSObject* object)
      : WrapperBase(cx, object), mId(id), mLocation(location) {}

  MOZ_IMPLICIT Addon(PropertyIterElem& iter)
      : WrapperBase(iter.Cx(), iter.Value()),
        mId(iter.Name()),
        mLocation(*static_cast<InstallLocation*>(iter.Context())) {}

  Addon(const Addon& other)
      : WrapperBase(other.mCx, other.mObject),
        mId(other.mId),
        mLocation(other.mLocation) {}

  const nsString& Id() { return mId; }

  nsString Path() { return GetString("path"); }

  nsString Type() { return GetString("type", "extension"); }

  bool Enabled() { return GetBool("enabled"); }

  double LastModifiedTime() { return GetNumber("lastModifiedTime"); }

  bool ShouldCheckStartupModifications() {
    return Type().EqualsLiteral("locale");
  }

  Result<nsCOMPtr<nsIFile>, nsresult> FullPath();

  Result<bool, nsresult> UpdateLastModifiedTime();

 private:
  nsString mId;
  InstallLocation& mLocation;
};

Result<nsCOMPtr<nsIFile>, nsresult> Addon::FullPath() {
  nsString path = Path();

  // First check for an absolute path, in case we have a proxy file.
  nsCOMPtr<nsIFile> file;
  if (NS_SUCCEEDED(NS_NewLocalFile(path, false, getter_AddRefs(file)))) {
    return std::move(file);
  }

  // If not an absolute path, fall back to a relative path from the location.
  MOZ_TRY(NS_NewLocalFile(mLocation.Path(), false, getter_AddRefs(file)));

  MOZ_TRY(file->AppendRelativePath(path));
  return std::move(file);
}

Result<bool, nsresult> Addon::UpdateLastModifiedTime() {
  nsCOMPtr<nsIFile> file;
  MOZ_TRY_VAR(file, FullPath());

  JS::RootedObject obj(mCx, mObject);

  bool result;
  if (NS_FAILED(file->Exists(&result)) || !result) {
    JS::RootedValue value(mCx, JS::NullValue());
    if (!JS_SetProperty(mCx, obj, "currentModifiedTime", value)) {
      JS_ClearPendingException(mCx);
    }

    return true;
  }

  PRTime time;

  nsCOMPtr<nsIFile> manifest = file;
  if (!IsNormalFile(manifest)) {
    manifest = CloneAndAppend(file, "manifest.json");
    if (!IsNormalFile(manifest)) {
      return true;
    }
  }

  if (NS_FAILED(manifest->GetLastModifiedTime(&time))) {
    return true;
  }

  double lastModified = time;
  JS::RootedValue value(mCx, JS::NumberValue(lastModified));
  if (!JS_SetProperty(mCx, obj, "currentModifiedTime", value)) {
    JS_ClearPendingException(mCx);
  }

  return lastModified != LastModifiedTime();
}

InstallLocation::InstallLocation(JSContext* cx, const JS::Value& value)
    : WrapperBase(cx, value), mAddonsObj(cx), mAddonsIter() {
  mAddonsObj = GetObject("addons");
  if (!mAddonsObj) {
    mAddonsObj = JS_NewPlainObject(cx);
  }
  mAddonsIter.emplace(cx, mAddonsObj, this);
}

/*****************************************************************************
 * XPC interfacing
 *****************************************************************************/

nsresult AddonManagerStartup::ReadStartupData(
    JSContext* cx, JS::MutableHandleValue locations) {
  locations.set(JS::UndefinedValue());

  nsCOMPtr<nsIFile> file =
      CloneAndAppend(ProfileDir(), "addonStartup.json.lz4");

  nsCString data;
  auto res = ReadFileLZ4(file);
  if (res.isOk()) {
    data = res.unwrap();
  } else if (res.inspectErr() != NS_ERROR_FILE_NOT_FOUND) {
    return res.unwrapErr();
  }

  if (data.IsEmpty() || !ParseJSON(cx, data, locations)) {
    return NS_OK;
  }

  if (!locations.isObject()) {
    return NS_ERROR_UNEXPECTED;
  }

  JS::RootedObject locs(cx, &locations.toObject());
  for (auto e1 : PropertyIter(cx, locs)) {
    InstallLocation loc(e1);

    bool shouldCheck = loc.ShouldCheckStartupModifications();

    for (auto e2 : loc.Addons()) {
      Addon addon(e2);

      if (addon.Enabled() &&
          (shouldCheck || addon.ShouldCheckStartupModifications())) {
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

nsresult AddonManagerStartup::EncodeBlob(JS::HandleValue value, JSContext* cx,
                                         JS::MutableHandleValue result) {
  StructuredCloneData holder;

  ErrorResult rv;
  holder.Write(cx, value, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  nsAutoCString scData;

  holder.Data().ForEachDataChunk([&](const char* aData, size_t aSize) {
    scData.Append(nsDependentCSubstring(aData, aSize));
    return true;
  });

  nsCString lz4;
  MOZ_TRY_VAR(lz4, EncodeLZ4(scData, STRUCTURED_CLONE_MAGIC));

  JS::RootedObject obj(cx);
  MOZ_TRY(nsContentUtils::CreateArrayBuffer(cx, lz4, &obj.get()));

  result.set(JS::ObjectValue(*obj));
  return NS_OK;
}

nsresult AddonManagerStartup::DecodeBlob(JS::HandleValue value, JSContext* cx,
                                         JS::MutableHandleValue result) {
  NS_ENSURE_TRUE(value.isObject() &&
                     JS::IsArrayBufferObject(&value.toObject()) &&
                     JS::ArrayBufferHasData(&value.toObject()),
                 NS_ERROR_INVALID_ARG);

  StructuredCloneData holder;

  nsCString data;
  {
    JS::AutoCheckCannotGC nogc;

    auto obj = &value.toObject();
    bool isShared;

    size_t len = JS::GetArrayBufferByteLength(obj);
    NS_ENSURE_TRUE(len <= INT32_MAX, NS_ERROR_INVALID_ARG);
    nsDependentCSubstring lz4(
        reinterpret_cast<char*>(JS::GetArrayBufferData(obj, &isShared, nogc)),
        uint32_t(len));

    MOZ_TRY_VAR(data, DecodeLZ4(lz4, STRUCTURED_CLONE_MAGIC));
  }

  bool ok = holder.CopyExternalData(data.get(), data.Length());
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

  ErrorResult rv;
  holder.Read(cx, result, rv);
  return rv.StealNSResult();
  ;
}

static nsresult EnumerateZip(nsIZipReader* zip, const nsACString& pattern,
                             nsTArray<nsString>& results) {
  nsCOMPtr<nsIUTF8StringEnumerator> entries;
  MOZ_TRY(zip->FindEntries(pattern, getter_AddRefs(entries)));

  bool hasMore;
  while (NS_SUCCEEDED(entries->HasMore(&hasMore)) && hasMore) {
    nsAutoCString name;
    MOZ_TRY(entries->GetNext(name));

    results.AppendElement(NS_ConvertUTF8toUTF16(name));
  }

  return NS_OK;
}

nsresult AddonManagerStartup::EnumerateJAR(nsIURI* uri,
                                           const nsACString& pattern,
                                           nsTArray<nsString>& results) {
  nsCOMPtr<nsIZipReaderCache> zipCache;
  MOZ_TRY_VAR(zipCache, GetJarCache());

  nsCOMPtr<nsIZipReader> zip;
  nsCOMPtr<nsIFile> file;
  if (nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(uri)) {
    nsCOMPtr<nsIURI> fileURI;
    nsCString entry;
    MOZ_TRY(ParseJARURI(jarURI, getter_AddRefs(fileURI), entry));

    MOZ_TRY_VAR(file, GetFile(fileURI));
    MOZ_TRY(
        zipCache->GetInnerZip(file, Substring(entry, 1), getter_AddRefs(zip)));
  } else {
    MOZ_TRY_VAR(file, GetFile(uri));
    MOZ_TRY(zipCache->GetZip(file, getter_AddRefs(zip)));
  }
  MOZ_ASSERT(zip);

  return EnumerateZip(zip, pattern, results);
}

nsresult AddonManagerStartup::EnumerateJARSubtree(nsIURI* uri,
                                                  nsTArray<nsString>& results) {
  nsCOMPtr<nsIURI> fileURI;
  nsCString entry;
  MOZ_TRY(ParseJARURI(uri, getter_AddRefs(fileURI), entry));

  // Mangle the path into a pattern to match all child entries by escaping any
  // existing pattern matching metacharacters it contains and appending "/*".
  constexpr auto metaChars = "[]()?*~|$\\"_ns;

  nsCString pattern;
  pattern.SetCapacity(entry.Length());

  // The first character of the entry name is "/", which we want to skip.
  for (auto chr : Span(Substring(entry, 1))) {
    if (metaChars.FindChar(chr) >= 0) {
      pattern.Append('\\');
    }
    pattern.Append(chr);
  }
  if (!pattern.IsEmpty() && !StringEndsWith(pattern, "/"_ns)) {
    pattern.Append('/');
  }
  pattern.Append('*');

  return EnumerateJAR(fileURI, pattern, results);
}

nsresult AddonManagerStartup::InitializeURLPreloader() {
  MOZ_RELEASE_ASSERT(xpc::IsInAutomation());

  URLPreloader::ReInitialize();

  return NS_OK;
}

/******************************************************************************
 * RegisterChrome
 ******************************************************************************/

namespace {
static bool sObserverRegistered;

struct ContentEntry final {
  explicit ContentEntry(nsTArray<nsCString>&& aArgs, uint8_t aFlags = 0)
      : mArgs(std::move(aArgs)), mFlags(aFlags) {}

  AutoTArray<nsCString, 2> mArgs;
  uint8_t mFlags;
};

};  // anonymous namespace
};  // namespace mozilla

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::ContentEntry);

namespace mozilla {
namespace {

class RegistryEntries final : public nsIJSRAIIHelper,
                              public LinkedListElement<RegistryEntries> {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIJSRAIIHELPER

  using Override = AutoTArray<nsCString, 2>;
  using Locale = AutoTArray<nsCString, 3>;

  RegistryEntries(FileLocation& location, nsTArray<Override>&& overrides,
                  nsTArray<ContentEntry>&& content, nsTArray<Locale>&& locales)
      : mLocation(location),
        mOverrides(std::move(overrides)),
        mContent(std::move(content)),
        mLocales(std::move(locales)) {}

  void Register();

 protected:
  virtual ~RegistryEntries() { Unused << Destruct(); }

 private:
  FileLocation mLocation;
  const nsTArray<Override> mOverrides;
  const nsTArray<ContentEntry> mContent;
  const nsTArray<Locale> mLocales;
};

NS_IMPL_ISUPPORTS(RegistryEntries, nsIJSRAIIHelper)

void RegistryEntries::Register() {
  RefPtr<nsChromeRegistry> cr = nsChromeRegistry::GetSingleton();

  nsChromeRegistry::ManifestProcessingContext context(NS_EXTENSION_LOCATION,
                                                      mLocation);

  for (auto& override : mOverrides) {
    const char* args[] = {override[0].get(), override[1].get()};
    cr->ManifestOverride(context, 0, const_cast<char**>(args), 0);
  }

  for (auto& content : mContent) {
    const char* args[] = {content.mArgs[0].get(), content.mArgs[1].get()};
    cr->ManifestContent(context, 0, const_cast<char**>(args), content.mFlags);
  }

  for (auto& locale : mLocales) {
    const char* args[] = {locale[0].get(), locale[1].get(), locale[2].get()};
    cr->ManifestLocale(context, 0, const_cast<char**>(args), 0);
  }
}

NS_IMETHODIMP
RegistryEntries::Destruct() {
  if (isInList()) {
    remove();

    // No point in doing I/O to check for new chrome during shutdown, return
    // early in that case.
    nsCOMPtr<nsIAppStartup> appStartup = components::AppStartup::Service();
    if (!appStartup || appStartup->GetShuttingDown()) {
      return NS_OK;
    }

    // When we remove dynamic entries from the registry, we need to rebuild it
    // in order to ensure a consistent state. See comments in Observe().
    RefPtr<nsChromeRegistry> cr = nsChromeRegistry::GetSingleton();
    return cr->CheckForNewChrome();
  }
  return NS_OK;
}

static LinkedList<RegistryEntries>& GetRegistryEntries() {
  static LinkedList<RegistryEntries> sEntries;
  return sEntries;
}
};  // anonymous namespace

NS_IMETHODIMP
AddonManagerStartup::RegisterChrome(nsIURI* manifestURI,
                                    JS::HandleValue locations, JSContext* cx,
                                    nsIJSRAIIHelper** result) {
  auto IsArray = [cx](JS::HandleValue val) -> bool {
    bool isArray;
    return JS::IsArrayObject(cx, val, &isArray) && isArray;
  };

  NS_ENSURE_ARG_POINTER(manifestURI);
  NS_ENSURE_TRUE(IsArray(locations), NS_ERROR_INVALID_ARG);

  FileLocation location;
  MOZ_TRY_VAR(location, GetFileLocation(manifestURI));

  nsTArray<RegistryEntries::Locale> locales;
  nsTArray<ContentEntry> content;
  nsTArray<RegistryEntries::Override> overrides;

  JS::RootedObject locs(cx, &locations.toObject());
  JS::RootedValue arrayVal(cx);
  JS::RootedObject array(cx);

  for (auto elem : ArrayIter(cx, locs)) {
    arrayVal = elem.Value();
    NS_ENSURE_TRUE(IsArray(arrayVal), NS_ERROR_INVALID_ARG);

    array = &arrayVal.toObject();

    AutoTArray<nsCString, 4> vals;
    for (auto val : ArrayIter(cx, array)) {
      nsAutoJSString str;
      NS_ENSURE_TRUE(str.init(cx, val.Value()), NS_ERROR_OUT_OF_MEMORY);

      vals.AppendElement(NS_ConvertUTF16toUTF8(str));
    }
    NS_ENSURE_TRUE(vals.Length() > 0, NS_ERROR_INVALID_ARG);

    nsCString type = vals[0];
    vals.RemoveElementAt(0);

    if (type.EqualsLiteral("override")) {
      NS_ENSURE_TRUE(vals.Length() == 2, NS_ERROR_INVALID_ARG);
      overrides.AppendElement(std::move(vals));
    } else if (type.EqualsLiteral("content")) {
      if (vals.Length() == 3 &&
          vals[2].EqualsLiteral("contentaccessible=yes")) {
        NS_ENSURE_TRUE(xpc::IsInAutomation(), NS_ERROR_INVALID_ARG);
        vals.RemoveElementAt(2);
        content.AppendElement(ContentEntry(
            std::move(vals), nsChromeRegistry::CONTENT_ACCESSIBLE));
      } else {
        NS_ENSURE_TRUE(vals.Length() == 2, NS_ERROR_INVALID_ARG);
        content.AppendElement(ContentEntry(std::move(vals)));
      }
    } else if (type.EqualsLiteral("locale")) {
      NS_ENSURE_TRUE(vals.Length() == 3, NS_ERROR_INVALID_ARG);
      locales.AppendElement(std::move(vals));
    } else {
      return NS_ERROR_INVALID_ARG;
    }
  }

  if (!sObserverRegistered) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    NS_ENSURE_TRUE(obs, NS_ERROR_UNEXPECTED);
    obs->AddObserver(this, "chrome-manifests-loaded", false);

    sObserverRegistered = true;
  }

  auto entry = MakeRefPtr<RegistryEntries>(
      location, std::move(overrides), std::move(content), std::move(locales));

  entry->Register();
  GetRegistryEntries().insertBack(entry);

  entry.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
AddonManagerStartup::Observe(nsISupports* subject, const char* topic,
                             const char16_t* data) {
  // The chrome registry is maintained as a set of global resource mappings
  // generated mainly from manifest files, on-the-fly, as they're parsed.
  // Entries added later override entries added earlier, and no record is kept
  // of the former state.
  //
  // As a result, if we remove a dynamically-added manifest file, or a set of
  // dynamic entries, the registry needs to be rebuilt from scratch, from the
  // manifests and dynamic entries that remain. The chrome registry itself
  // takes care of re-parsing manifes files. This observer notification lets
  // us know when we need to re-register our dynamic entries.
  if (!strcmp(topic, "chrome-manifests-loaded")) {
    for (auto entry : GetRegistryEntries()) {
      entry->Register();
    }
  }

  return NS_OK;
}

}  // namespace mozilla
