/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <stdio.h>

#include "nsIRegistry.h"
#include "nsIEnumerator.h"
#include "nsIFactory.h"
#include "prmem.h"
#include "plstr.h"

// Hack to get to nsRegistry implementation.                                                                                       
extern "C" NS_EXPORT nsresult
NS_RegistryGetFactory(const nsCID &cid, nsISupports* servMgr, nsIFactory** aFactory );

static void display( nsIRegistry *reg, nsIRegistry::Key root, const char *name );
static void displayValues( nsIRegistry *reg, nsIRegistry::Key root );
static void printString( const char *value, int indent );

int main( int argc, char *argv[] ) {


#ifdef __MWERKS__
	// Hack in some arguments.  A NULL registry name is supposed to tell libreg
	// to use the default registry (which does seem to work).
	argc = 2;
	const char* myArgs[] =
	{
		"regExport"
	,	NULL
	};
	argv = const_cast<char**>(myArgs);
#endif

    // Get nsRegistry factory.
    nsCID cid = NS_IREGISTRY_IID; // Not really an IID, but this factory stuff is a hack anyway.
    nsIFactory *factory;
    nsresult rv = NS_RegistryGetFactory( cid, 0, &factory );

    // Check result.
    if ( rv == NS_OK ) {
        // Create registry implementation object.
        nsIID regIID = NS_IREGISTRY_IID;
        nsIRegistry *reg;
        rv = factory->CreateInstance( 0, regIID, (void**)&reg );

        // Check result.
        if ( rv == NS_OK ) {
            // Latch onto the registry object.
            reg->AddRef();

            // Open it against the input file name.
            rv = reg->Open( argv[1] );
        
            if ( rv == NS_OK ) {
                printf( "Registry %s opened OK.\n", argv[1] ? argv[1] : "<default>" );
        
                // Recurse over all 3 branches.
                display( reg, nsIRegistry::Common, "nsRegistry::Common" );
                display( reg, nsIRegistry::Users, "nsRegistry::Users" );
                display( reg, nsIRegistry::Common, "nsRegistry::CurrentUser" );
        
            } else {
                printf( "Error opening registry file %s, rv=0x%08X\n", argv[1] ? argv[1] : "<default>", (int)rv );
            }
            // Release the registry.
            reg->Release();
        } else {
            printf( "Error creating nsRegistry object, rv=0x%08X\n", (int)rv );
        }
        // Release the factory.
        factory->Release();
    } else {
        printf( "Error creating nsRegistry factory, rv=0x%08X\n", (int)rv );
    }

    return rv;
}

void display( nsIRegistry *reg, nsIRegistry::Key root, const char *rootName ) {
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
        while( NS_SUCCEEDED( rv ) && !keys->IsDone() ) {
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
                    rv = node->GetName( &name );
                    // Test result.
                    if ( rv == NS_OK ) {
                        // Build complete name.
                        char *fullName = new char[ PL_strlen(rootName) + PL_strlen(name) + 2 ];
                        PL_strcpy( fullName, rootName );
                        PL_strcat( fullName, "/" );
                        PL_strcat( fullName, name );
                        // Display contents under this subkey.
                        nsIRegistry::Key key;
                        rv = reg->GetSubtree( root, name, &key );
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

static void displayValues( nsIRegistry *reg, nsIRegistry::Key root ) {
    // Emumerate values at this registry location.
    nsIEnumerator *values;
    nsresult rv = reg->EnumerateValues( root, &values );

    // Check result.
    if ( rv == NS_OK ) {
        // Go to beginning.
        rv = values->First();

        // Enumerate values till done.
        while( rv == NS_OK && !values->IsDone() ) {
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
                    rv = value->GetName( &name );
                    // Test result.
                    if ( rv == NS_OK ) {
                        // Print name:
                        printf( "\t\t%s", name );
                        // Get info about this value.
                        uint32 type;
                        rv = reg->GetValueType( root, name, &type );
                        if ( rv == NS_OK ) {
                            // Print value contents.
                            switch ( type ) {
                                case nsIRegistry::String: {
                                        char *value;
                                        rv = reg->GetString( root, name, &value );
                                        if ( rv == NS_OK ) {
                                            printString( value, strlen(name) );
                                            PR_Free( value );
                                        } else {
                                            printf( "\t Error getting string value, rv=0x%08X", (int)rv );
                                        }
                                    }
                                    break;

                                case nsIRegistry::Int32:
                                    printf( "\t= Int32" );
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
