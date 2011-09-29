/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla PostScript driver printer list component.
 *
 * The Initial Developer of the Original Code is
 * Kenneth Herron <kherron+mozilla@fmailbox.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef nsCUPSShim_h___
#define nsCUPSShim_h___

#include "prtypes.h"

/* Various CUPS data types. We don't #include cups headers to avoid
 * requiring CUPS to be installed on the build host (and to avoid having
 * to test for CUPS in configure).
 */
typedef struct                          /**** Printer Options ****/
{
    char          *name;                  /* Name of option */
    char          *value;                 /* Value of option */
} cups_option_t;

typedef struct               /**** Destination ****/
{
    char          *name,       /* Printer or class name */
                  *instance;   /* Local instance name or NULL */
    int           is_default;  /* Is this printer the default? */
    int           num_options; /* Number of options */
    cups_option_t *options;    /* Options */
} cups_dest_t;

typedef cups_dest_t* (PR_CALLBACK *CupsGetDestType)(const char *printer,
                                                    const char *instance,
                                                    int num_dests, 
                                                    cups_dest_t *dests);
typedef int (PR_CALLBACK *CupsGetDestsType)(cups_dest_t **dests);
typedef int (PR_CALLBACK *CupsFreeDestsType)(int         num_dests,
                                             cups_dest_t *dests);
typedef int (PR_CALLBACK *CupsPrintFileType)(const char    *printer,
                                             const char    *filename,
                                             const char    *title,
                                             int           num_options,
                                             cups_option_t *options);
typedef int (PR_CALLBACK *CupsTempFdType)(char *filename,
                                          int   length);
typedef int (PR_CALLBACK *CupsAddOptionType)(const char    *name,
                                             const char    *value,
                                             int           num_options,
                                             cups_option_t **options);

struct PRLibrary;

/* Note: this class relies on static initialization. */
class nsCUPSShim {
    public:
        /**
         * Initialize this object. Attempt to load the CUPS shared
         * library and find function pointers for the supported
         * functions (see below).
         * @return PR_FALSE if the shared library could not be loaded, or if
         *                  any of the functions could not be found.
         *         PR_TRUE  for successful initialization.
         */
        bool Init();

        /**
         * @return PR_TRUE  if the object was initialized successfully.
         *         PR_FALSE otherwise.
         */
        bool IsInitialized() { return nsnull != mCupsLib; }

        /* Function pointers for supported functions. These are only
         * valid after successful initialization.
         */
        CupsAddOptionType   mCupsAddOption;
        CupsFreeDestsType   mCupsFreeDests;
        CupsGetDestType     mCupsGetDest;
        CupsGetDestsType    mCupsGetDests;
        CupsPrintFileType   mCupsPrintFile;
        CupsTempFdType      mCupsTempFd;

    private:
        PRLibrary *mCupsLib;
};



#endif /* nsCUPSShim_h___ */
