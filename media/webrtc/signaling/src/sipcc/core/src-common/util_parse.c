/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
