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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
/*
 * All the data structures for Software fortezza are internal only.
 * The external API for Software fortezza is MACI (which is only used by
 * the PKCS #11 module.
 */

#ifndef _SWFORTT_H_
#define _SWFORTT_H_

/* structure typedefs */
typedef struct FORTKeySlotStr FORTKeySlot;
typedef struct FORTRaRegistersStr  FORTRaRegisters;
typedef struct FORTSWTokenStr  FORTSWToken;

/* Der parsing typedefs */
typedef struct fortKeyInformationStr fortKeyInformation;
typedef struct fortProtectedDataStr fortProtectedData;
typedef struct fortSlotEntryStr fortSlotEntry;
typedef struct fortProtectedPhraseStr fortProtectedPhrase;
typedef struct FORTSWFileStr FORTSWFile;
typedef struct FORTSignedSWFileStr FORTSignedSWFile;


#endif
