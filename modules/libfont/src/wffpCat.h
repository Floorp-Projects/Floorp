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
 * wffpCat.h (FontDisplayerCatalogObject.h)
 *
 * This maintains the displayer catalog.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _wffpCat_H_
#define _wffpCat_H_

#include "libfont.h"
#include "nf.h"

#include "Mnfrc.h"
#include "Mnffmi.h"
#include "Mnffp.h"
#include "Mnffbu.h"

#include "wfList.h"

class FontCatalogFile {
	FILE *fp;
	int indentLevel;
	int writing;
public:
	FontCatalogFile(const char *filename, int write=0);
	~FontCatalogFile(void);

	// Anything less than 0 isn't good status.
	int status(void);

	// Read values from a catalog file
	char *readline(char *buf, int buflen);
	int isEof();

	// Print values to the catalog file
	int output(const char *name);
	int output(const char *name, int value);
	int output(const char *name, const char *str);

	int indentIn(void);
	int indentOut(void);
};

struct catalog_store {
	jint rcMajorType;
	jint rcMinorType;
	struct nffmi **fmis;
	int maxFmiCount;
	int fmiCount;
};

class FontDisplayerCatalogObject : public wfList {
private:
	int copyFmis(struct catalog_store *ele, struct nffmi **fmis);
	int addFmi(struct catalog_store *ele, struct nffmi *fmi);

public:
	FontDisplayerCatalogObject(const char *reconstructString = NULL);
	~FontDisplayerCatalogObject();
	int finalize(void);

	// Query if a particular fmi is supported by the catalog
	int supportsFmi(struct nfrc *rc, struct nffmi *fmi);

	// Update the catalog with a new list of fmi per rc
	// Takes an internal copy of the fmi array.
	int update(struct nfrc *rc, struct nffmi **fmi);
	int addFmi(jint rcMajorType, jint rcMinorType, struct nffmi *fmi);

	// Query if this catalog has been initialized for the rc
	int isInitialized(struct nfrc *rc);

	// For serailization
	int describe(FontCatalogFile &fc);
	int reconstruct(FontCatalogFile &fc);
};

#endif /* _wffpCat_H_ */
