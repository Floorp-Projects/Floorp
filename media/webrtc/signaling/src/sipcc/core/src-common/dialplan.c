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

#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "xml_defs.h"
#include "logger.h"
#include "logmsg.h"
#include "util_parse.h"
#include "debug.h"
#include "phone_types.h"
#include "regmgrapi.h"


#include "upgrade.h"
#include "dialplan.h"

extern char DirectoryBuffer[DIALPLAN_MAX_SIZE];

/*
 * Tones with Bellcore- are Bellcore defined tones.
 * Tones with Cisco- are our tones.
 * The order of these names MUST match the order of
 * the vcm_tones_t type in vcm.h
 */
const char *tone_names[] = {
    "Bellcore-Inside",
    "Bellcore-Outside",
    "Bellcore-Busy",
    "Bellcore-Alerting",
    "Bellcore-BusyVerify",
    "Bellcore-Stutter",
    "Bellcore-MsgWaiting",
    "Bellcore-Reorder",
    "Bellcore-CallWaiting",
    "Bellcore-Cw2",
    "Bellcore-Cw3",
    "Bellcore-Cw4",
    "Bellcore-Hold",
    "Bellcore-Confirmation",
    "Bellcore-Permanent",
    "Bellcore-Reminder",
    "Bellcore-None",
    "Cisco-ZipZip",
    "Cisco-Zip",
    "Cisco-BeepBonk"
};

static struct DialTemplate *basetemplate;
char DialTemplateFile[MAX_TEMPLATE_LENGTH];
char g_dp_version_stamp[MAX_DP_VERSION_STAMP_LEN];

/*
 *  Function: addbytes()
 *
 *  Parameters:
 *
 *  Description:
 *
 *  Returns: None
 */
static void
addbytes (char **output, int *outlen, const char *input, int inlen)
{
    char *target = *output;

    if (inlen == -1) {
        inlen = strlen(input);
    }
    if (inlen >= *outlen) {
        inlen = *outlen - 1;
    }
    memcpy(target, input, inlen);
    target += inlen;
    *outlen += inlen;
    *output = target;
    /*
     * Null terminate the result
     */
    *target = '\0';
}

/*
 *  Function: poundDialingEnabled()
 *
 *  Parameters: None
 *
 *  Description: Determines if '#' is treated as a "dial now" character
 *
 *  Returns: TRUE if # is treated as end of dial signal
 *           FALSE if # is treated as a dialed digit
 */
static boolean
poundDialingEnabled (void)
{
    if (sip_regmgr_get_cc_mode(1) == REG_MODE_NON_CCM) {
        /*
         * Operating in Peer-to-Peer SIP mode, allow # dialing
         */
        return (TRUE);
    } else {
        return (FALSE);
    }
}

/*
 *  Function: isDialedDigit()
 *
 *  Parameters: input - single char
 *
 *  Description: Determine if the char is 0-9 or +, *
 *               # is matched here unless it is set to
 *               be used as "dial immediately"
 *
 *  Returns: DialMatchAction
 */
boolean
isDialedDigit (char input)
{
    boolean result = FALSE;

    if (!isdigit(input)) {
        if ((input == '*') || (input == '+') || ((input == '#') && (!poundDialingEnabled()))) {
            result = TRUE;
        }
    } else {
        result = TRUE;
    }

    return (result);
}

/*
 *  Function: MatchLineNumber()
 *
 *  Parameters: templateLine - line number specified in Dial Plan
 *              line         - line number to match
 *
 *  Description: The template line numbers are initialized to zero.
 *               Zero means that all lines match. If the Line parm
 *               is specified in the Dial Plan template, then its
 *               value must match the specified line.
 *^M
 *  Returns: boolean
 */
static boolean
MatchLineNumber (const line_t templateLine, const line_t line)
{
    /* Zero in the template matches any line. */
    return (boolean) ((templateLine == line) || (templateLine == 0));
}

/*
 *  Function: MatchDialTemplate()
 *
 *  Parameters: pattern - pattern string to match
 *              line    - line number to match
 *                        (May be 0 to match all lines)
 *              timeout - returned dial timeout in seconds
 *                        (May be NULL to not get a timeout)
 *              rewrite - buffer to hold rewritten string
 *                        (May be NULL for no rewrite)
 *              rewritelen - Bytes available in the buffer to write to
 *              routemode - pointer to location to hold route mode returned
 *                          (May be NULL to not get a routemode)
 *              tone - pointer to location to hold tone returned
 *
 *  Description: Find the best template to match a pattern
 *
 *  Returns: DialMatchAction
 */
DialMatchAction
MatchDialTemplate (const char *pattern,
                   const line_t line,
                   int *timeout,
                   char *rewrite,
                   int rewritelen,
                   RouteMode *pRouteMode,
                   vcm_tones_t *pTone)
{
    DialMatchAction result = DIAL_NOMATCH;
    struct DialTemplate *ptempl = basetemplate;
    struct DialTemplate *pbestmatch = NULL;
    boolean bestmatch_dialnow = FALSE;
    int best_comma_count = 0;
    DialMatchAction partialmatch_type = DIAL_NOMATCH;
    boolean partialmatch = FALSE;

    int matchlen = 0;
    int partialmatchlen = 0;
    int givedialtone = 0;
    int comma_counter = 0;

    /*
     * We need to provide a default rewrite string in case we do not have any template matches.
     * This happens when there is no template match (such as no * pattern) and when they have
     * no dial plan file at all
     */
    if (rewrite != NULL) {
        char *output = rewrite;
        int room = rewritelen;

        addbytes(&output, &room, pattern, -1);
    }

    /*
     * If the dialplan is empty, check to see if a # is in the pattern
     * and return DIAL_IMMEDIATELY. If not go ahead and return
     * DIAL_NOMATCH since there will be no template match in
     * an empty dialplan.
     */
    if (ptempl == NULL) {
        if (strchr(pattern, '#') && (poundDialingEnabled())) {
            return DIAL_IMMEDIATELY;
        } else {
            return DIAL_NOMATCH;
        }
    }

    /*
     * Iterate through all the templates. Skip the this template if it's not
     * for this line or all lines.
     */
    while (ptempl != NULL) {
        if (MatchLineNumber(ptempl->line, line)) {
            char *pinput = (char *) pattern;
            char *pmatch = ptempl->pattern;
            int thismatchlen = 0;
            DialMatchAction thismatch = DIAL_FULLMATCH;
            char *subs[MAX_SUBTITUTIONS];
            int subslen[MAX_SUBTITUTIONS];
            int subscount = -1;
            boolean dialnow = FALSE;

            while (*pinput) {
                int idx;

                /* Since the code below combines multiple ,
                 * in a row into "one" , only increment
                 * comma counter once instead of once
                 * for each comma combined.
                 */
                if (pmatch[0] == ',') {
                    comma_counter++;
                }

                /*
                 * Skip over any dial tone characters
                 */
                while (pmatch[0] == ',') {
                    pmatch++;
                }
                /*
                 * If this is a pattern character, we need to capture the digits for the
                 * substitution strings
                 */
                if (((pmatch[0] == '.') && isDialedDigit(pinput[0])) ||
                     (pmatch[0] == '*')) {
                    /*
                     * Get the next index in the substitution array (if any)
                     * Note that if they have more pattern sections in the pattern string
                     * the last one will get the counts for the characters.  So for example
                     * with the MAX_SUBSTITUTIONS of 5 and a pattern of
                     *  1.2.3.4.5.6.7.
                     * and an input string of
                     *  1a2b3c4d5e6f7g
                     * the arrays of substitions would come out as follows:
                     *   %0 = 1a2b3c4d5e6f7g  (as expected)
                     *   %1 = a               (as expected)
                     *   %2 = b               (as expected)
                     *   %3 = c               (as expected)
                     *   %4 = d               (as expected)
                     *   %5 = e6f    NOT what they really wanted, but predictable from the algorithm
                     */
                    if (subscount < (MAX_SUBTITUTIONS - 1)) {
                        subscount++;
                        subs[subscount] = pinput;
                        subslen[subscount] = 1;
                    }
                    if (pmatch[0] == '.') {
                        thismatch = DIAL_FULLPATTERN;
                        /*
                         * . in the pattern will match anything but doesn't contribute
                         * to our matching length for finding the best template
                         */
                        while (isdigit(pinput[1]) && (pmatch[1] == '.')) {
                            pinput++;
                            pmatch++;
                            subslen[subscount]++;
                        }
                    } else {
                        thismatch = DIAL_WILDPATTERN;
                        /*
                         * '*' is a wild card to match 1 or more characters
                         * Do a hungry first match
                         *
                         * Note: If match is currently pointing to an escape character,
                         *       we need to go past it to match the actual character.
                         */
                        if (pmatch[1] == DIAL_ESCAPE) {
                            idx = 2;
                        } else {
                            idx = 1;
                        }

                        /*
                         * The '*' will not match the '#' character since its' default use
                         * causes the phone to "dial immediately"
                         */
                        if ((pinput[0] == '#') && (poundDialingEnabled())) {
                            dialnow = TRUE;
                        } else {
                            while ((pinput[1] != '\0') &&
                                   (pinput[1] != pmatch[idx])) {
                                /*
                                 * If '#' is found and pound dialing is enabled, break out of the loop.
                                 */
                                if ((pinput[1] == '#') &&
                                    (poundDialingEnabled())) {
                                    break;
                                }
                                pinput++;
                                subslen[subscount]++;
                            }
                        }
                    }
                    /*
                     * Any other character must match exactly
                     */
                } else {
                    /*
                     * Look for the Escape character '\' and remove it
                     * Right now, the only character that needs to be escaped is '*'
                     * Note that we treat \ at the end of a line as a non-special
                     * character
                     */
                    if ((pmatch[0] == DIAL_ESCAPE) && (pmatch[1] != '\0')) {
                        pmatch++;
                    }

                    if (pmatch[0] != pinput[0]) {
                        /*
                         * We found a '#' that doesn't match the current match template
                         * This means that the '#" should be interpreted as the dial
                         * termination character.
                         */
                        if ((pinput[0] == '#') && (poundDialingEnabled())) {
                            dialnow = TRUE;
                            break;
                        }
                        /*
                         * No match, so abandon with no pattern to select
                         */
                        thismatchlen = -1;
                        thismatch = DIAL_NOMATCH;
                        break;

                    } else {
                        /*
                         * We matched one character, count it for the overall template match
                         */
                        thismatchlen++;
                    }
                }
                pmatch++;
                pinput++;
            }

            /*
             * *pinput = NULL means we matched everything that
             * was dialed in this template
             * Partial never matches * rules
             * Since 97. should have precendence over 9..
             * also check matchlen.
             * Since fullmatches (exact digits) have precendence
             * over fullpattern (.) check matchtype
             */
            if ((*pinput == NUL) || (dialnow)) {
                if ((thismatchlen > partialmatchlen) ||
                    ((thismatchlen == partialmatchlen) &&
                     (thismatch > partialmatch_type))) {
                    partialmatch_type = thismatch;
                    partialmatchlen = thismatchlen;
                    pbestmatch = ptempl;
                    partialmatch = TRUE;
                    bestmatch_dialnow = dialnow;
                    best_comma_count = comma_counter;
                    result = DIAL_NOMATCH;
                }
            }

            /*
             * If we exhausted the match string, then the template is a perfect match
             * However, we don't want to take this as the best template unless it matched
             * more digits than any other pattern.  For example if we have a pattern of
             *   9011*
             *   9.11
             *  We would want 9011 to match against the first one even though it is not complete
             *
             * We also have to be careful that a pattern such as
             *   *
             * does not beat something like
             *   9.......
             * when you have 94694210
             */
            if (pmatch[0] == '\0') {
                /*
                 * If this pattern is better, we want to adopt it
                 */
                if ((thismatchlen > matchlen) ||
                    ((thismatchlen == matchlen) && (thismatch > result)) ||
                    ((thismatch == DIAL_WILDPATTERN) &&
                     ((result == DIAL_NOMATCH) && (partialmatch == FALSE)))) {
                    /*
                     * this is a better match than what we found before
                     */
                    pbestmatch = ptempl;
                    bestmatch_dialnow = dialnow;
                    matchlen = thismatchlen;
                    result = thismatch;
                    /*
                     * Generate a rewrite string
                     *
                     *  <TEMPLATE MATCH="31."     Timeout="0" User="Phone" Rewrite="11111111%s"/>
                     *      310 -> 111111110
                     *  <TEMPLATE MATCH="32.."    Timeout="0" User="Phone" Rewrite="122222232.."/>
                     *      3255 -> 12222223255
                     *  <TEMPLATE MATCH="33..."   Timeout="0" User="Phone" Rewrite="13333333%1"/>
                     *  <TEMPLATE MATCH="34..1.." Timeout="0" User="Phone" Rewrite="14444%155%2"/>
                     *  <TEMPLATE MATCH="34..2.." Timeout="0" User="Phone" Rewrite="1444%s"/>
                     *  <TEMPLATE MATCH="34..3.." Timeout="0" User="Phone" Rewrite="11..11.."/>
                     */
                    if (rewrite != NULL) {
                        int dotindex = -1;
                        int dotsleft = 0;
                        char *output = rewrite;
                        int room = rewritelen;
                        char *prewrite = pbestmatch->rewrite;

                        if ((prewrite == NULL) || (prewrite[0] == '\0')) {
                            /*
                             * Null or empty patterns produce the input as the result
                             */
                            addbytes(&output, &room, pattern, -1);
                        } else {
                            while (prewrite[0] != '\0') {
                                if (prewrite[0] == '.') {
                                    /*
                                     * For a dot, we copy over the single previously matched character
                                     */
                                    while ((dotsleft == 0) &&
                                           (dotindex < subscount)) {
                                        dotindex++;
                                        dotsleft = subslen[dotindex];
                                    }
                                    if (dotsleft > 0) {
                                        addbytes(&output, &room,
                                                 subs[dotindex] +
                                                 subslen[dotindex] - dotsleft,
                                                 1);
                                        dotsleft--;
                                    }
                                } else if (prewrite[0] == '%') {
                                    int idx = prewrite[1] - '1';

                                    prewrite++; // Consume the %
                                    if ((prewrite[0] == 's') ||
                                        (prewrite[0] == '0')) {
                                        /*
                                         * %0 or %s means copy the entire input string
                                         */
                                        addbytes(&output, &room, pattern, -1);
                                    } else if ((idx >= 0) &&
                                               (idx <= subscount)) {
                                        /*
                                         * %n where n is withing the range of substitution patterns copies over
                                         * all of the characters that matched that group
                                         */
                                        addbytes(&output, &room, subs[idx],
                                                 subslen[idx]);
                                    } else if (prewrite[0]) {
                                        /*
                                         * %anything copies over anything.  This is how you would get a
                                         * % in the rewrite string
                                         */
                                        addbytes(&output, &room, prewrite, 1);
                                    }
                                } else {
                                    /*
                                     * Just a normal character, just copy it over
                                     */
                                    addbytes(&output, &room, prewrite, 1);
                                }
                                /*
                                 * Consume the character we used (if any)
                                 */
                                if (prewrite[0]) {
                                    prewrite++;
                                }
                            }
                        }
                        /*
                         * Put on the usermode (if specified)
                         */
                        switch (pbestmatch->userMode) {
                        case UserPhone:
                            if (*(output - 1) == '>') {
                                --room;
                                --output;
                                addbytes(&output, &room, ";user=phone>", -1);
                            } else {
                                addbytes(&output, &room, ";user=phone", -1);
                            }
                            break;
                        case UserIP:
                            if (*(output - 1) == '>') {
                                --room;
                                --output;
                                addbytes(&output, &room, ";user=ip>", -1);
                            } else {
                                addbytes(&output, &room, ";user=ip", -1);
                            }
                            break;
                        default:
                            break;
                        }
                    }
                }
            } else if (pmatch[0] == ',') {
                givedialtone = 1;
            }

            /*
             * Even if this pattern was not taken as a full match, remember the longest length
             */
            if (thismatchlen > matchlen) {
                matchlen = thismatchlen;
            }
        }
        /*
         * Try the next template
         */
        ptempl = ptempl->next;
        comma_counter = 0;
    }
    /*
     * Did we get any templates at all?
     */
    switch (result) {
    case DIAL_FULLPATTERN:
    case DIAL_FULLMATCH:
        givedialtone = 0;
        /* FALLTHROUGH */
    case DIAL_WILDPATTERN:
        if (timeout != NULL) {
            *timeout = pbestmatch->timeout;
        }
        if (pRouteMode != NULL) {
            *pRouteMode = pbestmatch->routeMode;
        }
        break;

    default:
        /*
         * If we have received a partial match, set the timeout
         * to be the default timeout (i.e. interdigit timeout)
         * to allow for additional digit collection.
         */
        if (partialmatch) {
            if ((timeout != NULL) && (*timeout == 0)) {
                *timeout = DIAL_TIMEOUT;
            }
        } else if ((*(pattern + strlen(pattern) - 1) == '#') &&
                   (poundDialingEnabled())) {
            /*
             * No template was matched, however if the dialplan
             * does not have the default '*' rule and the pattern
             * dialed did not match any other template this code
             * would be hit. Therefore check for a # to see if
             * we need to set the dial immediately flag.
             */
            result = DIAL_IMMEDIATELY;
        }
        break;
    }

    /*
     * If the bestmatch template says to dialnow, do it.
     */
    if (bestmatch_dialnow) {
        /*
         * If there is partial match, and dialled pattern has "#",
         * then do not Dial immediately. Give user a chance to edit
         * the mistake if any.
         */
        if (!((poundDialingEnabled()) && (strchr(pattern, '#')) &&
              partialmatch)) {
            result = DIAL_IMMEDIATELY;
            if (timeout != NULL) {
                *timeout = 0;
            }
        }
    }

    if (givedialtone) {
        /*
         * Give user dial-tone. The tone given
         * is based on the rule. Default tone
         * is Outside-Dial-Tone.
         */
        if (pTone != NULL) {
            *pTone = VCM_DEFAULT_TONE;
            if (pbestmatch != NULL) {
                if (best_comma_count < pbestmatch->tones_defined) {
                    *pTone = pbestmatch->tone[best_comma_count];
                }
            }
        }
        result = DIAL_GIVETONE;
    }

    return result;   // Nothing special to match against
}


/*
 *  Function: InitDialPlan()
 *
 *  Parameters: wipe - should we wipe the current dialplan out before we start,
 *                     or leave it in place for safety's sake
 *
 *  Description: Reads the Dial Plan from memory and submits a TFTP
 *      request for the Dial Plan file.
 *
 *  Returns: None
 */
void
InitDialPlan (boolean wipe)
{
}



/*
 *  Function: FreeDialTemplates()
 *
 *  Parameters: None
 *
 *  Description: Frees the Dial Templates from memory
 *
 *  Returns: None
 */
void
FreeDialTemplates (void)
{
    struct DialTemplate *pnext;

    while (basetemplate != NULL) {
        pnext = basetemplate->next;
        cpr_free(basetemplate);
        basetemplate = pnext;
    }
}


/*
 *  Function: AddDialTemplate()
 *
 *  Parameters: pattern -
 *              line    -
 *              timeout -
 *              userMode -
 *              rewrite -
 *              routeMode -
 *              tone - array of tones to play when , is hit in the rule
 *              tones_defined - # of tones this rule actually defined
 *
 *  Description: Add a dial template to the known list of templates
 *
 *  Returns: None
 */
static void
AddDialTemplate (const char *pattern, const line_t line,
                 int timeout, UserMode userMode,
                 const char *rewrite, RouteMode routeMode,
                 vcm_tones_t tone[MAX_TONES], int tones_defined)
{
    struct DialTemplate *pnewtemplate;
    int patternlen = strlen(pattern);
    int counter;

    pnewtemplate = (struct DialTemplate *)
        cpr_malloc(sizeof(struct DialTemplate) + patternlen + strlen(rewrite) +
                   2);
    if (pnewtemplate != NULL) {
        pnewtemplate->next = NULL;
        pnewtemplate->pattern = (char *) (pnewtemplate + 1);
        strcpy(pnewtemplate->pattern, (char *) pattern);
        pnewtemplate->rewrite = pnewtemplate->pattern + patternlen + 1;
        strcpy(pnewtemplate->rewrite, (char *) rewrite);
        pnewtemplate->line = line;
        pnewtemplate->timeout = timeout;
        pnewtemplate->userMode = userMode;
        pnewtemplate->routeMode = routeMode;
        pnewtemplate->tones_defined = tones_defined;
        for (counter = 0; counter < MAX_TONES; counter++) {
            pnewtemplate->tone[counter] = tone[counter];
        }

        /*
         * Now add it to the end of all the templates
         */
        if (basetemplate == NULL) {
            basetemplate = pnewtemplate;
        } else {
            struct DialTemplate *base = basetemplate;

            while (base->next != NULL) {
                base = base->next;
            }
            base->next = pnewtemplate;
        }
    }
}

/*
 *  Function: show_dialplan_cmd
 *
 *  Parameters:   standard args
 *
 *  Description:  Display the current dialplan (if any)
 *
 *  Returns:
 *
 */
int32_t
show_dialplan_cmd (int32_t argc, const char *argv[])
{
    struct DialTemplate *pTemp;
    char umode[32], rmode[32];
    char line_str[32];
    uint32_t idx = 1;
    int32_t counter = 0;

    debugif_printf("Dialplan is....\n");
    debugif_printf("Dialplan version: %s\n", g_dp_version_stamp);

    pTemp = basetemplate;
    if (basetemplate == NULL) {
        debugif_printf("EMPTY\n");
        return 0;
    }
    while (pTemp != NULL) {
        switch (pTemp->routeMode) {
        case RouteEmergency:
            strcpy(rmode, "Emergency");
            break;
        case RouteFQDN:
            strcpy(rmode, "FQDN");
            break;
        default:
            strcpy(rmode, "Default");
            break;
        }

        switch (pTemp->userMode) {
        case UserPhone:
            strcpy(umode, "Phone");
            break;
        case UserIP:
            strcpy(umode, "IP");
            break;
        default:
            strcpy(umode, "Unspecified");
            break;
        }

        if (pTemp->line == 0) {
            sprintf(line_str, "All");
        } else {
            sprintf(line_str, "%d", pTemp->line);
        }
        debugif_printf("%02d. Pattern: %s  Rewrite: %s Line: %s\n"
                       "    Timeout: %04d   UserMode: %s  RouteMode: %s\n",
                       idx, pTemp->pattern, pTemp->rewrite, line_str,
                       pTemp->timeout, umode, rmode);
        for (counter = 0; counter < pTemp->tones_defined; counter++) {
            debugif_printf("    Tone %d: %s\n", counter + 1,
                           tone_names[(int) (pTemp->tone[counter])]);
        }
        pTemp = pTemp->next;
        idx++;

    }
    return (0);
}


/*
 *  Function: ParseDialEntry()
 *
 *  Parameters: parseptr - pointer to bytes to be parsed
 *
 *  Description: Parse the contents of a TEMPLATE tag and add the data to
 *      the dial template lists.
 *      The keywords parsed are:
 *           MATCH="string"
 *           Line="number"
 *           Timeout="number"
 *           User="Phone" | "IP"
 *           Rewrite="string"
 *      All other keywords are silently ignored
 *  The format of the TEMPLATE can be one ofthe following:
 *  1. <TEMPLATE KEYWORD1="value1" ... KEYWORDN="valueN" />
 *  2. <TEMPLATE KEYWORD1="value1" ... KEYWORDN="valueN"></TEMPLATE>
 *
 *  Returns: Int - The Number of errors encountered
 */
static int
ParseDialEntry (char **parseptr)
{
    char dialtemplate[MAX_TEMPLATE_LENGTH];
    char rewrite[MAX_TEMPLATE_LENGTH];
    int timeout = DIAL_TIMEOUT; /* Default to DIAL_TIMEOUT if none specified */
    unsigned char line = 0;     /* Default to 0 if not specified. Zero matches all lines. */
    int counter = 0;
    int tone_counter = 0;
    UserMode usermode = UserUnspec;
    RouteMode routeMode = RouteDefault;
    vcm_tones_t tone[MAX_TONES];
    ParseDialState state = STATE_ANY;

    dialtemplate[0] = '\0';
    rewrite[0] = '\0';
    // Set all tones to default. Rule may override.
    for (counter = 0; counter < MAX_TONES; counter++) {
        tone[counter] = VCM_DEFAULT_TONE;
    }

    for (;;) {
        char buffer[64];
        XMLToken tok;

        tok = parse_xml_tokens(parseptr, buffer, sizeof(buffer));
        switch (tok) {
        case TOK_KEYWORD:
            if (state != STATE_ANY) {
                if ((cpr_strcasecmp(buffer, "TEMPLATE") == 0) &&
                    (state == STATE_END_TAG_STARTED)) {
                    state = STATE_END_TAG_FOUND;
                } else {
                    return 1;
                }
            } else if (cpr_strcasecmp(buffer, "MATCH") == 0) {
                state = STATE_GOT_MATCH;
            } else if (cpr_strcasecmp(buffer, "LINE") == 0) {
                state = STATE_GOT_LINE;
            } else if (cpr_strcasecmp(buffer, "TIMEOUT") == 0) {
                state = STATE_GOT_TIMEOUT;
            } else if (cpr_strcasecmp(buffer, "USER") == 0) {
                state = STATE_GOT_USER;
            } else if (cpr_strcasecmp(buffer, "REWRITE") == 0) {
                state = STATE_GOT_REWRITE;
            } else if (cpr_strcasecmp(buffer, "ROUTE") == 0) {
                state = STATE_GOT_ROUTE;
            } else if (cpr_strcasecmp(buffer, "TONE") == 0) {
                state = STATE_GOT_TONE;
            } else {
                return 1;
            }
            break;

        case TOK_EQ:
            switch (state) {
            case STATE_GOT_MATCH:
                state = STATE_GOT_MATCH_EQ;
                break;
            case STATE_GOT_LINE:
                state = STATE_GOT_LINE_EQ;
                break;
            case STATE_GOT_TIMEOUT:
                state = STATE_GOT_TIMEOUT_EQ;
                break;
            case STATE_GOT_USER:
                state = STATE_GOT_USER_EQ;
                break;
            case STATE_GOT_REWRITE:
                state = STATE_GOT_REWRITE_EQ;
                break;
            case STATE_GOT_ROUTE:
                state = STATE_GOT_ROUTE_EQ;
                break;
            case STATE_GOT_TONE:
                state = STATE_GOT_TONE_EQ;
                break;
                
            default:
                return 1;
            }
            break;

        case TOK_STR:
            switch (state) {
            case STATE_GOT_MATCH_EQ:
                sstrncpy(dialtemplate, buffer, sizeof(dialtemplate));
                break;

            case STATE_GOT_LINE_EQ:
                line = (unsigned char) atoi(buffer);
                break;

            case STATE_GOT_TIMEOUT_EQ:
                timeout = atoi(buffer);

                if (timeout < 0) {
                    return 1;
                }
                break;

            case STATE_GOT_USER_EQ:
                if (cpr_strcasecmp(buffer, "PHONE") == 0) {
                    usermode = UserPhone;
                } else if (cpr_strcasecmp(buffer, "IP") == 0) {
                    usermode = UserIP;
                } else {
                    return 1;
                }
                break;

            case STATE_GOT_ROUTE_EQ:
                if (cpr_strcasecmp(buffer, "DEFAULT") == 0) {
                    routeMode = RouteDefault;
                } else if (cpr_strcasecmp(buffer, "EMERGENCY") == 0) {
                    routeMode = RouteEmergency;
                } else if (cpr_strcasecmp(buffer, "FQDN") == 0) {
                    routeMode = RouteFQDN;
                } else {
                    return 1;
                }
                break;

            case STATE_GOT_TONE_EQ:
                if (tone_counter < MAX_TONES) {
                    /* Tone = "" check */
                    if (*buffer == '\000') {
                        tone[tone_counter] = VCM_DEFAULT_TONE;
                    } else {
                        for (counter = 0; counter < VCM_MAX_DIALTONE; counter++) {
                            if (cpr_strcasecmp(buffer, tone_names[counter]) ==
                                0) {
                                tone[tone_counter] = (vcm_tones_t) counter;
                                break;
                            }
                        }
                        // Tone string not matched
                        if (counter == VCM_MAX_DIALTONE) {
                            return 1;
                        }
                    }
                    tone_counter++;
                    // Too many tones defined in the rule
                } else {
                    return 1;
                }
                break;

            case STATE_GOT_REWRITE_EQ:
                sstrncpy(rewrite, buffer, sizeof(rewrite));
                break;

            default:
                return 1;
            }
            state = STATE_ANY;
            break;

        case TOK_EMPTYBRACKET: /* "/>" */
            AddDialTemplate(dialtemplate, line, timeout, usermode, rewrite,
                            routeMode, tone, tone_counter);
            if (state == STATE_ANY) {
                return 0;
            }
            /*
             * There was a parsing error, so just ignore it
             */
            return 1;

        case TOK_RBRACKET:     /* ">" */
            if (state == STATE_ANY) {
                state = STATE_START_TAG_COMPLETED;
            } else if (state == STATE_END_TAG_FOUND) {
                AddDialTemplate(dialtemplate, line, timeout, usermode, rewrite,
                                routeMode, tone, tone_counter);
                return 0;
            } else {
                return 1;
            }
            break;

        case TOK_ENDLBRACKET:  /* "</" */
            if (state == STATE_START_TAG_COMPLETED) {
                state = STATE_END_TAG_STARTED;
            } else {
                return 1;
            }
            break;


        default:
            /*
             * Problem parsing. Let them know
             */
            return 1;
        }
    }
}

/*
 *  Function: ParseDialVersion()
 *
 *  Parameters: parseptr - pointer to bytes to be parsed
 *
 *  Description: Parse the contents of a versionStamp tag and store it
 *  The format of the versionStamp is as follows:
 *  <versionStamp>value up to 64 chars</versionStamp>
 *
 *  Returns: 0 - if the version stamp is successfully parsed.
 *           1 - if the version stamp  parsing fails.
 */
static int
ParseDialVersion (char **parseptr)
{
    ParseDialState state = STATE_ANY;
    char version_stamp[MAX_DP_VERSION_STAMP_LEN] = { 0 };
    int len;

    for (;;) {
        char buffer[MAX_DP_VERSION_STAMP_LEN];
        XMLToken tok;

        tok = parse_xml_tokens(parseptr, buffer, sizeof(buffer));
        switch (tok) {
        case TOK_KEYWORD:
            switch (state) {
            case STATE_START_TAG_COMPLETED:
                /*
                 * the keyword is version stamp value here. copy it temporarily.
                 */
                memcpy(version_stamp, buffer, MAX_DP_VERSION_STAMP_LEN);
                break;
            case STATE_END_TAG_STARTED:
                if (cpr_strcasecmp(buffer, "versionStamp") != 0) {
                    return 1;
                }
                state = STATE_END_TAG_FOUND;
                break;
                
            default:
                break;
            }
            break;

        case TOK_RBRACKET:     /* ">" */
            if (state == STATE_ANY) {
                state = STATE_START_TAG_COMPLETED;
            } else if (state == STATE_END_TAG_FOUND) {
                /* strip of the warping curly braces if they exist */
                len = strlen(version_stamp);
                if (len <= 2)
                {
                    CCAPP_ERROR("ParseDialVersion(): Version length [%d] is way too small", len);
                    return (1);                                
                }
                
                memset(g_dp_version_stamp, 0, MAX_DP_VERSION_STAMP_LEN);
                if ((version_stamp[0] == '{')
                    && (version_stamp[len - 1] == '}')) {
                    memcpy(g_dp_version_stamp, (version_stamp + 1), (len - 2));
                } else {
                    memcpy(g_dp_version_stamp, version_stamp, len);
                }
                return 0;
            } else {
                return 1;
            }
            break;

        case TOK_ENDLBRACKET:  /* "</" */
            if (state == STATE_START_TAG_COMPLETED) {
                state = STATE_END_TAG_STARTED;
            } else {
                return 1;
            }
            break;

        default:
            /*
             * Problem parsing. Let them know
             */
            return 1;
        }
    }
}


/*
 *  Function: ParseDialTemplate()
 *
 *  Parameters: parseptr - pointer to bytes to be parsed
 *
 *  Description: Parse the contents of a dial template XML file
 *      All Start and end tags are ignored
 *      The empty element <TEMPLATE is parsed by ParseDialEntry
 *
 *  Returns: false if parse fails else true
 */
boolean
ParseDialTemplate (char *parseptr)
{
    char buffer[MAX_TEMPLATE_LENGTH];
    XMLToken tok;
    int LookKey;
    int LookEndKey;
    int errors = 0;
    int insideDialPlan = 0;

    LookKey = 0;
    LookEndKey = 0;
    FreeDialTemplates();
    /*
     * reset the version stamp so that dialplans that do not contain versionStamp
     * tag will not keep the previous versionstamp.
     */
    g_dp_version_stamp[0] = 0;


    if (parseptr == NULL) {
        debugif_printf("ParseDialTempate(): parseptr=NULL Returning.\n");
        return (FALSE);
    }

    while ((tok =
            parse_xml_tokens(&parseptr, buffer, sizeof(buffer))) != TOK_EOF) {
        if (LookEndKey) {
            if (tok == TOK_RBRACKET) {
                LookEndKey = 0;
            } else if ((tok == TOK_KEYWORD)
                       && !cpr_strcasecmp(buffer, "DIALTEMPLATE")) {
                insideDialPlan = 0;
            }
        } else if (tok == TOK_LBRACKET) {
            LookKey = 1;
        } else if ((LookKey != 0) && (tok == TOK_KEYWORD)
                   && !cpr_strcasecmp(buffer, "DIALTEMPLATE")) {
            insideDialPlan = 1;
        } else if ((LookKey != 0) && (tok == TOK_KEYWORD)
                   && !strcmp(buffer, "TEMPLATE")) {
            if (insideDialPlan) {
                errors += ParseDialEntry(&parseptr);
            } else {
                errors++;
            }
        } else if ((LookKey != 0) && (tok == TOK_KEYWORD)
                   && !cpr_strcasecmp(buffer, "versionStamp")) {
            if (insideDialPlan) {
                /* <versionStamp> tag found. parse it */
                errors += ParseDialVersion(&parseptr);
            } else {
                errors++;
            }
        } else if (tok == TOK_ENDLBRACKET) {
            LookEndKey = 1;
        } else {
            LookKey = 0;
        }
    }
    /*
     * If we had any parse errors, put them into the log
     */
    log_clear(LOG_CFG_PARSE_DIAL);
    if (errors != 0) {
        log_msg(LOG_CFG_PARSE_DIAL, errors, DialTemplateFile);
        return (FALSE);
    }

    return (TRUE);
}

