/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifdef XP_UNIX
#define MAX_JDOUBLE jlong_MAXINT
#define MIN_JDOUBLE jlong_MININT
#else
#define MAX_JDOUBLE 0x7FEFFFFFFFFFFFFFL
#define MIN_JDOUBLE 0x1L
#endif

#define MAX_JINT 2147483647
#define MIN_JINT -2147483648
#define MAX_JBYTE 2
#define MIN_JBYTE -2
#define MAX_JFLOAT 1
#define MIN_JFLOAT 0
#define MAX_JLONG 1
#define MIN_JLONG 0
#define MAX_JSHORT 1
#define MIN_JSHORT 0
