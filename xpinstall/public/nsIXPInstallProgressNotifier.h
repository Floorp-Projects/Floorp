/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Douglas Turner <dougt@netscape.com>
 */


#ifndef nsIXPInstallProgressNotifier_h__
#define nsIXPInstallProgressNotifier_h__


class nsIXPInstallProgressNotifier
{
     public:
        
        ///////////////////////////////////////////////////////////////////////////////////////////////////////
        // Function name	: BeforeJavascriptEvaluation
        // Description	    : This will be called when prior to the install script being evaluate
        // Return type		: void 
        // Argument         : void
        ///////////////////////////////////////////////////////////////////////////////////////////////////////

        virtual void BeforeJavascriptEvaluation(void) = 0;
        
        ///////////////////////////////////////////////////////////////////////////////////////////////////////
        // Function name	: AfterJavascriptEvaluation
        // Description	    : This will be called after the install script has being evaluated
        // Return type		: void 
        // Argument         : void
        ///////////////////////////////////////////////////////////////////////////////////////////////////////
         
        virtual void AfterJavascriptEvaluation(void) = 0;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////
        // Function name	: InstallStarted
        // Description	    : This will be called when StartInstall has been called
        // Return type		: void
        // Argument         : char* UIPackageName - User Package Name
        ///////////////////////////////////////////////////////////////////////////////////////////////////////

        virtual void InstallStarted(const char* UIPackageName) = 0;
        
        ///////////////////////////////////////////////////////////////////////////////////////////////////////
        // Function name	: ItemScheduled
        // Description	    : This will be called when items are being scheduled 
        // Return type		: Any value returned other than zero, will be treated as an error and the script will be aborted
        // Argument         : The message that should be displayed to the user
        ///////////////////////////////////////////////////////////////////////////////////////////////////////

        virtual long ItemScheduled( const char* message ) = 0;
        
        ///////////////////////////////////////////////////////////////////////////////////////////////////////
        // Function name	: InstallFinalization
        // Description	    : This will be called when the installation is in its Finalize stage 
        // Return type		: void 
        // Argument         : char* message - The message that should be displayed to the user
        // Argument         : long itemNum  - This is the current item number
        // Argument         : long totNum   - This is the total number of items
        ///////////////////////////////////////////////////////////////////////////////////////////////////////

        virtual void InstallFinalization( const char* message, long itemNum, long totNum ) = 0;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////
        // Function name	: InstallAborted
        // Description	    : This will be called when the install is aborted 
        // Return type		: void
        // Argument         : void
        ///////////////////////////////////////////////////////////////////////////////////////////////////////

        virtual void InstallAborted(void) = 0;

};


#endif
