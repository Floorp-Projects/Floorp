/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CFGFILE_UTILS_H_
#define _CFGFILE_UTILS_H_

#include "cpr_types.h"

//=============================================================================
//
//  Structure/Type definitions
//
//-----------------------------------------------------------------------------

struct var_struct;

typedef int (*parse_func_t)(const struct var_struct *, const char *);
typedef int (*print_func_t)(const struct var_struct *, char *, int);

typedef struct {
    const char *name;
    int         value;
} key_table_entry_t;

#define NULL_KEY  (-1)

typedef struct var_struct {
    const char  *name;
    void        *addr;
    int          length;
    parse_func_t parse_func;
    print_func_t print_func;
    const key_table_entry_t *key_table;
} var_t;

/*********************************************************
 *
 *  Config Table "Helper" Routines
 *
 *  These #defines are routines that are called from the
 *  config table entries to parse (PA), print (PR),
 *  and export (XP), different config entries.  These are the
 *  "common" helper routines.  Protocol-specific routines
 *  are located in prot_configmgr_private.h
 *
 *********************************************************/
#define PA_IP       cfgfile_parse_ip
#define PR_IP       cfgfile_print_ip
#define PR_IPN      cfgfile_print_ip_ntohl
#define PA_STR      cfgfile_parse_str
#define PR_STR      cfgfile_print_str
#define PA_INT      cfgfile_parse_int
#define PR_INT      cfgfile_print_int
#define PA_KEY      cfgfile_parse_key
#define PA_KEYE     cfgfile_parse_key_entry
#define PR_KEY      cfgfile_print_key
#define PR_KEYE     cfgfile_print_key_entry
#define PR_MAC      cfgfile_print_mac
#define XP_NONE     0

/*********************************************************
 *
 *  Config Table "Helper" Macros
 *
 *  These macros are used to help build the actual config
 *  table.  They provide the address and length of the
 *  entries.  They also tell which table the entry is
 *  stored in.
 *
 *********************************************************/
#define CFGADDR(field) ((void*)&(prot_cfg_block.field))
#define CFGLEN(field)  (sizeof(prot_cfg_block.field))
#define CFGVAR(field)  CFGADDR(field),CFGLEN(field)

/* generic config file parsing functions */
int cfgfile_parse_ip(const var_t *, const char *value);
int cfgfile_parse_str(const var_t *, const char *value);
int cfgfile_parse_int(const var_t *, const char *value);
int cfgfile_parse_key(const var_t *, const char *value);
int cfgfile_parse_key_entry(const var_t *, const char *value);

/* generic config file printing functions */
int cfgfile_print_ip(const var_t *, char *buf, int);
int cfgfile_print_ip_ntohl(const var_t *, char *buf, int);
int cfgfile_print_str(const var_t *, char *buf, int);
int cfgfile_print_int(const var_t *, char *buf, int);
int cfgfile_print_key(const var_t *, char *buf, int);
int cfgfile_print_key_entry(const var_t *, char *buf, int);

/* generic config file export (print) functions */
int sprint_ip(char *, uint32_t ip);
int sprint_mac(char *, const unsigned char *ptr);
int cfgfile_print_mac(const var_t *entry, char *buf, int);

#endif /* _CFGFILE_UTILS_H_ */
