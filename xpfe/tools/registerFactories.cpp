/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include <stdio.h>

#include "nsRepository.h"
#include "nsIAppRunner.h"

static nsresult registerLib( const nsCID &cid, const char *libName ) {
    printf( "Registering library %s...", libName );
    nsresult rv = nsRepository::RegisterFactory( cid,       // Class ID.
                                                 libName,   // Library name.
                                                 PR_TRUE,   // Replace if already there.
                                                 PR_TRUE ); // Store it in the NS Registry.

    if ( rv == NS_OK ) {
        printf( "succeeded.\n" );
    } else {
        printf( "failed.\n" );
    }

    return rv;
}

int main(int argc, char* argv[]) {
    // Fire up the XPCOM repository with our NS Registry file.
    nsresult rv = nsRepository::Initialize("mozilla.reg");

    // Register required libraries.
    if ( rv == NS_OK ) {
        nsCID appRunner = NS_APPRUNNER_CID;
        registerLib( appRunner, "nsAppRunner.dll" );
    }

    // Return result (NS_OK means it worked).
    return rv;
}
