// The contents of this file are subject to the Mozilla Public License
// Version 1.1 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License
// at http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
// the License for the specific language governing rights and
// limitations under the License.
//
// The Initial Developer of the Original Code is Kipp E.B. Hickman.

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
