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
#ifndef nsAppRunner_h__
#define nsAppRunner_h__

#include "nsIAppRunner.h"

class string;

// nsAppRunner
//
// Implements nsIAppRunner interface.
//
class nsAppRunner : public nsIAppRunner
{
public:
// nsISupports interface implementation.
    NS_DECL_ISUPPORTS

// nsIAppRunner interface implementation.
    NS_IMETHOD main( int argc, char *argv[] );

// Implementation
    // ctor
    nsAppRunner();

    // Access command line options:
    //   ParseCommandLineOptions - Parses command line options.
    //   IsOptionSpecified       - Returns PR_TRUE if switch was present in command line args.
    //   ValueForOption          - Returns option value; e.g., -P"mozilla" -> string("mozilla")
    virtual PRBool ParseCommandLine( int argc, char *argv[] );
    virtual PRBool IsOptionSpecified( const char *key );
    virtual string ValueForOption( const char *key );

    // Process management:
    //   Initialize       - Initialization; default does nothing.
    //   OnExit           - Called just prior to exit; default returns
    //                      input argument.
    virtual nsresult Initialize();
    virtual nsresult OnExit( nsresult rvIn );

    // AppShell loading:
    //   AppShellCID  - Returns CID for app shell to be loaded/started.
    //   LoadAppShell - Default creates appShell from ServiceManager using
    //                  AppShellCID().
    //   RunShell     - Initialize/Run/Shutdown the app shell
    //   AppShell     - Returns pointer to app shell
    virtual nsCID        AppShellCID() const;
    virtual nsresult     LoadAppShell();
    virtual nsresult     RunShell();
    virtual nsIAppShell *AppShell() const;

    // Utilities:
    //   DisplayMsg - Displays error/status message.  Default is to outpout to
    //                stderr.  Override to put up message box, etc.
    virtual void DisplayMsg( const char *title, const char *text );
            void DisplayMsg( const char *title, const char *text, ... );
            void DisplayMsg( const char *title, const char *text, valist args );

protected:
    virtual ~nsAppRunner();

private:
    int mArgc;
    struct Arg {
        const char *key;
        const char *value;
    } *mArg;
    int IndexOfArg( const char *key, PRBool ignoreCase );
}; //  nsAppRunner

#endif
