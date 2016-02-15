/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <dbghelp.h>

const DWORD CV_SIGNATURE_RSDS = 0x53445352; // 'SDSR'

struct CV_INFO_PDB70 {
  DWORD      CvSignature;
  GUID       Signature;
  DWORD      Age;
  BYTE       PdbFileName[1];
};

void print_guid(const GUID& guid, DWORD age)
{
  printf("%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%X",
         guid.Data1, guid.Data2, guid.Data3,
         guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
         guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7],
         age);
}

int main(int argc, char** argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: fileid <file>\n");
    return 1;
  }

  HANDLE file = CreateFileA(argv[1],
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            nullptr,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "Couldn't open file: %s\n", argv[1]);
    return 1;
  }

  HANDLE mapFile = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, 0);
  if (mapFile == nullptr) {
    fprintf(stderr, "Couldn't create file mapping\n");
    CloseHandle(file);
    return 1;
  }

  uint8_t* base = reinterpret_cast<uint8_t*>(MapViewOfFile(mapFile,
                                                           FILE_MAP_READ,
                                                           0,
                                                           0,
                                                           0));
  if (base == nullptr) {
    fprintf(stderr, "Couldn't map file\n");
    CloseHandle(mapFile);
    CloseHandle(file);
    return 1;
  }

  DWORD size;
  PIMAGE_DEBUG_DIRECTORY debug_dir =
    reinterpret_cast<PIMAGE_DEBUG_DIRECTORY>(
      ImageDirectoryEntryToDataEx(base,
                                  FALSE,
                                  IMAGE_DIRECTORY_ENTRY_DEBUG,
                                  &size,
                                  nullptr));

  bool found = false;
  if (debug_dir->Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
    CV_INFO_PDB70* cv =
      reinterpret_cast<CV_INFO_PDB70*>(base + debug_dir->PointerToRawData);
    if (cv->CvSignature == CV_SIGNATURE_RSDS) {
      found = true;
      print_guid(cv->Signature, cv->Age);
    }
  }

  UnmapViewOfFile(base);
  CloseHandle(mapFile);
  CloseHandle(file);

  return found ? 0 : 1;
}
