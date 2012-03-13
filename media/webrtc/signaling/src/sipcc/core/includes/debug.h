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

#ifndef _DEBUG_INCLUDED_H  /* allows multiple inclusion */
#define _DEBUG_INCLUDED_H

#include "cpr_types.h"
#include "plat_api.h"
#include "phone_debug.h"
#include "CSFLog.h"

typedef cc_int32_t (*debug_callback)(cc_int32_t argc, const char *argv[]);
typedef cc_int32_t (*show_callback)(cc_int32_t argc, const char *argv[]);
typedef cc_int32_t (*clear_callback)(cc_int32_t argc, const char *argv[]);

typedef enum {
    TEST_OPEN,
    TEST_CLOSE,
    TEST_KEY,
    TEST_ONHOOK,
    TEST_OFFHOOK,
    TEST_SHOW,
    TEST_HIDE,
    TEST_PROFILE,
    TEST_C3PO
} test_command_t;


typedef int32_t (*test_callback)(int32_t argc, const char *argv[],
                                 test_command_t command);

extern int32_t TestMode;
extern int32_t TestShow;

typedef enum {
    DEBUG_ENTRY_TYPE_FLAG,
    DEBUG_ENTRY_TYPE_DEBUG_FUNC,
    DEBUG_ENTRY_TYPE_SHOW_FUNC
} debug_entry_type_e;

typedef struct {
    const char *keyw;
    union {
        int32_t *flag;
        debug_callback func;
        show_callback show_func;
    } u;
    debug_entry_type_e type;
    boolean show_tech;
} debug_entry_t;

typedef struct {
    const char *keyw;
    union {
        int32_t *flag;
        clear_callback func;
    } u;
} clear_entry_t;

typedef struct {
    const char *keyw;
    const char *abrv;
    const char *help;
    boolean hidden;
    union {
        int32_t *flag;
        test_callback func;
    } u;
    test_command_t command;
} test_entry_t;


typedef struct {
    unsigned char flag;
    unsigned char hookevent;
    unsigned char keyevent;     // keyevent and key will double as a timer value internally
    unsigned char key;
} testevent_t;

// The next 4 defines are used as flag values to specify the event types in the queue.
// Note that we cannot use the HOOKSCAN and KEYSCAN that are defined in phone.h, because
// HOOKSCAN requires a full integer. In order to minimize space in the test event queue
// we need short numbers.
#define TEST_NONE 0
#define TEST_KEYSCAN 1
#define TEST_HOOKSCAN 2
#define TEST_TIMER 0x80


#define MAX_DEBUG_NAME 50
#define MAX_SHOW_NAME 50
#define MAX_CLEAR_NAME 50


typedef struct {
    cc_debug_category_e category;
    char debugName[MAX_DEBUG_NAME];
    cc_int32_t *key; /* The variable associated with this debug category */
} debugStruct_t;

typedef struct {
    cc_debug_show_options_e category;
    char showName[MAX_SHOW_NAME];
    show_callback callbackFunc;  /* The callback function for the show cmd */
    boolean  showTech; /* TRUE indicates this show is also tied with tech cmd */
} debugShowStruct_t;

typedef struct {
    cc_debug_clear_options_e category;
    char clearName[MAX_CLEAR_NAME];
    clear_callback callbackFunc;  /* The callback function for the clear cmd */
} debugClearStruct_t;

/*
 * Prototypes for public functions
 */
void bind_test_keyword(const char *keyword, const char *abrv, boolean hidden,
                       test_callback func, test_command_t command,
                       const char *help);
testevent_t TESTGetEvent(void);

// Send debug output to CSFLog
#define debugif_printf(format, ...) CSFLogDebug("debugif", format, ## __VA_ARGS__ )


#endif /* _DEBUG_INCLUDED_H */
