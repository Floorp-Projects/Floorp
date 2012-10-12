/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
