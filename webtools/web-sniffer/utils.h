/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
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
 * The Original Code is SniffURI.
 *
 * The Initial Developer of the Original Code is
 * Erik van der Poel <erik@vanderpoel.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

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
