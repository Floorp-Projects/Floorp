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

#include <stdio.h>        // For printf, etc.

#include "nsRepository.h" // For nsRepository.
#include "nsIAppRunner.h" // For nsIAppRunner.

// Define Class IDs.
static NS_DEFINE_IID(kAppRunnerCID, NS_APPRUNNER_CID);

// Define Interface IDs.
static NS_DEFINE_IID(kIAppRunnerIID, NS_IAPPRUNNER_IID);

// Registry file name.
// This should probably be overridable via command line option
// or otherwise.
const char *registryFile = "mozilla.reg";

int main(int argc, char* argv[]) {
  nsresult rv;

  // Initialize XPCOM.
  rv = nsRepository::Initialize( registryFile );

  if ( rv == NS_OK ) {
      // Attempt to create appRunner.
      nsIAppRunner *appRunner = 0;
      rv = nsRepository::CreateInstance( kIAppRunnerIID,
                                         0,
                                         kAppRunnerCID,
                                         (void**)&appRunner );
      if ( rv == NS_OK && appRunner ) {
          // Run it.
          rv = appRunner->main( argc, argv );
      } else {
          fprintf( stderr, "Error creating appRunner object, rv=%lX\n", (unsigned long)rv );
      }
  } else {
      fprintf( stderr, "Error initilizing nsRepository, rv=%lX\n", (unsigned long)rv );
  }

  return rv;
}
