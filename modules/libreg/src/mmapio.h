/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is James L. Nance.
 * Portions created by James L. Nance are Copyright (C) 1999, James
 * L. Nance.  All Rights Reserved.
 * 
 * Contributor(s):  James L. Nance <jim_nance@yahoo.com>
 */

#include "prio.h"

typedef struct MmioFileStruct MmioFile;

PRStatus mmio_FileSeek(MmioFile *file, PRInt32 offset, PRSeekWhence whence);
PRInt32  mmio_FileRead(MmioFile *file, char *dest, PRInt32 count);
PRInt32  mmio_FileWrite(MmioFile *file, const char *src, PRInt32 count);
PRInt32  mmio_FileTell(MmioFile *file);
PRStatus mmio_FileClose(MmioFile *file);
MmioFile *mmio_FileOpen(char *path, PRIntn flags, PRIntn mode);
