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

#include "cpr_types.h"
#include "cpr_string.h"
#include "xml_defs.h"

XMLToken
parse_xml_tokens (char **parseptr, char *value, int maxlen)
{
    char *input = *parseptr;

    for (;;) {
        if (*input == '\0') {
            return TOK_EOF;
        } else if (isspace(*input)) {
            input++;
        } else if (input[0] == '<') {
            input++;
            if (input[0] == '/') {
                *parseptr = input + 1;
                return TOK_ENDLBRACKET;
            }
            /* A starting bracket */
            if (input[0] != '!') {
                *parseptr = input;
                return TOK_LBRACKET;
            }
            /* find closing bracket */
            while ((*input != '\0') && (*input != '>')) {
                input++;
            }
            /* Throw away the closing bracket for the comment */
            if (input[0] == '>') {
                input++;
            }
        } else if ((input[0] == '/') && (input[1] == '>')) {
            *parseptr = input + 2;
            return TOK_EMPTYBRACKET;
        } else if (input[0] == '>') {
            *parseptr = input + 1;
            return TOK_RBRACKET;
        } else if (input[0] == '=') {
            *parseptr = input + 1;
            return TOK_EQ;
        } else if (input[0] == '"') {
            char *endquote;
            int len;

            input++;
            endquote = strchr(input, '"');
            if (endquote == NULL) {
                *parseptr = input + strlen(input);
                return TOK_EOF;
            }
            len = endquote - input;
            if (len >= maxlen) {
                len = maxlen - 1;
            }
            memcpy(value, input, len);
            value[len] = 0;
            *parseptr = endquote + 1;
            return TOK_STR;
        } else if (isalnum(input[0]) || (input[0] == '{')) {
            char *endtok = input + 1;
            int len;

            while (isalnum(endtok[0]) || (endtok[0] == '-')
                   || (endtok[0] == '}')) {
                endtok++;
            }
            len = endtok - input;
            if (len >= maxlen) {
                len = maxlen - 1;
            }
            memcpy(value, input, len);
            value[len] = 0;
            *parseptr = endtok;
            return TOK_KEYWORD;
        } else {
            *parseptr = input + 1;
            return TOK_ERR;
        }
    }
}
