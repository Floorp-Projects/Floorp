/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "strset.h"
#include <malloc.h>
#include <string.h>

StrSet::StrSet()
{
    strings = 0;
    numstrings = 0;
}

void StrSet::add(const char* s)
{
    if (strings) {
	strings = (char**) realloc(strings, (numstrings + 1) * sizeof(char*));
    } else {
	strings = (char**) malloc(sizeof(char*));
    }
    strings[numstrings] = strdup(s);
    numstrings++;
}

int StrSet::contains(const char* s)
{
    char** sp = strings;
    int i = numstrings;

    while (--i >= 0) {
	char *ss = *sp++;
	if (ss[0] == s[0]) {
	    if (strcmp(ss, s) == 0) {
		return 1;
	    }
	}
    }
    return 0;
}
