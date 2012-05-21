/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
         * @return false if the shared library could not be loaded, or if
         *                  any of the functions could not be found.
         *         true  for successful initialization.
         */
        bool Init();

        /**
         * @return true  if the object was initialized successfully.
         *         false otherwise.
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
