/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
