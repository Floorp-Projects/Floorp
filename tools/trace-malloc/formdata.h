/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
