/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
 * fmi.h (FontMatchInfoObject.h)
 *
 * C++ implementation of the (fmi) FontMatchInfoObject
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _fmi_H_
#define _fmi_H_

#include "libfont.h"
#include "nf.h"

#include "Mnffmi.h"

#include "wfList.h"

#define FMI_FONTPART_DEFAULT		'*'
#define FMI_FONTPART_SEPERATOR		'-'
#define FMI_FONTPART_ESCAPE			'\\'
#define FMI_FONTPART_ATTRSEPERATOR	':'

#define FMI_TYPE_CSTRING	1
#define FMI_TYPE_JINT		2

struct fmi_attr_store {
  const char *attr;
  int valueType;
  union {
	void *genericValue;
	const char *stringValue;
	jint intValue;
  } u;
};

class FontMatchInfoObject : public wfList {
protected:
  // A string representation of this FMI
  const char *stringRepresentation;
  int stringLen;
  int stringMaxLen;

  int addAttribute(const char *attr, const char *value);
  int addAttribute(const char *attr, jint value);

  // These are used to keep the stringRepresentaion insync with the attributes.
  int addToString(const char *&str, int &strLen, int &strMaxLen, struct fmi_attr_store *ele);
  int addToString(const char *&str, int &strLen, int &strMaxLen, const char *value);
  int addToString(const char *&str, int &strLen, int &strMaxLen, const char value);
  const char *scanFontpart(const char *str, char *fontpart, const char *&attr, const char *&value);
  int releaseStringRepresentation(void);


public:
  FontMatchInfoObject(const char *name, const char *charset,
					  const char *encoding, jint weight, jint pitch,
					  jint style, jint underline, jint strikeOut,
					  jint resolutionX, jint resolutionY);
  FontMatchInfoObject(const char *reconstructString);
  ~FontMatchInfoObject();
  
  void *GetValue(const char *attr);
  const char ** ListAttributes();

  // Checks if this fmi is equivalent to the passed in fmi
  // Comparison takes care of dont cares
  jint IsEquivalent(struct nffmi *fmi);

  // Check if two FontMatchInfoObjects are EQUAL. (All values including
  // wildcards should match)
  int isEqual(FontMatchInfoObject *fob);

  const char *describe(void);
  int reconstruct(const char *describeString);

  friend void free_fmi_attr_store (wfList *object, void *item);
};

#endif /* _fmi_H_ */
