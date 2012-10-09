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

#ifndef DIALPLAN_H
#define DIALPLAN_H

#include <vcm.h>
#include "phone_types.h"

#define DIALPLAN_MAX_SIZE    0x2000
#define MAX_SUBTITUTIONS 5
#define MAX_TONES 3
#define MAX_TEMPLATE_LENGTH 196
#define DIAL_ESCAPE      '\\'
#define MAX_DIALSTRING   256
#define DIAL_TIMEOUT      10
#define MAX_DP_VERSION_STAMP_LEN   (64+1)
extern char g_dp_version_stamp[MAX_DP_VERSION_STAMP_LEN];

typedef enum {
    DIAL_NOMATCH = 0,
    DIAL_GIVETONE,
    DIAL_WILDPATTERN,
    DIAL_FULLPATTERN,
    DIAL_FULLMATCH,
    DIAL_IMMEDIATELY
} DialMatchAction;

/* Set enum values to match DialMatchAction */
typedef enum {
    DIALTONE_NOMATCH = 0,
    DIALTONE_WILD = 2,
    DIALTONE_FULL,
    DIALTONE_EXACT
} DialToneMatch;

typedef enum {
    UserUnspec,
    UserPhone,
    UserIP
} UserMode;

typedef enum {
    RouteDefault,       // Route using the default proxy
    RouteEmergency,     // Route using the emergency proxy
    RouteFQDN           // Route according to the FQDN in the entry
} RouteMode;

struct DialTemplate {
    struct DialTemplate *next;
    char *pattern;
    line_t line;
    char *rewrite;
    int timeout;
    UserMode userMode;
    RouteMode routeMode;
    int tones_defined;
    vcm_tones_t tone[MAX_TONES];
};

struct StoredDialTemplate {
    short size;                 // Size of header part of structure
    short nextOffset;           // total Number of bytes used in the entry
    //   A zero here is used as a last entry
    int timeout;
    line_t line;
    UserMode userMode;
    short pattern_offset;       // Offset to the pattern string
    short rewrite_offset;       // Offset to the rewrite string
    RouteMode routeMode;
    int tones_defined;
    vcm_tones_t tone[MAX_TONES];

};

typedef enum {
    STATE_ANY,
    STATE_GOT_MATCH,
    STATE_GOT_MATCH_EQ,
    STATE_GOT_LINE,
    STATE_GOT_LINE_EQ,
    STATE_GOT_TIMEOUT,
    STATE_GOT_TIMEOUT_EQ,
    STATE_GOT_USER,
    STATE_GOT_USER_EQ,
    STATE_GOT_REWRITE,
    STATE_GOT_REWRITE_EQ,
    STATE_GOT_ROUTE,
    STATE_GOT_ROUTE_EQ,
    STATE_GOT_TONE,
    STATE_GOT_TONE_EQ,
    STATE_START_TAG_COMPLETED, /* start tag parsing is complete when self-terminating (<tag/>) format is not used */
    STATE_END_TAG_STARTED,     /* end tag started when we see "</" */
    STATE_END_TAG_FOUND        /* end tag is parsed */
} ParseDialState;

extern DialMatchAction MatchDialTemplate(const char *pattern,
                                         const line_t line,
                                         int *timeout,
                                         char *rewrite, int rewritelen,
                                         RouteMode *pRouteMode,
                                         vcm_tones_t *pTone);
void SaveDialTemplate(void);
void RestoreDialPlan(void);
void InitDialPlan(boolean);
void FreeDialTemplates(void);
extern short handle_dialplan(short, void *);
extern boolean ParseDialTemplate(char *parseptr);

#endif //DIALPLAN_H
