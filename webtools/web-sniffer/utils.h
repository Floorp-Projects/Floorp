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
 * The Original Code is Web Sniffer.
 * 
 * The Initial Developer of the Original Code is Erik van der Poel.
 * Portions created by Erik van der Poel are
 * Copyright (C) 1998,1999,2000 Erik van der Poel.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#ifdef FREE
#undef FREE
#endif
#define FREE(p) do { if (p) { free(p); (p) = NULL; } } while (0)

unsigned char *appendString(const unsigned char *str1,
	const unsigned char *str2);
unsigned char *copySizedString(const unsigned char *str, int size);
unsigned char *copyString(const unsigned char *str);
unsigned char *lowerCase(unsigned char *buf);
void *utilRealloc(void *ptr, size_t oldSize, size_t newSize);

#endif /* _UTILS_H_ */
