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
