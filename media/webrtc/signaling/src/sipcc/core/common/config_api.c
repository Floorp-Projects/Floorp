/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "config.h"
#include "dns_utils.h"
#include "phone_debug.h"
#include "ccapi.h"
#include "debug.h"

cc_int32_t ConfigDebug;

/*
 * This file contains the API routines that are used to
 * access the config table.
 *
 * Avoid writing more of these routines.
 * Try and reuse these routines as much as possible.
 *
 * We should be able to set and retrieve any type of value
 * using one of these routines.
 */


/*
 *  Function: config_get_string()
 *
 *  Description: Get any arbitrary config entry as a string
 *
 *  Parameters: id - The id of the config string to get
 *              buffer - Empty buffer where string will be copied
 *              buffer_len -  length of the buffer where string will be copied
 *
 *  Returns: None
 */
void
config_get_string (int id, char *buffer, int buffer_len)
{
    const var_t *entry;
    char *buf_start;

    /*
     * Set the result to be empty in case we can't find anything
     */
    buffer[0] = 0;
    if ((id >= 0) && (id < CFGID_PROTOCOL_MAX)) {
        entry = &prot_cfg_table[id];
        if (entry->length > buffer_len) {
            CONFIG_ERROR(CFG_F_PREFIX"insufficient buffer: %d", "config_get_string",
                    id);
        } else {
            buf_start = buffer;
            entry->print_func(entry, buffer, buffer_len);
            CONFIG_DEBUG(DEB_F_PREFIX"CFGID %d: get str: %s = %s", DEB_F_PREFIX_ARGS(CONFIG_API, "config_get_string"), id, entry->name,
                         buf_start);
        }
    } else {
        CONFIG_ERROR(CFG_F_PREFIX"Invalid ID: %d", "config_get_string", id);
    }
}


/*
 *  Function: config_set_string()
 *
 *  Parameters: id - The id of the config string to set
 *              buffer - The new value for the string
 *
 *  Description: Set any arbitrary config entry as a string
 *
 *  Returns: None
 */
void
config_set_string (int id, char *buffer)
{
    const var_t *entry;

    if ((id >= 0) && (id < CFGID_PROTOCOL_MAX)) {
        entry = &prot_cfg_table[id];
        if (entry->parse_func(entry, buffer)) {
            /* Parse function returned an error */
            CONFIG_ERROR(CFG_F_PREFIX"Parse function failed. ID: %d %s:%s", "config_set_string", id, entry->name, buffer);
        } else {
            CONFIG_DEBUG(DEB_F_PREFIX"CFGID %d: %s set str to %s", DEB_F_PREFIX_ARGS(CONFIG_API, "config_set_string"), id, entry->name,
                         buffer);
        }
    } else {
        CONFIG_ERROR(CFG_F_PREFIX"Invalid ID: %d", "config_set_string", id);
    }
}

#define MAX_CONFIG_VAL_PRINT_LEN 258
/*
 *  Function: print_config_value()
 *
 *  Description: If debug is enabled then print value contained in
 *               the buffer. Cast and dereference the buffer ptr
 *               according to length. If no match to char, short,
 *               int or long then just print each byte (ex: MacAddr).
 *               Called by config_set/get_value() function.
 *
 *  Parameters: id         - the id of the config value to get
 *              get_set    - config action (get val or set val)
 *              entry_name - config id name
 *              buffer     - buffer containing the value
 *              length     - number of bytes in the buffer
 *
 *  Returns: none
 */
/*
 * Some logical upper limit to avoid long print out in case
 * of large length value
 */
void
print_config_value (int id, char *get_set, const char *entry_name,
                    void *buffer, int length)
{
    long  long_val  = 0;
    int   int_val   = 0;
    short short_val = 0;
    char  char_val  = 0;
    char  str[MAX_CONFIG_VAL_PRINT_LEN];
    char *in_ptr;
    char *str_ptr;

    if (length == sizeof(char)) {
        char_val = *(char *) buffer;
        long_val = (long) char_val;
        CONFIG_DEBUG(DEB_F_PREFIX"CFGID %d: %s: %s = %ld", DEB_F_PREFIX_ARGS(CONFIG_API, "print_config_value"), id, get_set, entry_name,
                     long_val);
    } else if (length == sizeof(short)) {
        short_val = *(short *) buffer;
        long_val = (long) short_val;
        CONFIG_DEBUG(DEB_F_PREFIX"CFGID %d: %s: %s = %ld", DEB_F_PREFIX_ARGS(CONFIG_API, "print_config_value"), id, get_set, entry_name,
                     long_val);
    } else if (length == sizeof(int)) {
        int_val = *(int *) buffer;
        long_val = (long) int_val;
        CONFIG_DEBUG(DEB_F_PREFIX"CFGID %d: %s: %s = %ld", DEB_F_PREFIX_ARGS(CONFIG_API, "print_config_value"), id, get_set, entry_name,
                     long_val);
    } else if (length == sizeof(long)) {
        long_val = *(long *) buffer;
        CONFIG_DEBUG(DEB_F_PREFIX"CFGID %d: %s: %s = %ld", DEB_F_PREFIX_ARGS(CONFIG_API, "print_config_value"), id, get_set, entry_name,
                     long_val);
    } else if (length < MAX_CONFIG_VAL_PRINT_LEN / 2) {

        in_ptr = (char *) buffer;
        str_ptr = &str[0];
        while (length--) {
            sprintf(str_ptr++, "%02x", *in_ptr++);
            str_ptr++;
        }
        *str_ptr = '\0';
        CONFIG_DEBUG(DEB_F_PREFIX"CFGID %d: %s: %s = %s", DEB_F_PREFIX_ARGS(CONFIG_API, "print_config_value"), id, get_set, entry_name, str);
    } else {
        CONFIG_ERROR(CFG_F_PREFIX"cfg_id = %d length too long -> %d", "print_config_value",
                id, length);
    }
}

/*
 *  Function: config_get_value()
 *
 *  Description: Get any arbitrary config entry as a raw data value.
 *    If the length doesn't match the actual length of the field,
 *    nothing will be copied.
 *
 *  Parameters: id     - The id of the config value to get
 *              buffer - Empty buffer where value will be copied
 *              length - The number of bytes to get
 *
 *  Returns: None
 */
void
config_get_value (int id, void *buffer, int length)
{
    const var_t *entry;

    /*
     *  Retrieve raw entry from table.....
     */
    if ((id >= 0) && (id < CFGID_PROTOCOL_MAX)) {
        entry = &prot_cfg_table[id];
        if (length == entry->length) {
            memcpy(buffer, entry->addr, entry->length);

            if (ConfigDebug) {
                print_config_value(id, "Get Val", entry->name, buffer, length);
            }
        } else {
            CONFIG_ERROR(CFG_F_PREFIX"%s size error", "config_get_value",
                    entry->name);
        }
    } else {
        CONFIG_ERROR(CFG_F_PREFIX"Invalid ID: %d", "config_get_value", id);
    }
}


/*
 *  Function: config_set_value()
 *
 *  Description: Set arbitrary config entry as a raw data value.
 *    If the length doesn't match the actual length of the field,
 *    nothing will be copied.
 *
 *  Parameters: id     - The id of the config value to set
 *              buffer - The new value to be set
 *              length - The number of bytes to set
 *
 *  Returns: None
 */
void
config_set_value (int id, void *buffer, int length)
{
    const var_t *entry;

    /*
     *  Retrieve entry from table.....
     */
    if ((id >= 0) && (id < CFGID_PROTOCOL_MAX)) {
        entry = &prot_cfg_table[id];
        if (entry->length != length) {
            CONFIG_ERROR(CFG_F_PREFIX" %s size error entry size=%d, len=%d",
                    "config_set_value", entry->name, entry->length, length);
            return;
        }
        memcpy(entry->addr, buffer, entry->length);
        if (ConfigDebug) {
            print_config_value(id, "Set Val", entry->name, buffer, length);
        }
    } else {
        CONFIG_ERROR(CFG_F_PREFIX"Invalid ID: %d", "config_set_value", id);
    }
}

/* Function: get_printable_cfg()
 *
 *  Description: prints the config value in the buf
 *
 *  Parameters:  indx, buf, len
 *
 *  Returns:     buf
 */
char *
get_printable_cfg(unsigned int indx, char *buf, unsigned int len)
{
   const var_t *table;
   buf[0]=0;

   table = &prot_cfg_table[indx];
   // If this field has a password, print the param name, but NOT the
   // real password
   if (indx>=CFGID_LINE_PASSWORD && indx < CFGID_LINE_PASSWORD+MAX_CONFIG_LINES) {
     // and add an invisible one
     sstrncpy(buf, "**********", MAX_CONFIG_VAL_PRINT_LEN);
   } else if ( table->print_func ) {
     table->print_func(table, buf, len);
   }

   if ( buf[0] == 0 ) {
     sstrncpy(buf,"EMPTY", len);
   }
   return buf;
}

/*
 *  Function: show_config_cmd()
 *
 *  Description: Callback passed in the config init routine for show config
 *
 *  Parameters:  argc, argv
 *
 *  Returns:     zero(0)
 */

cc_int32_t
show_config_cmd (cc_int32_t argc, const char *argv[])
{
    const var_t *table;
    char buf[MAX_CONFIG_VAL_PRINT_LEN];
    int i, feat;

    debugif_printf("\n------ Current *Cache* Configuration ------\n");
    table = prot_cfg_table;

    for ( i=0; i < CFGID_LINE_FEATURE; i++ ) {
        if (table->print_func) {
            table->print_func(table, buf, sizeof(buf));

            // If this field has a password, print the param name, but NOT the
            // real password
            if (strstr(table->name, "Password") != 0) {
                // and add an invisible one
                sstrncpy(buf, "**********", sizeof(buf));
            }
            debugif_printf("%s : %s\n", table->name, buf);
        }
        table++;
    }

    debugif_printf("%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s",
        prot_cfg_table[CFGID_LINE_INDEX].name,
        prot_cfg_table[CFGID_LINE_FEATURE].name,
        prot_cfg_table[CFGID_PROXY_ADDRESS].name,
        prot_cfg_table[CFGID_PROXY_PORT].name,
        prot_cfg_table[CFGID_LINE_CALL_WAITING].name,
        prot_cfg_table[CFGID_LINE_MSG_WAITING_LAMP].name,
        prot_cfg_table[CFGID_LINE_MESSAGE_WAITING_AMWI].name,
        prot_cfg_table[CFGID_LINE_RING_SETTING_IDLE].name,
        prot_cfg_table[CFGID_LINE_RING_SETTING_ACTIVE].name,
        prot_cfg_table[CFGID_LINE_NAME].name,
        prot_cfg_table[CFGID_LINE_AUTOANSWER_ENABLED].name,
        prot_cfg_table[CFGID_LINE_AUTOANSWER_MODE].name,
        prot_cfg_table[CFGID_LINE_AUTHNAME].name,
        prot_cfg_table[CFGID_LINE_PASSWORD].name,
        prot_cfg_table[CFGID_LINE_DISPLAYNAME].name,
        prot_cfg_table[CFGID_LINE_CONTACT].name);

    for (i=0; i< MAX_CONFIG_LINES; i++) {
      config_get_value(CFGID_LINE_FEATURE+i, &feat, sizeof(feat));
      if ( feat != CC_FEATURE_NONE ){
        debugif_printf("%3s ", get_printable_cfg(CFGID_LINE_INDEX+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%4s ", get_printable_cfg(CFGID_LINE_FEATURE+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%12s ", get_printable_cfg(CFGID_PROXY_ADDRESS+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%s ", get_printable_cfg(CFGID_PROXY_PORT+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%3s ", get_printable_cfg(CFGID_LINE_CALL_WAITING+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%6s ", get_printable_cfg(CFGID_LINE_MSG_WAITING_LAMP+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%6s ", get_printable_cfg(CFGID_LINE_MESSAGE_WAITING_AMWI+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%6s ", get_printable_cfg(CFGID_LINE_RING_SETTING_IDLE+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%6s ", get_printable_cfg(CFGID_LINE_RING_SETTING_ACTIVE+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("     %s ", get_printable_cfg(CFGID_LINE_NAME+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%s ", get_printable_cfg(CFGID_LINE_AUTOANSWER_ENABLED+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%s ", get_printable_cfg(CFGID_LINE_AUTOANSWER_MODE+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%s ", get_printable_cfg(CFGID_LINE_AUTHNAME+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%s ", get_printable_cfg(CFGID_LINE_PASSWORD+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%s ", get_printable_cfg(CFGID_LINE_DISPLAYNAME+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
        debugif_printf("%s\n", get_printable_cfg(CFGID_LINE_CONTACT+i, buf, MAX_CONFIG_VAL_PRINT_LEN));
      }
    }

    return (0);
}


/**********************************************
 *  Line-Based Config API
 **********************************************/

/*
 *  Function: config_get_line_id()
 *
 *  Description: Given the line and the line-specific ID, this function
 *               will return the actual ID used to access the value in the
 *               config table.
 *
 *  Parameters: id   - The id config value to get
 *              line - The line that the ID is associated with
 *
 *  Returns: TRUE if the entry is found
 *           FALSE otherwise.
 */
static int
config_get_line_id (int id, int line)
{
    int line_id = 0;
    const var_t *entry;

    if ((line == 0) || (line > MAX_REG_LINES)) {
        entry = &prot_cfg_table[id];  // XXX set but not used
        (void) entry;
        CONFIG_ERROR(CFG_F_PREFIX"ID=%d- line %d out of range", "config_get_line_id", id, line);
        return (0);
    }
    line_id = id + line - 1;

    return (line_id);
}


/*
 *  Function: config_get_line_string()
 *
 *  Description: Get any arbitrary line config entry as a string
 *
 *  Parameters: id         - The id of the config string to get
 *              buffer     - Empty buffer where string will be copied
 *              line       - The line that the ID is associated with
 *              buffer_len - length of the output buffer
 *
 *  Returns: None
 */
void
config_get_line_string (int id, char *buffer, int line, int buffer_len)
{
    int line_id = 0;

    line_id = config_get_line_id(id, line);
    if (line_id) {
        config_get_string(line_id, buffer, buffer_len);
    }
}


/*
 *  Function: config_set_line_string()
 *
 *  Description: Set any arbitrary line config entry as a string
 *
 *  Parameters: id     - The id of the config string to set
 *              buffer - The new value for the string
 *              line   - The line that the ID is associated with
 *
 *  Returns: None
 */
void
config_set_line_string (int id, char *buffer, int line)
{
    int line_id = 0;

    line_id = config_get_line_id(id, line);
    if (line_id) {
        config_set_string(line_id, buffer);
    }
}

/*
 *  Function: config_get_line_value()
 *
 *  Parameters: id - The id of the config value to get
 *              *buffer - Empty buffer where value will be copied
 *              length - The number of bytes to get
 *              line - The line that the ID is associated with
 *
 *  Description: Get any arbitrary line config entry as a raw data value.
 *    If the length doesn't match the actual length of the field,
 *    nothing will be copied.
 *
 *  Returns: None
 */
void
config_get_line_value (int id, void *buffer, int length, int line)
{
    int line_id = 0;

    line_id = config_get_line_id(id, line);
    if (line_id) {
        config_get_value(line_id, buffer, length);
    }
}

/*
 *  Function: config_set_line_value()
 *
 *  Description: Set arbitrary config entry as a raw data value.
 *    If the length doesn't match the actual length of the field,
 *    nothing will be copied.
 *
 *  Parameters: id     - The id of the config value to set
 *              buffer - The new value to be set
 *              length - The number of bytes to set
 *              line   - The line that the ID is associated with
 *
 *  Returns: None
 */
void
config_set_line_value (int id, void *buffer, int length, int line)
{
    int line_id = 0;

    line_id = config_get_line_id(id, line);
    if (line_id) {
        config_set_value(line_id, buffer, length);
    }
}

/*
 *  Function: config_init()
 *
 *  Description: Initialize the Config Debug command
 *
 *  Parameters:  none
 *
 *  Returns:     none
 *
 */
void
config_init (void)
{
    /* Place holder for future init related actions */
}
