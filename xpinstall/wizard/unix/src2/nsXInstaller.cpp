/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */


#include "nsXInstaller.h"
#include "nsINIParser.h"

#include "XIErrors.h"

nsXInstaller::nsXInstaller()
{
}

nsXInstaller::~nsXInstaller()
{
}

int
nsXInstaller::ParseConfig()
{
    int err = OK;
    nsINIParser *parser = new nsINIParser( CONFIG_INI );
    char *bufalloc = NULL;
    char buf[512];
    int bufsize = 0;

    if (!parser)
        return E_MEM;    

    err = parser->GetError();
    if (err != nsINIParser::OK)
        return err;

    bufsize = 512;
    // err = parser->GetString("section2", "key2", buf, &bufsize);
    err = parser->GetStringAlloc("section2", "key2", &bufalloc, &bufsize); 
    if ( (err == nsINIParser::OK) && (bufalloc) && (bufsize > 0) )
    {
        DUMP("section2 key2 = ");
        DUMP(bufalloc);
    }

    return err;
}

int
nsXInstaller::Run()
{
    int err = OK;

    if ( (err = StartWizard()) != OK)
        goto au_revoir;

    if ( (err = Download()) != OK)
        goto au_revoir;
    
    if ( (err = Extract()) != OK)
        goto au_revoir;

    if ( (err = Install()) != OK)
        goto au_revoir;
    
au_revoir:
    return err;
}

int 
nsXInstaller::StartWizard()
{
    return OK;
}

int
nsXInstaller::Download()
{
    return OK;
}

int
nsXInstaller::Extract()
{
    return OK;
}

int
nsXInstaller::Install()
{
    return OK;
}

int
main(int argc, char **argv)
{
    nsXInstaller *installer = new nsXInstaller();
    int err = OK;

    if (installer)
    {
        if ( (err = installer->ParseConfig()) == OK)
            err = installer->Run();
    }

	exit(err);
}

