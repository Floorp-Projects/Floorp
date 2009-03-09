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
 * The Original Code is McCoy.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com>
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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>

/*
Icon files are made up of:

IconHeader
IconDirEntry1
IconDirEntry2
...
IconDirEntryN
IconData1
IconData2
...
IconDataN

Each IconData must be added as a new RT_ICON resource to the exe. Then
an RT_GROUP_ICON resource must be added that contains an equivalent
header:

IconHeader
IconResEntry1
IconResEntry2
...
IconResEntryN
*/

#pragma pack(push, 2)
typedef struct
{
  WORD Reserved;
  WORD ResourceType;
  WORD ImageCount;
} IconHeader;

typedef struct
{
  BYTE Width;
  BYTE Height;
  BYTE Colors;
  BYTE Reserved;
  WORD Planes;
  WORD BitsPerPixel;
  DWORD ImageSize;
  DWORD ImageOffset;
} IconDirEntry;

typedef struct
{
  BYTE Width;
  BYTE Height;
  BYTE Colors;
  BYTE Reserved;
  WORD Planes;
  WORD BitsPerPixel;
  DWORD ImageSize;
  WORD ResourceID;    // This field is the one difference to above
} IconResEntry;
#pragma pack(pop)

int
main(int argc, char **argv)
{
  if (argc != 3) {
    printf("Usage: redit <exe file> <icon file>\n");
    return 1;
  }

  int file = _open(argv[2], _O_BINARY | _O_RDONLY);
  if (file == -1) {
    fprintf(stderr, "Unable to open icon file.\n");
    return 1;
  }

  // Load all the data from the icon file
  long filesize = _filelength(file);
  char* data = (char*)malloc(filesize);
  _read(file, data, filesize);
  _close(file);
  IconHeader* header = (IconHeader*)data;

  // Open the target library for updating
  HANDLE updateRes = BeginUpdateResource(argv[1], FALSE);
  if (updateRes == NULL) {
    fprintf(stderr, "Unable to open library for modification.\n");
    free(data);
    return 1;
  }

  // Allocate the group resource entry
  long groupsize = sizeof(IconHeader) + header->ImageCount * sizeof(IconResEntry);
  char* group = (char*)malloc(groupsize);
  if (!group) {
    fprintf(stderr, "Failed to allocate memory for new images.\n");
    free(data);
    return 1;
  }
  memcpy(group, data, sizeof(IconHeader));

  IconDirEntry* sourceIcon = (IconDirEntry*)(data + sizeof(IconHeader));
  IconResEntry* targetIcon = (IconResEntry*)(group + sizeof(IconHeader));

  for (int id = 1; id <= header->ImageCount; id++) {
    // Add the individual icon
    if (!UpdateResource(updateRes, RT_ICON, MAKEINTRESOURCE(id),
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                        data + sourceIcon->ImageOffset, sourceIcon->ImageSize)) {
      fprintf(stderr, "Unable to update resource.\n");
      EndUpdateResource(updateRes, TRUE);  // Discard changes, ignore errors
      free(data);
      free(group);
      return 1;
    }
    // Copy the data for this icon (note that the structs have different sizes)
    memcpy(targetIcon, sourceIcon, sizeof(IconResEntry));
    targetIcon->ResourceID = id;
    sourceIcon++;
    targetIcon++;
  }
  free(data);

  if (!UpdateResource(updateRes, RT_GROUP_ICON, "MAINICON",
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                      group, groupsize)) {
    fprintf(stderr, "Unable to update resource.\n");
    EndUpdateResource(updateRes, TRUE);  // Discard changes
    free(group);
    return 1;
  }

  free(group);

  // Save the modifications
  if (!EndUpdateResource(updateRes, FALSE)) {
    fprintf(stderr, "Unable to write changes to library.\n");
    return 1;
  }

  return 0;
}
