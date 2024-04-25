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

  // "Enum" denoting error-location. Members are in `VALID_STRINGS`, and have no
  // name other than their string.
  //
  // (Note: under C++20, this could reasonably be replaced with an `nsString`
  // alongside a check that all constructors are either a) consteval or b) from
  // IPC.)
  class Location {
    uint32_t value;
    constexpr explicit Location(uint32_t value) : value(value) {}

    // Valid locations for errors. (Indices do not need to remain stable between
    // releases; but -- where meaningful -- string values themselves should, for
    // ease of telemetry-aggregation.)
    constexpr static std::string_view const VALID_STRINGS[] = {
        "ApplyCommands",
        "CoCreateInstance(CLSID_ShellLibrary)",
        "GetFileResults: GetShellItemPath (1)",
        "GetFileResults: GetShellItemPath (2)",
        "GetShellItemPath",
        "IFileDialog::GetFileTypeIndex",
        "IFileDialog::GetOptions",
        "IFileDialog::GetResult",
        "IFileDialog::GetResult: item",
        "IFileDialog::Show",
        "IFileOpenDialog::GetResults",
        "IFileOpenDialog::GetResults: items",
        "IPC",
        "IShellItemArray::GetCount",
        "IShellItemArray::GetItemAt",
        "MakeFileDialog",
        "NS_NewNamedThread",
        "Save + FOS_ALLOWMULTISELECT",
        "ShowFilePicker",
        "ShowFolderPicker",
        "ShowRemote: UtilityProcessManager::GetSingleton",
        "ShowRemote: invocation of CreateWinFileDialogActor",
        "UtilityProcessManager::CreateWinFileDialogActor",
        "internal IPC failure?",
    };
    constexpr static size_t VALID_STRINGS_COUNT =
        std::extent_v<decltype(VALID_STRINGS)>;

    // Prevent duplicates from occurring in VALID_STRINGS by forcing it to be
    // sorted. (Note that std::is_sorted is not constexpr until C++20.)
    static_assert(
        []() {
          for (size_t i = 0; i + 1 < VALID_STRINGS_COUNT; ++i) {
            if (!(VALID_STRINGS[i] < VALID_STRINGS[i + 1])) {
              return false;
            }
          }
          return true;
        }(),
        "VALID_STRINGS should be ASCIIbetically sorted");

   public:
    constexpr uint32_t Serialize() const { return value; }
    constexpr static Location Deserialize(uint32_t val) {
      return Location{val};
    }

   public:
    constexpr static Location npos() { return Location{~uint32_t(0)}; }

    constexpr bool IsValid() const { return value < VALID_STRINGS_COUNT; }

    constexpr std::string_view ToString() const {
      return value < VALID_STRINGS_COUNT ? VALID_STRINGS[value]
                                         : "<bad filedialog::Error::Location?>";
    }
    constexpr static Location FromString(std::string_view str) {
      for (uint32_t i = 0; i < VALID_STRINGS_COUNT; ++i) {
        if (str == VALID_STRINGS[i]) return Location{i};
      }
      return npos();
    }

    constexpr char const* c_str() const { return ToString().data(); }
  };

  // Where and how (run-time) this error occurred.
  Kind kind;
  // Where (compile-time) this error occurred.
  Location where;
  // Why (run-time) this error occurred. Probably an HRESULT.
  uint32_t why;

  // `impl Debug for Kind`
  static const char* KindName(Kind);
};

// Create a filedialog::Error, confirming at compile-time that the supplied
// where-string is valid.
#define MOZ_FD_ERROR(kind_, where_, why_)                                 \
  ([](HRESULT why_arg_) -> ::mozilla::widget::filedialog::Error {         \
    using Error = ::mozilla::widget::filedialog::Error;                   \
    constexpr static const Error::Location loc =                          \
        Error::Location::FromString(where_);                              \
    static_assert(                                                        \
        loc.IsValid(),                                                    \
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
