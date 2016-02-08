// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "sandbox/win/src/filesystem_policy.h"

#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/win_utils.h"

namespace {

NTSTATUS NtCreateFileInTarget(HANDLE* target_file_handle,
                              ACCESS_MASK desired_access,
                              OBJECT_ATTRIBUTES* obj_attributes,
                              IO_STATUS_BLOCK* io_status_block,
                              ULONG file_attributes,
                              ULONG share_access,
                              ULONG create_disposition,
                              ULONG create_options,
                              PVOID ea_buffer,
                              ULONG ea_lenght,
                              HANDLE target_process) {
  NtCreateFileFunction NtCreateFile = NULL;
  ResolveNTFunctionPtr("NtCreateFile", &NtCreateFile);

  HANDLE local_handle = INVALID_HANDLE_VALUE;
  NTSTATUS status = NtCreateFile(&local_handle, desired_access, obj_attributes,
                                 io_status_block, NULL, file_attributes,
                                 share_access, create_disposition,
                                 create_options, ea_buffer, ea_lenght);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  if (!sandbox::SameObject(local_handle, obj_attributes->ObjectName->Buffer)) {
    // The handle points somewhere else. Fail the operation.
    ::CloseHandle(local_handle);
    return STATUS_ACCESS_DENIED;
  }

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle,
                         target_process, target_file_handle, 0, FALSE,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return STATUS_ACCESS_DENIED;
  }
  return STATUS_SUCCESS;
}

}  // namespace.

namespace sandbox {

bool FileSystemPolicy::GenerateRules(const wchar_t* name,
                                     TargetPolicy::Semantics semantics,
                                     LowLevelPolicy* policy) {
  base::string16 mod_name(name);
  if (mod_name.empty()) {
    return false;
  }

  if (!PreProcessName(mod_name, &mod_name)) {
    // The path to be added might contain a reparse point.
    NOTREACHED();
    return false;
  }

  // TODO(cpu) bug 32224: This prefix add is a hack because we don't have the
  // infrastructure to normalize names. In any case we need to escape the
  // question marks.
  if (_wcsnicmp(mod_name.c_str(), kNTDevicePrefix, kNTDevicePrefixLen)) {
    mod_name = FixNTPrefixForMatch(mod_name);
    name = mod_name.c_str();
  }

  EvalResult result = ASK_BROKER;

  // List of supported calls for the filesystem.
  const unsigned kCallNtCreateFile = 0x1;
  const unsigned kCallNtOpenFile = 0x2;
  const unsigned kCallNtQueryAttributesFile = 0x4;
  const unsigned kCallNtQueryFullAttributesFile = 0x8;
  const unsigned kCallNtSetInfoRename = 0x10;

  DWORD  rule_to_add = kCallNtOpenFile | kCallNtCreateFile |
                       kCallNtQueryAttributesFile |
                       kCallNtQueryFullAttributesFile | kCallNtSetInfoRename;

  PolicyRule create(result);
  PolicyRule open(result);
  PolicyRule query(result);
  PolicyRule query_full(result);
  PolicyRule rename(result);

  switch (semantics) {
    case TargetPolicy::FILES_ALLOW_DIR_ANY: {
      open.AddNumberMatch(IF, OpenFile::OPTIONS, FILE_DIRECTORY_FILE, AND);
      create.AddNumberMatch(IF, OpenFile::OPTIONS, FILE_DIRECTORY_FILE, AND);
      break;
    }
    case TargetPolicy::FILES_ALLOW_READONLY: {
      // We consider all flags that are not known to be readonly as potentially
      // used for write.
      DWORD allowed_flags = FILE_READ_DATA | FILE_READ_ATTRIBUTES |
                            FILE_READ_EA | SYNCHRONIZE | FILE_EXECUTE |
                            GENERIC_READ | GENERIC_EXECUTE | READ_CONTROL;
      DWORD restricted_flags = ~allowed_flags;
      open.AddNumberMatch(IF_NOT, OpenFile::ACCESS, restricted_flags, AND);
      create.AddNumberMatch(IF_NOT, OpenFile::ACCESS, restricted_flags, AND);

      // Read only access don't work for rename.
      rule_to_add &= ~kCallNtSetInfoRename;
      break;
    }
    case TargetPolicy::FILES_ALLOW_QUERY: {
      // Here we don't want to add policy for the open or the create.
      rule_to_add &= ~(kCallNtOpenFile | kCallNtCreateFile |
                       kCallNtSetInfoRename);
      break;
    }
    case TargetPolicy::FILES_ALLOW_ANY: {
      break;
    }
    default: {
      NOTREACHED();
      return false;
    }
  }

  if ((rule_to_add & kCallNtCreateFile) &&
      (!create.AddStringMatch(IF, OpenFile::NAME, name, CASE_INSENSITIVE) ||
       !policy->AddRule(IPC_NTCREATEFILE_TAG, &create))) {
    return false;
  }

  if ((rule_to_add & kCallNtOpenFile) &&
      (!open.AddStringMatch(IF, OpenFile::NAME, name, CASE_INSENSITIVE) ||
       !policy->AddRule(IPC_NTOPENFILE_TAG, &open))) {
    return false;
  }

  if ((rule_to_add & kCallNtQueryAttributesFile) &&
      (!query.AddStringMatch(IF, FileName::NAME, name, CASE_INSENSITIVE) ||
       !policy->AddRule(IPC_NTQUERYATTRIBUTESFILE_TAG, &query))) {
    return false;
  }

  if ((rule_to_add & kCallNtQueryFullAttributesFile) &&
      (!query_full.AddStringMatch(IF, FileName::NAME, name, CASE_INSENSITIVE)
       || !policy->AddRule(IPC_NTQUERYFULLATTRIBUTESFILE_TAG,
                           &query_full))) {
    return false;
  }

  if ((rule_to_add & kCallNtSetInfoRename) &&
      (!rename.AddStringMatch(IF, FileName::NAME, name, CASE_INSENSITIVE) ||
       !policy->AddRule(IPC_NTSETINFO_RENAME_TAG, &rename))) {
    return false;
  }

  return true;
}

// Right now we insert two rules, to be evaluated before any user supplied rule:
// - go to the broker if the path doesn't look like the paths that we push on
//    the policy (namely \??\something).
// - go to the broker if it looks like this is a short-name path.
//
// It is possible to add a rule to go to the broker in any case; it would look
// something like:
//    rule = new PolicyRule(ASK_BROKER);
//    rule->AddNumberMatch(IF_NOT, FileName::BROKER, TRUE, AND);
//    policy->AddRule(service, rule);
bool FileSystemPolicy::SetInitialRules(LowLevelPolicy* policy) {
  PolicyRule format(ASK_BROKER);
  PolicyRule short_name(ASK_BROKER);

  bool rv = format.AddNumberMatch(IF_NOT, FileName::BROKER, TRUE, AND);
  rv &= format.AddStringMatch(IF_NOT, FileName::NAME, L"\\/?/?\\*",
                              CASE_SENSITIVE);

  rv &= short_name.AddNumberMatch(IF_NOT, FileName::BROKER, TRUE, AND);
  rv &= short_name.AddStringMatch(IF, FileName::NAME, L"*~*", CASE_SENSITIVE);

  if (!rv || !policy->AddRule(IPC_NTCREATEFILE_TAG, &format))
    return false;

  if (!policy->AddRule(IPC_NTCREATEFILE_TAG, &short_name))
    return false;

  if (!policy->AddRule(IPC_NTOPENFILE_TAG, &format))
    return false;

  if (!policy->AddRule(IPC_NTOPENFILE_TAG, &short_name))
    return false;

  if (!policy->AddRule(IPC_NTQUERYATTRIBUTESFILE_TAG, &format))
    return false;

  if (!policy->AddRule(IPC_NTQUERYATTRIBUTESFILE_TAG, &short_name))
    return false;

  if (!policy->AddRule(IPC_NTQUERYFULLATTRIBUTESFILE_TAG, &format))
    return false;

  if (!policy->AddRule(IPC_NTQUERYFULLATTRIBUTESFILE_TAG, &short_name))
    return false;

  if (!policy->AddRule(IPC_NTSETINFO_RENAME_TAG, &format))
    return false;

  if (!policy->AddRule(IPC_NTSETINFO_RENAME_TAG, &short_name))
    return false;

  return true;
}

bool FileSystemPolicy::CreateFileAction(EvalResult eval_result,
                                        const ClientInfo& client_info,
                                        const base::string16 &file,
                                        uint32 attributes,
                                        uint32 desired_access,
                                        uint32 file_attributes,
                                        uint32 share_access,
                                        uint32 create_disposition,
                                        uint32 create_options,
                                        HANDLE *handle,
                                        NTSTATUS* nt_status,
                                        ULONG_PTR *io_information) {
  // The only action supported is ASK_BROKER which means create the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return false;
  }
  IO_STATUS_BLOCK io_block = {0};
  UNICODE_STRING uni_name = {0};
  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitObjectAttribs(file, attributes, NULL, &obj_attributes, &uni_name);
  *nt_status = NtCreateFileInTarget(handle, desired_access, &obj_attributes,
                                    &io_block, file_attributes, share_access,
                                    create_disposition, create_options, NULL,
                                    0, client_info.process);

  *io_information = io_block.Information;
  return true;
}

bool FileSystemPolicy::OpenFileAction(EvalResult eval_result,
                                      const ClientInfo& client_info,
                                      const base::string16 &file,
                                      uint32 attributes,
                                      uint32 desired_access,
                                      uint32 share_access,
                                      uint32 open_options,
                                      HANDLE *handle,
                                      NTSTATUS* nt_status,
                                      ULONG_PTR *io_information) {
  // The only action supported is ASK_BROKER which means open the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return true;
  }
  // An NtOpen is equivalent to an NtCreate with FileAttributes = 0 and
  // CreateDisposition = FILE_OPEN.
  IO_STATUS_BLOCK io_block = {0};
  UNICODE_STRING uni_name = {0};
  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitObjectAttribs(file, attributes, NULL, &obj_attributes, &uni_name);
  *nt_status = NtCreateFileInTarget(handle, desired_access, &obj_attributes,
                                    &io_block, 0, share_access, FILE_OPEN,
                                    open_options, NULL, 0,
                                    client_info.process);

  *io_information = io_block.Information;
  return true;
}

bool FileSystemPolicy::QueryAttributesFileAction(
    EvalResult eval_result,
    const ClientInfo& client_info,
    const base::string16 &file,
    uint32 attributes,
    FILE_BASIC_INFORMATION* file_info,
    NTSTATUS* nt_status) {
  // The only action supported is ASK_BROKER which means query the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  NtQueryAttributesFileFunction NtQueryAttributesFile = NULL;
  ResolveNTFunctionPtr("NtQueryAttributesFile", &NtQueryAttributesFile);

  UNICODE_STRING uni_name = {0};
  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitObjectAttribs(file, attributes, NULL, &obj_attributes, &uni_name);
  *nt_status = NtQueryAttributesFile(&obj_attributes, file_info);

  return true;
}

bool FileSystemPolicy::QueryFullAttributesFileAction(
    EvalResult eval_result,
    const ClientInfo& client_info,
    const base::string16 &file,
    uint32 attributes,
    FILE_NETWORK_OPEN_INFORMATION* file_info,
    NTSTATUS* nt_status) {
  // The only action supported is ASK_BROKER which means query the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  NtQueryFullAttributesFileFunction NtQueryFullAttributesFile = NULL;
  ResolveNTFunctionPtr("NtQueryFullAttributesFile", &NtQueryFullAttributesFile);

  UNICODE_STRING uni_name = {0};
  OBJECT_ATTRIBUTES obj_attributes = {0};
  InitObjectAttribs(file, attributes, NULL, &obj_attributes, &uni_name);
  *nt_status = NtQueryFullAttributesFile(&obj_attributes, file_info);

  return true;
}

bool FileSystemPolicy::SetInformationFileAction(
    EvalResult eval_result, const ClientInfo& client_info,
    HANDLE target_file_handle, void* file_info, uint32 length,
    uint32 info_class, IO_STATUS_BLOCK* io_block,
    NTSTATUS* nt_status) {
  // The only action supported is ASK_BROKER which means open the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  NtSetInformationFileFunction NtSetInformationFile = NULL;
  ResolveNTFunctionPtr("NtSetInformationFile", &NtSetInformationFile);

  HANDLE local_handle = NULL;
  if (!::DuplicateHandle(client_info.process, target_file_handle,
                         ::GetCurrentProcess(), &local_handle, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    *nt_status = STATUS_ACCESS_DENIED;
    return true;
  }

  base::win::ScopedHandle handle(local_handle);

  FILE_INFORMATION_CLASS file_info_class =
      static_cast<FILE_INFORMATION_CLASS>(info_class);
  *nt_status = NtSetInformationFile(local_handle, io_block, file_info, length,
                                    file_info_class);

  return true;
}

bool PreProcessName(const base::string16& path, base::string16* new_path) {
  ConvertToLongPath(path, new_path);

  bool reparsed = false;
  if (ERROR_SUCCESS != IsReparsePoint(*new_path, &reparsed))
    return false;

  // We can't process reparsed file.
  return !reparsed;
}

base::string16 FixNTPrefixForMatch(const base::string16& name) {
  base::string16 mod_name = name;

  // NT prefix escaped for rule matcher
  const wchar_t kNTPrefixEscaped[] = L"\\/?/?\\";
  const int kNTPrefixEscapedLen = arraysize(kNTPrefixEscaped) - 1;

  if (0 != mod_name.compare(0, kNTPrefixLen, kNTPrefix)) {
    if (0 != mod_name.compare(0, kNTPrefixEscapedLen, kNTPrefixEscaped)) {
      // TODO(nsylvain): Find a better way to do name resolution. Right now we
      // take the name and we expand it.
      mod_name.insert(0, kNTPrefixEscaped);
    }
  } else {
    // Start of name matches NT prefix, replace with escaped format
    // Fixes bug: 334882
    mod_name.replace(0, kNTPrefixLen, kNTPrefixEscaped);
  }

  return mod_name;
}

}  // namespace sandbox
