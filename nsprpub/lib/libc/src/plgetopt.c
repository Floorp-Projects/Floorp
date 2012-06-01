/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:          plgetopt.c
** Description:   utilities to parse argc/argv
*/

#include "prmem.h"
#include "prlog.h"
#include "prerror.h"
#include "plstr.h"
#include "plgetopt.h"

#include <string.h>

static char static_Nul = 0;

struct PLOptionInternal
{
    const char *options;        /* client options list specification */
    PRIntn argc;                /* original number of arguments */
    char **argv;                /* vector of pointers to arguments */
    PRIntn xargc;               /* which one we're processing now */
    const char *xargv;          /* where within *argv[xargc] */
    PRIntn minus;               /* do we already have the '-'? */
    const PLLongOpt *longOpts;  /* Caller's array */
    PRBool endOfOpts;           /* have reached a "--" argument */
    PRIntn optionsLen;          /* is strlen(options) */
};

/*
** Create the state in which to parse the tokens.
**
** argc        the sum of the number of options and their values
** argv        the options and their values
** options    vector of single character options w/ | w/o ':
*/
PR_IMPLEMENT(PLOptState*) PL_CreateOptState(
    PRIntn argc, char **argv, const char *options)
{
    return PL_CreateLongOptState( argc, argv, options, NULL);
}  /* PL_CreateOptState */

PR_IMPLEMENT(PLOptState*) PL_CreateLongOptState(
    PRIntn argc, char **argv, const char *options, 
    const PLLongOpt *longOpts)
{
    PLOptState *opt = NULL;
    PLOptionInternal *internal;

    if (NULL == options) 
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return opt;
    }

    opt = PR_NEWZAP(PLOptState);
    if (NULL == opt) 
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return opt;
    }

    internal = PR_NEW(PLOptionInternal);
    if (NULL == internal)
    {
        PR_DELETE(opt);
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }

    opt->option = 0;
    opt->value = NULL;
    opt->internal = internal;
    opt->longOption   =  0;
    opt->longOptIndex = -1;

    internal->argc = argc;
    internal->argv = argv;
    internal->xargc = 0;
    internal->xargv = &static_Nul;
    internal->minus = 0;
    internal->options = options;
    internal->longOpts = longOpts;
    internal->endOfOpts = PR_FALSE;
    internal->optionsLen = PL_strlen(options);

    return opt;
}  /* PL_CreateLongOptState */

/*
** Destroy object created by CreateOptState()
*/
PR_IMPLEMENT(void) PL_DestroyOptState(PLOptState *opt)
{
    PR_DELETE(opt->internal);
    PR_DELETE(opt);
}  /* PL_DestroyOptState */

PR_IMPLEMENT(PLOptStatus) PL_GetNextOpt(PLOptState *opt)
{
    PLOptionInternal *internal = opt->internal;

    opt->longOption   =  0;
    opt->longOptIndex = -1;
    /*
    ** If the current xarg points to nul, advance to the next
    ** element of the argv vector. If the vector index is equal
    ** to argc, we're out of arguments, so return an EOL.
    ** Note whether the first character of the new argument is
    ** a '-' and skip by it if it is.
    */
    while (0 == *internal->xargv)
    {
        internal->xargc += 1;
        if (internal->xargc >= internal->argc)
        {
            opt->option = 0;
            opt->value = NULL;
            return PL_OPT_EOL;
        }
        internal->xargv = internal->argv[internal->xargc];
        internal->minus = 0;
        if (!internal->endOfOpts && ('-' == *internal->xargv)) 
        {
            internal->minus++;
            internal->xargv++;  /* and consume */
            if ('-' == *internal->xargv && internal->longOpts) 
            {
                internal->minus++;
                internal->xargv++;
                if (0 == *internal->xargv) 
                {
                    internal->endOfOpts = PR_TRUE;
                }
            }
        }
    }

    /*
    ** If we already have a '-' or '--' in hand, xargv points to the next
    ** option. See if we can find a match in the list of possible
    ** options supplied.
    */
    if (internal->minus == 2) 
    {
        char * foundEqual = strchr(internal->xargv,'=');
        PRIntn optNameLen = foundEqual ? (foundEqual - internal->xargv) :
                            strlen(internal->xargv);
        const PLLongOpt *longOpt = internal->longOpts;
        PLOptStatus result = PL_OPT_BAD;

        opt->option = 0;
        opt->value  = NULL;

        for (; longOpt->longOptName; ++longOpt) 
        {
            if (strncmp(longOpt->longOptName, internal->xargv, optNameLen))
                continue;  /* not a possible match */
            if (strlen(longOpt->longOptName) != optNameLen)
                continue;  /* not a match */
            /* option name match */
            opt->longOptIndex = longOpt - internal->longOpts;
            opt->longOption   = longOpt->longOption;
            /* value is part of the current argv[] element if = was found */
            /* note: this sets value even for long options that do not
             * require option if specified as --long=value */
            if (foundEqual) 
            {
                opt->value = foundEqual + 1;
            }
            else if (longOpt->valueRequired)
            {
                /* value is the next argv[] element, if any */
                if (internal->xargc + 1 < internal->argc)
                {
                    opt->value = internal->argv[++(internal->xargc)];
                }
                /* missing value */
                else
                {
                    break; /* return PL_OPT_BAD */
                }
            }
            result = PL_OPT_OK;
            break;
        }
        internal->xargv = &static_Nul; /* consume this */
        return result;
    }
    if (internal->minus)
    {
        PRIntn cop;
        PRIntn eoo = internal->optionsLen;
        for (cop = 0; cop < eoo; ++cop)
        {
            if (internal->options[cop] == *internal->xargv)
            {
                opt->option = *internal->xargv++;
                opt->longOption = opt->option & 0xff;
                /*
                ** if options indicates that there's an associated
                ** value, it must be provided, either as part of this
                ** argv[] element or as the next one
                */
                if (':' == internal->options[cop + 1])
                {
                    /* value is part of the current argv[] element */
                    if (0 != *internal->xargv)
                    {
                        opt->value = internal->xargv;
                    }
                    /* value is the next argv[] element, if any */
                    else if (internal->xargc + 1 < internal->argc)
                    {
                        opt->value = internal->argv[++(internal->xargc)];
                    }
                    /* missing value */
                    else
                    {
                        return PL_OPT_BAD;
                    }

                    internal->xargv = &static_Nul;
                    internal->minus = 0;
                }
                else 
                    opt->value = NULL; 
                return PL_OPT_OK;
            }
        }
        internal->xargv += 1;  /* consume that option */
        return PL_OPT_BAD;
    }

    /*
    ** No '-', so it must be a standalone value. The option is nul.
    */
    opt->value = internal->argv[internal->xargc];
    internal->xargv = &static_Nul;
    opt->option = 0;
    return PL_OPT_OK;
}  /* PL_GetNextOpt */

/* plgetopt.c */
