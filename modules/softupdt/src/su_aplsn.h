/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef APSINGLE_H
#define APSINGLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "xp_core.h"
#include "su_applsn_defines.h"


#include <Files.h>

#define APPLESINGLE_MIME_TYPE "application/applefile"

XP_BEGIN_PROTOS
//int
//convert_apsingle_file(const char *inFile, int inflags, const char *outFile, int outflags, int *bin);

/* Decodes
 * Arguments:
 * inFile - name of the AppleSingle file
 * outSpec - destination. If destination is renamed (as part of decoding of realName)
 *                      the outSpec is modified to represent the new name
 * wantedEntries - a bitmask what entries do we want decoded. Bit positions correspond to entryIDs
 */
int
SU_DecodeAppleSingle( const char * inSrc, char ** dst );

/*
 * Arguments:
 * inSpec - file to be encoded
 * outSpec - destination. If an error occurs during decoding, destination will be deleted!
 * wantedEntries - a bitmask what entries do we want encoded. Bit positions correspond to entryIDs
 */
int
SU_EncodeAppleSingle( const char * inSpec, const char * outFile);

XP_END_PROTOS


#ifdef __cplusplus
}
#endif

#endif /* APSINGLE_H */
