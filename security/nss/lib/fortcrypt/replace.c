/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
# include <stdio.h>
# include <string.h>

int main(int argc, char* argv[]) {
    FILE *templ;
    FILE *target;
    unsigned char buffer[81];
    unsigned char *find, *replace;
    int matchcount = 0;
    int ch;
    int len;

    buffer[0] = '\0';

    if (argc != 5) {
        fprintf(stderr, "usuage: replace template.js searchstring replacestring target.js \n");
        return 1;
    }

    templ = fopen(argv[1], "r");
    if (!templ) {
        fprintf(stderr, "Cannot open template script %s\n", argv[1]);
        return 2;
    }

    find = (unsigned char*) argv[2];
    replace = (unsigned char*) argv[3];

    target = fopen(argv[4], "w");
    if (!target) {
        fclose(templ);
        fprintf(stderr, "Cannot write to target script %s\n", argv[4]);
        return 3;
    }

    for (len = 0; find[len]!='\0'; len++);

    if (len > 80) {
        fprintf(stderr, "length of searchstring exceeds 80 chars");
        return 4;
    }

    /* get a char from templ */
    while ((int)(ch=fgetc(templ)) != EOF) {
        if ((unsigned char)ch == find[matchcount]) {
            /* if it matches find[matchcount], 
             * then store one more char in buffer,
             * increase match count, and checks if 
             * the whole word has been found */
            buffer[matchcount] = (unsigned char) ch;
            buffer[++matchcount] = '\0';
            
            if (matchcount == len) {
                matchcount = 0;
                fprintf(target, "%s", replace);
            } 
        } else {
            /* reset matchcount, flush buffer */
            if (matchcount > 0) {
                fprintf(target, "%s", buffer);
                matchcount = 0;
            }
            fputc(ch, target);
        }
    }
    fclose(templ);
    fclose(target);
    return 0;
}
