/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
