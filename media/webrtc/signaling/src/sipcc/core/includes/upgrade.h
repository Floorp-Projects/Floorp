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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#ifndef _UPGRADE_INCLUDED_H
#define _UPGRADE_INCLUDED_H

#include "cpr_types.h"
#include "phone.h"

/*
 * Telecaster has these extra 32 bytes of data
 * in front of the load header.  I have no idea what
 * they are for, but having a structure makes it easier
 * to deal with this as opposed to adding 0x20 to the
 * address.
 */
typedef struct
{
    uint8_t vectors[0x20];
    LoadHdr hdr;
} BigLoadHdr;

/*
 * Prototypes for public functions
 */
void upgrade_bootup_init(void);
int upgrade_done(int tftp_rc);
void upgrade_start(const char *fname);
void upgrade_memcpy(void *dst, const void *src, int len);
int upgrade_check(const char *loadid);
int upgrade_validate_app_image(const BigLoadHdr *load);
int upgrade_validate_dsp_image(const DSPLoadHdr *dsp);
void upgrade_erase_dir_storage(void);
void upgrade_write_dir_storage(char *buffer, char *flash, int size);

#endif /* _UPGRADE_INCLUDED_H */
