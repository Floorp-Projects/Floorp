/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_filedialog_WinFileDialogCommands_h__
#define widget_windows_filedialog_WinFileDialogCommands_h__

#include "ipc/EnumSerializer.h"
#include "mozilla/Logging.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ipc/MessageLink.h"
#include "mozilla/widget/filedialog/WinFileDialogCommandsDefn.h"

// Windows interface types, defined in <shobjidl.h>
struct IFileDialog;
struct IFileOpenDialog;

namespace mozilla::ipc {
class LaunchError;
}

namespace mozilla::widget::filedialog {

namespace detail {

template <typename T, typename E, bool B>
struct PromiseInfo {
  using ResolveT = T;
  using RejectT = E;
  constexpr static const bool IsExclusive = B;
  using Promise = MozPromise<T, E, B>;
};

template <typename P>
auto DestructurePromiseImpl(P&&) {
  // Debugging hint: A type in the instantiation chain (here named `P`) was
  // expected to be a `RefPtr<MozPromise<...>>`, but was some other type.
  static_assert(false, "expected P = RefPtr<MozPromise< ... >>");
}

template <typename T, typename E, bool B>
auto DestructurePromiseImpl(RefPtr<MozPromise<T, E, B>>&&)
    -> PromiseInfo<T, E, B>;

template <typename P>
using DestructurePromise =
    std::decay_t<decltype(DestructurePromiseImpl(std::declval<P>()))>;

template <typename T, typename E>
struct ResultInfo {
  using OkT = T;
  using ErrorT = E;
};

template <typename R>
auto DestructureResultImpl(R&&) {
  // Debugging hint: A type in the instantiation chain (here named `R`) was
  // expected to be a `mozilla::Result<...>`, but was some other type.
  static_assert(false, "expected R = mozilla::Result< ... >");
}

template <typename T, typename E>
auto DestructureResultImpl(mozilla::Result<T, E>&&) -> ResultInfo<T, E>;

template <typename R>
using DestructureResult =
    std::decay_t<decltype(DestructureResultImpl(std::declval<R>()))>;

#define MOZ_ASSERT_SAME_TYPE(T1, T2, ...) \
  static_assert(std::is_same_v<T1, T2>, ##__VA_ARGS__)
}  // namespace detail

extern LazyLogModule sLogFileDialog;

// Simple struct for reporting errors.
struct Error {
  enum Kind {
    // This error's source was within the local (current) process.
    LocalError,
    // This error's source was within the remote host process, and it was
    // reported via noncatastrophic channels.
    RemoteError,
    // This error was reported via the IPC subsystem. (This includes unexpected
    // remote host-process crashes.)
    IPCError,
  };

  constexpr static const StaticString kInvalidValue{
      "<bad filedialog::Error::Location?>"};

  // "Enum" denoting error-location. Members are in `VALID_STRINGS`, and have no
  // name other than their string.
  //
  // The "enum" also supports importing "dynamic" strings from ipc::LaunchError.
  // These strings will not be present in the VALID_STRINGS set and so cannot be
  // faithfully serialized for IPC -- which is fine, since definitionally if you
  // get one you won't have an IPC connection to worry about passing it over
  // anyway.
  class Location {
    // The printable form of the location.
    StaticString text;
    // The numeric value for the location.
    //
    // This will be less than `VALID_STRINGS_COUNT` iff the text of `text` is
    // equal to that of some string in `VALID_STRINGS`; other values have no
    // individual significance.
    uint32_t value;

    constexpr explicit Location(uint32_t value)
        : text(value < VALID_STRINGS_COUNT ? StaticString(VALID_STRINGS[value])
                                           : kInvalidValue),
          value(value) {}

    // Valid locations for errors. (Indices do not need to remain stable between
    // releases; but -- where meaningful -- string values themselves should, for
    // ease of telemetry-aggregation.)
    constexpr static nsLiteralCString const VALID_STRINGS[] = {
        "ApplyCommands"_ns,
        "CoCreateInstance(CLSID_ShellLibrary)"_ns,
        "GetFileResults: GetShellItemPath (1)"_ns,
        "GetFileResults: GetShellItemPath (2)"_ns,
        "GetShellItemPath"_ns,
        "IFileDialog::GetFileTypeIndex"_ns,
        "IFileDialog::GetOptions"_ns,
        "IFileDialog::GetResult"_ns,
        "IFileDialog::GetResult: item"_ns,
        "IFileDialog::Show"_ns,
        "IFileOpenDialog::GetResults"_ns,
        "IFileOpenDialog::GetResults: items"_ns,
        "IPC"_ns,
        "IShellItemArray::GetCount"_ns,
        "IShellItemArray::GetItemAt"_ns,
        "MakeFileDialog"_ns,
        "NS_NewNamedThread"_ns,
        "Save + FOS_ALLOWMULTISELECT"_ns,
        "ShowFilePicker"_ns,
        "ShowFolderPicker"_ns,
        "ShowRemote: UtilityProcessManager::GetSingleton"_ns,
        "ShowRemote: invocation of CreateWinFileDialogActor"_ns,
        "UtilityProcessManager::CreateWinFileDialogActor"_ns,
        "internal IPC failure?"_ns,
    };
    constexpr static size_t VALID_STRINGS_COUNT =
        std::extent_v<decltype(VALID_STRINGS)>;

    // Prevent duplicates from occurring in VALID_STRINGS by forcing it to be
    // sorted. (Note that std::is_sorted is not constexpr until C++20.)
    static_assert(
        []() {
          for (size_t i = 0; i + 1 < VALID_STRINGS_COUNT; ++i) {
            if (!(std::string_view{VALID_STRINGS[i]} <
                  std::string_view{VALID_STRINGS[i + 1]})) {
              return false;
            }
          }
          return true;
        }(),
        "VALID_STRINGS should be ASCIIbetically sorted");

   public:
    // A LaunchError's carried location won't be in the list and so is not
    // IPDL-serializable -- but it's still telemetry-safe.
    constexpr explicit Location(mozilla::ipc::LaunchError const& err);

    constexpr uint32_t Serialize() const { return value; }
    constexpr static Location Deserialize(uint32_t val) {
      MOZ_ASSERT(val < VALID_STRINGS_COUNT);
      return Location(val);
    }

   public:
    constexpr static Location npos() { return Location(~uint32_t(0)); }

    // True iff this Location was formed from a string in VALID_STRINGS.
    constexpr bool IsSerializable() const {
      return value < VALID_STRINGS_COUNT;
    }

    constexpr StaticString ToString() const { return text; }

    constexpr static Location FromString(StaticString str) {
      std::string_view val(str.get());
      for (uint32_t i = 0; i < VALID_STRINGS_COUNT; ++i) {
        if (val == VALID_STRINGS[i]) {
          return Location(i);
        }
      }
      // TODO(C++20): make this `consteval` and fail here, eliminating any need
      // for a return value for the fallthrough case.
      return npos();
    }

    constexpr char const* c_str() const { return ToString().get(); }
  };  // class Error::Location

  // Where and how (run-time) this error occurred.
  Kind kind;
  // Where (compile-time) this error occurred.
  Location where;
  // Why (run-time) this error occurred. Usually an HRESULT, but its
  // interpretation is necessarily dependent on the value of `where`.
  uint32_t why;

  // `impl Debug for Kind`
  static const char* KindName(Kind);

  // Constructs an error of Kind "LocalError" (since launch-errors are always
  // considered local).
  static Error From(mozilla::ipc::LaunchError const&);
};

// Create a filedialog::Error from a where-string, confirming at compile-time
// that the supplied where-string is validly serializable.
#define MOZ_FD_ERROR(kind_, where_, why_)                                 \
  ([](HRESULT why_arg_) -> ::mozilla::widget::filedialog::Error {         \
    using Error = ::mozilla::widget::filedialog::Error;                   \
    constexpr static const Error::Location loc =                          \
        Error::Location::FromString(where_);                              \
    static_assert(                                                        \
        loc.IsSerializable(),                                             \
        "filedialog::Error: location not found in Error::VALID_STRINGS"); \
    return Error{                                                         \
        .kind = Error::kind_, .where = loc, .why = (uint32_t)why_arg_};   \
  }(why_))

// Create a filedialog::Error of kind LocalError (the usual case).
#define MOZ_FD_LOCAL_ERROR(where_, why_) MOZ_FD_ERROR(LocalError, where_, why_)

template <typename R>
using Promise = MozPromise<R, Error, true>;

enum class FileDialogType : uint8_t { Open, Save };

// Create a file-dialog of the relevant type. Requires MSCOM to be initialized.
mozilla::Result<RefPtr<IFileDialog>, Error> MakeFileDialog(FileDialogType);

// Apply the selected commands to the IFileDialog, in preparation for showing
// it. (The actual showing step is left to the caller.)
mozilla::Result<Ok, Error> ApplyCommands(::IFileDialog*,
                                         nsTArray<Command> const& commands);

// Extract one or more results from the file-picker dialog.
//
// Requires that Show() has been called and has returned S_OK.
mozilla::Result<Results, Error> GetFileResults(::IFileDialog*);

// Extract the chosen folder from the folder-picker dialog.
//
// Requires that Show() has been called and has returned S_OK.
mozilla::Result<nsString, Error> GetFolderResults(::IFileDialog*);

namespace detail {
// Log the error. If it's a notable error, kill the child process.
void LogProcessingError(LogModule* aModule, ipc::IProtocol* aCaller,
                        ipc::HasResultCodes::Result aCode, const char* aReason);

}  // namespace detail

// Show a file-picker on another thread in the current process.
RefPtr<Promise<Maybe<Results>>> SpawnFilePicker(HWND parent,
                                                FileDialogType type,
                                                nsTArray<Command> commands);

// Show a folder-picker on another thread in the current process.
RefPtr<Promise<Maybe<nsString>>> SpawnFolderPicker(HWND parent,
                                                   nsTArray<Command> commands);

}  // namespace mozilla::widget::filedialog

namespace IPC {
template <>
struct ParamTraits<mozilla::widget::filedialog::FileDialogType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::widget::filedialog::FileDialogType,
          mozilla::widget::filedialog::FileDialogType::Open,
          mozilla::widget::filedialog::FileDialogType::Save> {};
}  // namespace IPC

#endif  // widget_windows_filedialog_WinFileDialogCommands_h__
