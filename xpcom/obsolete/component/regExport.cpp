/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <stdio.h>

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIRegistry.h"
#include "nsIEnumerator.h"
#include "nsILocalFile.h"
#include "nsDependentString.h"
#include "prmem.h"
#include "plstr.h"
#include "nsMemory.h"

static void display( nsIRegistry *reg, nsRegistryKey root, const char *name );
static void displayValues( nsIRegistry *reg, nsRegistryKey root );
static void printString( const char *value, int indent );

int main( int argc, char *argv[] ) {


#ifdef __MWERKS__
    // Hack in some arguments.  A NULL registry name is supposed to tell libreg
    // to use the default registry (which does seem to work).
    argc = 1;
    const char* myArgs[] =
    {
        "regExport"
    };
    argv = const_cast<char**>(myArgs);
#endif

    nsresult rv;

    // Initialize XPCOM
    nsIServiceManager *servMgr = NULL;
    rv = NS_InitXPCOM2(&servMgr, NULL, NULL);
    if (NS_FAILED(rv))
    {
        // Cannot initialize XPCOM
        printf("Cannot initialize XPCOM. Exit. [rv=0x%08X]\n", (int)rv);
        exit(-1);
    }
    {
        // Get the component manager
        static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
        nsCOMPtr<nsIComponentManager> compMgr = do_GetService(kComponentManagerCID, &rv);
        if (NS_FAILED(rv))
        {
            // Cant get component manager
            printf("Cannot get component manager from service manager.. Exit. [rv=0x%08X]\n", (int)rv);
            exit(-1);
        }

        nsIRegistry *reg;

        if (argc>1) {
            // Create the registry
            rv = compMgr->CreateInstanceByContractID(NS_REGISTRY_CONTRACTID, NULL,
                                                 NS_GET_IID(nsIRegistry),
                                                 (void **) &reg);
            // Check result.
            if ( NS_FAILED(rv) )
    	    {   
                printf( "Error opening registry file %s, rv=0x%08X\n", argv[1] , (int)rv );
                return rv;
		    }
            // Open it against the input file name.
            nsCOMPtr<nsILocalFile> regFile;
            rv = NS_NewNativeLocalFile( nsDependentCString(argv[1]), PR_FALSE, getter_AddRefs(regFile) );
            if ( NS_FAILED(rv) ) {
                printf( "Error instantiating local file for %s, rv=0x%08X\n", argv[1], (int)rv );
                return rv;
            }

            rv = reg->Open( regFile );
    
            if ( rv == NS_OK ) 
            {
                printf( "Registry %s opened OK.\n", argv[1] );
            
                // Recurse over all 3 branches.
                display( reg, nsIRegistry::Common, "nsRegistry::Common" );
                display( reg, nsIRegistry::Users, "nsRegistry::Users" );
            }
            NS_RELEASE(reg);
        }
        else
        {
            // Called with no arguments. Print both the default registry and 
            // the components registry. We already printed the default regsitry.
            // So just do the component registry.
            rv = compMgr->CreateInstanceByContractID(NS_REGISTRY_CONTRACTID, NULL,
                                                 NS_GET_IID(nsIRegistry),
                                                 (void **) &reg);

            // Check result.
            if ( NS_FAILED(rv) )
            {   
                printf( "Error opening creating registry instance, rv=0x%08X\n", (int)rv );
                return rv;
            }
            rv = reg->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
            if ( rv == NS_ERROR_REG_BADTYPE ) {
                printf( "\n\n\nThere is no <Application Component Registry>\n" );
            }
            else if ( rv == NS_OK ) {

                printf( "\n\n\nRegistry %s opened OK.\n", "<Application Component Registry>\n" );
            
                // Recurse over all 3 branches.
                display( reg, nsIRegistry::Common, "nsRegistry::Common" );
                display( reg, nsIRegistry::Users, "nsRegistry::Users" );
            }
            NS_RELEASE(reg);
        }
    }
    NS_ShutdownXPCOM( servMgr );

    return rv;
}

void display( nsIRegistry *reg, nsRegistryKey root, const char *rootName ) {
    // Print out key name.
    printf( "%s\n", rootName );

    // Make sure it isn't a "root" key.
    if ( root != nsIRegistry::Common
         &&
         root != nsIRegistry::Users
         &&
         root != nsIRegistry::CurrentUser ) {
        // Print values stored under this key.
        displayValues( reg, root );
    }

    // Enumerate all subkeys (immediately) under the given node.
    nsIEnumerator *keys;
    nsresult rv = reg->EnumerateSubtrees( root, &keys );

    // Check result.
    if ( rv == NS_OK ) {
        // Set enumerator to beginning.
        rv = keys->First();
        // Enumerate subkeys till done.
        while( NS_SUCCEEDED( rv ) && (NS_OK != keys->IsDone()) ) {
            nsISupports *base;
            rv = keys->CurrentItem( &base );
            // Test result.
            if ( rv == NS_OK ) {
                // Get specific interface.
                nsIRegistryNode *node;
                nsIID nodeIID = NS_IREGISTRYNODE_IID;
                rv = base->QueryInterface( nodeIID, (void**)&node );
                // Test that result.
                if ( rv == NS_OK ) {
                    // Get node name.
                    char *name;
                    rv = node->GetNameUTF8( &name );
                    // Test result.
                    if ( rv == NS_OK ) {
                        // Build complete name.
                        char *fullName = new char[ PL_strlen(rootName) + PL_strlen(name) + 5 ];
                        PL_strcpy( fullName, rootName );
                        PL_strcat( fullName, " -  " );
                        PL_strcat( fullName, name );
                        // Display contents under this subkey.
                        nsRegistryKey key;
                        rv = reg->GetSubtreeRaw( root, name, &key );
                        if ( rv == NS_OK ) {
                            display( reg, key, fullName );
                            printf( "\n" );
                        } else {
                            printf( "Error getting key, rv=0x%08X\n", (int)rv );
                        }
                        delete [] fullName;
                    } else {
                        printf( "Error getting subtree name, rv=0x%08X\n", (int)rv );
                    }
                    // Release node.
                    node->Release();
                } else {
                    printf( "Error converting base node ptr to nsIRegistryNode, rv=0x%08X\n", (int)rv );
                }
                // Release item.
                base->Release();

                // Advance to next key.
                rv = keys->Next();
                // Check result.
                if ( NS_SUCCEEDED( rv ) ) {
                } else {
                    printf( "Error advancing enumerator, rv=0x%08X\n", (int)rv );
                }
            } else {
                printf( "Error getting current item, rv=0x%08X\n", (int)rv );
            }
        }
        // Release key enumerator.
        keys->Release();
    } else {
        printf( "Error creating enumerator for %s, root=0x%08X, rv=0x%08X\n",
                rootName, (int)root, (int)rv );
    }
    return;
}

static void displayValues( nsIRegistry *reg, nsRegistryKey root ) {
    // Emumerate values at this registry location.
    nsIEnumerator *values;
    nsresult rv = reg->EnumerateValues( root, &values );

    // Check result.
    if ( rv == NS_OK ) {
        // Go to beginning.
        rv = values->First();

        // Enumerate values till done.
        while( rv == NS_OK && (NS_OK != values->IsDone()) ) {
            nsISupports *base;
            rv = values->CurrentItem( &base );
            // Test result.
            if ( rv == NS_OK ) {
                // Get specific interface.
                nsIRegistryValue *value;
                nsIID valueIID = NS_IREGISTRYVALUE_IID;
                rv = base->QueryInterface( valueIID, (void**)&value );
                // Test that result.
                if ( rv == NS_OK ) {
                    // Get node name.
                    char *name;
                    rv = value->GetNameUTF8( &name );
                    // Test result.
                    if ( rv == NS_OK ) {
                        // Print name:
                        printf( "\t\t%s", name );
                        // Get info about this value.
                        PRUint32 type;
                        rv = reg->GetValueType( root, name, &type );
                        if ( rv == NS_OK ) {
                            // Print value contents.
                            switch ( type ) {
                                case nsIRegistry::String: {
                                        char *strValue;
                                        rv = reg->GetStringUTF8( root, name, &strValue );
                                        if ( rv == NS_OK ) {
                                            printString( strValue, strlen(name) );
                                            nsMemory::Free( strValue );
                                        } else {
                                            printf( "\t Error getting string value, rv=0x%08X", (int)rv );
                                        }
                                    }
                                    break;

                                case nsIRegistry::Int32:
                                    {
                                        PRInt32 val = 0;
                                        rv = reg->GetInt( root, name, &val );
                                        if (NS_SUCCEEDED(rv)) {
                                            printf( "\t= Int32 [%d, 0x%x]", val, val);
                                        }
                                        else {
                                            printf( "\t Error getting int32 value, rv=%08X", (int)rv);
                                        }
                                    }
                                    break;

                                case nsIRegistry::Bytes:
                                    printf( "\t= Bytes" );
                                    break;

                                case nsIRegistry::File:
                                    printf( "\t= File (?)" );
                                    break;

                                default:
                                    printf( "\t= ? (unknown type=0x%02X)", (int)type );
                                    break;
                            }
                        } else {
                            printf( "\t= ? (error getting value, rv=0x%08X)", (int)rv );
                        }
                        printf("\n");
                        nsMemory::Free( name );
                    } else {
                        printf( "Error getting value name, rv=0x%08X\n", (int)rv );
                    }
                    // Release node.
                    value->Release();
                } else {
                    printf( "Error converting base node ptr to nsIRegistryNode, rv=0x%08X\n", (int)rv );
                }
                // Release item.
                base->Release();

                // Advance to next key.
                rv = values->Next();
                // Check result.
                if ( NS_SUCCEEDED( rv ) ) {
                } else {
                    printf( "Error advancing enumerator, rv=0x%08X\n", (int)rv );
                    break;
                }
            } else {
                printf( "Error getting current item, rv=0x%08X\n", (int)rv );
                break;
            }
        }

        values->Release();
    } else {
        printf( "\t\tError enumerating values, rv=0x%08X\n", (int)rv );
    }
    return;
}

static void printString( const char *value, int /*indent*/ ) {
    // For now, just dump contents.
    printf( "\t = %s", value );
    return;
}
