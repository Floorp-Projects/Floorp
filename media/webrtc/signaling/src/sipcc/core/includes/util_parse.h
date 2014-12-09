/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _UTIL_PARSE_H_
#define _UTIL_PARSE_H_

#include "xml_defs.h"

XMLToken parse_xml_tokens(char **parseptr, char *value, int maxlen);

#endif
