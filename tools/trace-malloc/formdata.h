/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is formdata.h code, released
 * May 9, 2002.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Garrett Arch Blythe, 09-May-2002
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

#if !defined(__formdata_H__)
#define __formdata_H__

/*
**  formdata.h
**
**  Play (quick and dirty) utility API to parse up form get data into
**      name value pairs.
*/

typedef struct __struct_FormData
/*
**  Structure to hold the breakdown of any form data.
**
**  mNArray     Each form datum is a name value pair.
**              This array holds the names.
**              You can find the corresponding value at the same index in
**                  mVArray.
**              Never NULL, but perhpas an empty string.
**  mVArray     Each form datum is a name value pair.
**              This array holds the values.
**              You can find the corresponding name at the same index in
**                  mNArray.
**              Never NULL, but perhpas an empty string.
**  mNVCount    Count of array items in both mNArray and mVArray.
**  mStorage    Should be ignored by users of this API.
**              In reality holds the duped and decoded form data.
*/
{
    char** mNArray;
    char** mVArray;
    unsigned mNVCount;
    char* mStorage;
}
FormData;

FormData* FormData_Create(const char* inFormData)
/*
**  Take a contiguous string of form data, possibly hex encoded, and return
**      the name value pairs parsed up and decoded.
**  A caller of this routine should call FormData_Destroy at some point.
**
**  inFormData          The form data to parse up and decode.
**  returns FormData*   The result of our effort.
**                      This should be passed to FormData_Destroy at some
**                          point of the memory will be leaked.
*/
;

void FormData_Destroy(FormData* inDestroy)
/*
**  Release to the heap the structure previously created via FormData_Create.
**
**  inDestroy   The object to free off.
*/
;

#endif /* __formdata_H__ */
