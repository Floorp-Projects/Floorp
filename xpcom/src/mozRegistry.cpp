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

#include "mozIRegistry.h"
#include "nsIEnumerator.h"
#include "NSReg.h"
#include "prmem.h"

/*------------------------------- mozRegistry ----------------------------------
| This class implements the mozIRegistry interface using the functions         |
| provided by libreg (as declared in mozilla/modules/libreg/include/NSReg.h).  |
|                                                                              |
| Since that interface is designed to match the libreg function, this class    |
| is implemented with each member function being a simple wrapper for the      |
| corresponding libreg function.                                               |
------------------------------------------------------------------------------*/
struct mozRegistry : public mozIRegistry {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the mozIRegistry interface functions.
    NS_IMETHOD Open( const char *regFile = 0 );
    NS_IMETHOD OpenDefault();
    NS_IMETHOD Close();

    NS_IMETHOD GetString( Key baseKey, const char *path, char **result );
    NS_IMETHOD SetString( Key baseKey, const char *path, const char *value );
    NS_IMETHOD GetInt( Key baseKey, const char *path, int32 *result );
    NS_IMETHOD SetInt( Key baseKey, const char *path, int32 value );
    NS_IMETHOD GetBytes( Key baseKey, const char *path, void **result, uint32 *len );
    NS_IMETHOD SetBytes( Key baseKey, const char *path, void *value, uint32 len );
    NS_IMETHOD GetIntArray( Key baseKey, const char *path, int32 **result, uint32 *len );
    NS_IMETHOD SetIntArray( Key baseKey, const char *path, const int32 *value, uint32 len );

    NS_IMETHOD AddSubtree( Key baseKey, const char *path, Key *result );
    NS_IMETHOD RemoveSubtree( Key baseKey, const char *path );
    NS_IMETHOD GetSubtree( Key baseKey, const char *path, Key *result );

    NS_IMETHOD EnumerateSubtrees( Key baseKey, nsIEnumerator **result );
    NS_IMETHOD EnumerateAllSubtrees( Key baseKey, nsIEnumerator **result );

    NS_IMETHOD GetValueType( Key baseKey, const char *path, uint32 *result );
    NS_IMETHOD GetValueLength( Key baseKey, const char *path, uint32 *result );

    NS_IMETHOD EnumerateValues( Key baseKey, nsIEnumerator **result );

    NS_IMETHOD GetCurrentUserName( char **result );
    NS_IMETHOD SetCurrentUserName( const char *name );

    NS_IMETHOD Pack();

    // ctor/dtor
    mozRegistry();
    ~mozRegistry();

protected:
    HREG   mReg; // Registry handle.
    REGERR mErr; // Last libreg error code.
}; // mozRegistry


#include "nsIFactory.h"
/*---------------------------- mozRegistryFactory ------------------------------
| Class factory for mozRegistry objects.                                       |
------------------------------------------------------------------------------*/
struct mozRegistryFactory : public nsIFactory {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *,const nsIID &,void **);
    NS_IMETHOD LockFactory(PRBool aLock);

    // ctor
    mozRegistryFactory();
};


/*-------------------------- mozRegSubtreeEnumerator ---------------------------
| This class implements the nsIEnumerator interface and is used to implement   |
| the mozRegistry EnumerateSubtrees and EnumerateAllSubtrees functions.        |
------------------------------------------------------------------------------*/
struct mozRegSubtreeEnumerator : public nsIEnumerator {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIEnumerator interface functions.
    nsresult First();
    nsresult Last();
    nsresult Next();
    nsresult Prev();
    nsresult CurrentItem(nsISupports **aItem);
    nsresult IsDone();

    // ctor/dtor
    mozRegSubtreeEnumerator( HREG hReg, RKEY rKey, PRBool all );

protected:
    NS_IMETHOD advance(); // Implementation file; does appropriate NR_RegEnum call.
    HREG    mReg;   // Handle to registry we're affiliated with.
    RKEY    mKey;   // Base key being enumerated.
    REGENUM mEnum;  // Corresponding libreg "enumerator".
    REGENUM mNext;  // Lookahead value.
    uint32  mStyle; // Style (indicates all or some);
    PRBool  mDone;  // Done flag.
    REGERR  mErr;   // Last libreg error code.
}; // mozRegSubtreeEnumerator


/*-------------------------- mozRegValueEnumerator -----------------------------
| This class is a variation on mozRegSubtreeEnumerator that allocates          |
| mozRegistryValue objects rather than mozRegistryNode objects.  It also       |
| overrides certain functions to make sure the "value" oriented libreg         |
| functions used rather than the subtree oriented ones.                        |
------------------------------------------------------------------------------*/
struct mozRegValueEnumerator : public mozRegSubtreeEnumerator {
    // Override CurrentItem to allocate mozRegistryValue objects.
    nsresult CurrentItem( nsISupports **result );

    // Override advance() to use proper NR_RegEnumEntries.
    NS_IMETHOD advance();

    // ctor/dtor
    mozRegValueEnumerator( HREG hReg, RKEY rKey );
}; // mozRegValueEnumerator

/*----------------------------- mozRegistryNode --------------------------------
| This class implements the mozIRegistryNode interface.  Instances are         |
| allocated by mozRegSubtreeEnumerator::CurrentItem.                           |
------------------------------------------------------------------------------*/
struct mozRegistryNode : public mozIRegistryNode {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the mozIRegistryNode interface functions.
    NS_IMETHOD GetName( char **result );

    // ctor
    mozRegistryNode( HREG hReg, RKEY key, REGENUM slot );
    
protected:
    HREG    mReg;  // Handle to registry this node is part of.
    RKEY    mKey;  // Key this node is under.
    REGENUM mEnum; // Copy of corresponding content of parent enumerator.
    REGERR  mErr;  // Last libreg error code.
    char    mName[MAXREGPATHLEN]; // Buffer to hold name.
}; // mozRegistryNode


/*----------------------------- mozRegistryValue -------------------------------
| This class implements the mozIRegistryValue interface.  Instances are        |
| allocated by mozRegValueEnumerator::CurrentItem.                             |
------------------------------------------------------------------------------*/
struct mozRegistryValue : public mozIRegistryValue {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the mozIRegistryValue interface functions.
    NS_IMETHOD GetName( char **result );
    NS_IMETHOD GetValueType( uint32 *result );
    NS_IMETHOD GetValueLength( uint32 *result );

    // ctor
    mozRegistryValue( HREG hReg, RKEY key, REGENUM slot );

protected:
    nsresult getInfo(); // Get registry info.
    HREG    mReg;  // Handle to registry this node is part of.
    RKEY    mKey;  // Key this node is under.
    REGENUM mEnum; // Copy of corresponding content of parent enumerator.
    REGERR  mErr;  // Last libreg error code.
    REGINFO mInfo; // Value info.
    char    mName[MAXREGNAMELEN]; // Buffer to hold name.
}; // mozRegistryValue


/*----------------------------- regerr2nsresult --------------------------------
| This utility function maps a REGERR value to a corresponding nsresult        |
| error code.                                                                  |
------------------------------------------------------------------------------*/
static nsresult regerr2nsresult( REGERR err ) {
    nsresult rv = NS_ERROR_UNEXPECTED;
    switch( err ) {
        case REGERR_OK:
            rv = NS_OK;
            break;

        case REGERR_FAIL:
            rv = NS_ERROR_FAILURE;
            break;

        case REGERR_NOMORE:
            rv = NS_ERROR_REG_NO_MORE;
            break;
    
        case REGERR_NOFIND:
            rv = NS_ERROR_REG_NOT_FOUND;
            break;
    
        case REGERR_PARAM:
        case REGERR_BADTYPE:
        case REGERR_BADNAME:
            rv = NS_ERROR_INVALID_ARG;
            break;
    
        case REGERR_NOFILE:
            rv = NS_ERROR_REG_NOFILE;
            break;
    
        case REGERR_MEMORY:
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
    
        case REGERR_BUFTOOSMALL:
            rv = NS_ERROR_REG_BUFFER_TOO_SMALL;
            break;
    
        case REGERR_NAMETOOLONG:
            rv = NS_ERROR_REG_NAME_TOO_LONG;
            break;
    
        case REGERR_NOPATH:
            rv = NS_ERROR_REG_NO_PATH;
            break;
    
        case REGERR_READONLY:
            rv = NS_ERROR_REG_READ_ONLY;
            break;
    
        case REGERR_BADUTF8:
            rv = NS_ERROR_REG_BAD_UTF8;
            break;
    
    }
    return rv;
}

/*----------------------------- reginfo2DataType -------------------------------
| This utility function converts the type field in the REGINFO structure to    |
| the corresponding mozIRegistry::DataType value.                              |
------------------------------------------------------------------------------*/
static void reginfo2DataType( const REGINFO &in, uint32 &out ) {
    // Transfer information, based on entry type.
    switch( in.entryType ) {
        case REGTYPE_ENTRY_STRING_UTF:
            out = mozIRegistry::String;
            //out.length = in.entryLength;
            break;

        case REGTYPE_ENTRY_INT32_ARRAY:
            out = mozIRegistry::Int32;
            // Convert length in bytes to array dimension.
            //out.length = in.entryLength / sizeof(int32);
            break;

        case REGTYPE_ENTRY_BYTES:
            out = mozIRegistry::Bytes;
            //out.length = in.entryLength;
            break;

        case REGTYPE_ENTRY_FILE:
            out = mozIRegistry::File;
            //out.length = in.entryLength;
            break;
    }
}

/*----------------------------- reginfo2DataType -------------------------------
| This utility function converts the length field in the REGINFO structure to  |
| the proper units (if type==Int32 array, we divide by sizeof(int32)).         |
------------------------------------------------------------------------------*/
static void reginfo2Length( const REGINFO &in, uint32 &out ) {
    // Transfer information, based on entry type.
    switch( in.entryType ) {
        case REGTYPE_ENTRY_STRING_UTF:
            out = in.entryLength;
            break;

        case REGTYPE_ENTRY_INT32_ARRAY:
            // Convert length in bytes to array dimension.
            out = in.entryLength / sizeof(int32);
            break;

        case REGTYPE_ENTRY_BYTES:
            out = in.entryLength;
            break;

        case REGTYPE_ENTRY_FILE:
            out = in.entryLength;
            break;
    }
}

/*----------------------------------- IIDs -------------------------------------
| Static IID values for each imterface implemented here; required by the       |
| NS_IMPL_ISUPPORTS macro.                                                     |
------------------------------------------------------------------------------*/
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRegistryIID,      MOZ_IREGISTRY_IID);
static NS_DEFINE_IID(kIRegistryNodeIID,  MOZ_IREGISTRYNODE_IID);
static NS_DEFINE_IID(kIRegistryValueIID, MOZ_IREGISTRYVALUE_IID);

static NS_DEFINE_IID(kIEnumeratorIID, NS_IENUMERATOR_IID);

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

/*------------------------ nsISupports Implementation --------------------------
| This code generates the implementation of the nsISupports member functions   |
| for each class implemented in this file.                                     |
------------------------------------------------------------------------------*/
NS_IMPL_ISUPPORTS( mozRegistry,             kIRegistryIID      );
NS_IMPL_ISUPPORTS( mozRegSubtreeEnumerator, kIEnumeratorIID    );
NS_IMPL_ISUPPORTS( mozRegistryNode,         kIRegistryNodeIID  );
NS_IMPL_ISUPPORTS( mozRegistryValue,        kIRegistryValueIID );

/*------------------------- mozRegistry::mozRegistry ---------------------------
| Vanilla mozRegistry constructor.  The first time called, it does             |
| NR_StartupRegistry.                                                          |
------------------------------------------------------------------------------*/
mozRegistry::mozRegistry() 
    : mReg( 0 ) {
    NS_INIT_REFCNT();

    // Ensure libreg is started.
    static PRBool libregStarted = PR_FALSE;
    if( !libregStarted ) {
        NR_StartupRegistry();
        libregStarted = PR_TRUE;
    }

    return;
}

/*------------------------ mozRegistry::~mozRegistry ---------------------------
| The dtor closes the registry file(if open).                                 |
------------------------------------------------------------------------------*/
mozRegistry::~mozRegistry() {
    if( mReg ) {
        Close();
    }
    return;
}

/*---------------------------- mozRegistry::Open -------------------------------
| If the argument is null, delegate to OpenDefault, else open the registry     |
| file.  We first check to see if a registry file is already open and close    |
| it if so.                                                                    |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::Open( const char *regFile ) {
    // Check for default.
    if( !regFile ) {
        return OpenDefault();
    }
    // Ensure existing registry is closed.
    Close();
    // Open specified registry.
    mErr = NR_RegOpen((char*)regFile, &mReg );
    // Convert the result.
    return regerr2nsresult( mErr );
}

/*------------------------- mozRegistry::OpenDefault ---------------------------
| Open the "default" registry; in the case of this libreg-based implementation |
| that is done by passing a null file name pointer to NR_RegOpen.              |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::OpenDefault() {
    // Ensure existing registry is closed.
    Close();
    // Open default registry.
    mErr = NR_RegOpen( 0, &mReg );
    // Convert the result.
    return regerr2nsresult( mErr );
}

/*---------------------------- mozRegistry::Close ------------------------------
| Tests the mReg handle and if non-null, closes the registry via NR_RegClose.  |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::Close() {
    mErr = REGERR_OK;
    if( mReg ) {
        mErr = NR_RegClose( mReg );
        mReg = 0;
    }
    return regerr2nsresult( mErr );
}

/*-------------------------- mozRegistry::GetString ----------------------------
| First, look for the entry using GetValueInfo.  If found, and it's a string,  |
| allocate space for it and fetch the value.                                   |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::GetString( Key baseKey, const char *path, char **result ) {
    nsresult rv = NS_OK;
    // Make sure caller gave us place for result.
    if( result ) {
        *result = 0; // Clear result.

        // Get info about the requested entry.
        uint32 type, length;
        rv = GetValueType( baseKey, path, &type );
        if ( rv == NS_OK ) {
            rv = GetValueLength( baseKey, path, &length );
        }
        // See if that worked.
        if( rv == NS_OK ) {
            // Make sure the entry is a string.
            if( type == String ) {
                // Allocate space for result.
                *result =(char*)PR_Malloc( length + 1 );
                if( *result ) {
                    // Get string from registry into result buffer.
                    mErr = NR_RegGetEntryString( mReg,(RKEY)baseKey,(char*)path, *result, length+1 );
                    // Convert status.
                    rv = regerr2nsresult( mErr );
                    // Test result.
                    if( rv != NS_OK ) {
                        // Didn't get result, free buffer.
                        PR_Free( *result );
                        *result = 0;
                    }
                } else {
                    // Couldn't allocate buffer.
                    rv = NS_ERROR_OUT_OF_MEMORY;
                }
            } else {
                // They asked for the wrong type of value.
                rv = regerr2nsresult( REGERR_BADTYPE );
            }
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*-------------------------- mozRegistry::SetString ----------------------------
| Simply sets the registry contents using NR_RegSetEntryString.                |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::SetString( Key baseKey, const char *path, const char *value ) {
    nsresult rv = NS_OK;
    // Set the contents.
    mErr = NR_RegSetEntryString( mReg,(RKEY)baseKey,(char*)path,(char*)value );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*--------------------------- mozRegistry::GetInt ------------------------------
| This function is just shorthand for fetching a 1-element int32 array.  We    |
| implement it "manually" using NR_RegGetEntry(versus calling GetIntArray)    |
| to save allocating a copy of the result.                                     |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::GetInt( Key baseKey, const char *path, int32 *result ) {
    nsresult rv = NS_OK;

    // Make sure caller gave us place for result.
    if( result ) {
        // Get info about the requested entry.
        uint32 type;
        rv = GetValueType( baseKey, path, &type );
        // See if that worked.
        if( rv == NS_OK ) {
            // Make sure the entry is an int32 array.
            if( type == Int32 ) {
                uint32 len = sizeof *result;
                // Get int from registry into result field.
                mErr = NR_RegGetEntry( mReg,(RKEY)baseKey,(char*)path, result, &len );
                // Convert status.
                rv = regerr2nsresult( mErr );
            } else {
                // They asked for the wrong type of value.
                rv = NS_ERROR_REG_BADTYPE;
            }
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*--------------------------- mozRegistry::SetInt ------------------------------
| Write out the value as a one-element int32 array, using NR_RegSetEntry.      |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::SetInt( Key baseKey, const char *path, int32 value ) {
    nsresult rv = NS_OK;
    // Set the contents.
    mErr = NR_RegSetEntry( mReg,
                (RKEY)baseKey,
                (char*)path,
                           REGTYPE_ENTRY_INT32_ARRAY,
                           &value,
                           sizeof value );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*-------------------------- mozRegistry::GetBytes -----------------------------
| Get the registry contents at specified location using NR_RegGetEntry.        |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::GetBytes( Key baseKey, const char *path, void **result, uint32 *len ) {
    nsresult rv = NS_OK;

    // Make sure caller gave us place for result.
    if( result && len ) {
        // Get info about the requested entry.
        uint32 type;
        rv = GetValueType( baseKey, path, &type );
        // See if that worked.
        if( rv == NS_OK ) {
            // Make sure the entry is bytes.
            if( type == Bytes ) {
                // Get bytes from registry into result field.
                mErr = NR_RegGetEntry( mReg,(RKEY)baseKey,(char*)path, result, len );
                // Convert status.
                rv = regerr2nsresult( mErr );
            } else {
                // They asked for the wrong type of value.
                rv = NS_ERROR_REG_BADTYPE;
            }
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*-------------------------- mozRegistry::SetBytes -----------------------------
| Set the contents at the specified registry location, using NR_RegSetEntry.   |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::SetBytes( Key baseKey, const char *path, void *value, uint32 len ) {
    nsresult rv = NS_OK;
    // Set contents.
    mErr = NR_RegSetEntry( mReg,(RKEY)baseKey,(char*)path,
                           REGTYPE_ENTRY_BYTES, value, len );
    // Convert result;
    rv = regerr2nsresult( mErr );
    return rv;
}

/*------------------------- mozRegistry::GetIntArray ---------------------------
| Find out about the entry using GetValueInfo.  We check the type and then     |
| use NR_RegGetEntry to get the actual array.  We have to convert from the     |
| array dimension to number of bytes in the process.                           |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::GetIntArray( Key baseKey, const char *path, int32 **result, uint32 *len ) {
    nsresult rv = NS_OK;

    // Make sure caller gave us place for result.
    if( result && len ) {
        // Get info about the requested entry.
        uint32 type, length;
        rv = GetValueType( baseKey, path, &type );
        if ( rv == NS_OK ) {
            rv = GetValueLength( baseKey, path, &length );
        } 
        // See if that worked.
        if( rv == NS_OK ) {
            // Make sure the entry is bytes.
            if( type == Int32 ) {
                // Allocate space for result.
                *len = length * sizeof(int32);
                *result =(int32*)PR_Malloc( *len );
                // Make sure that worked.
                if( *result ) {
                    // Get array from registry into result field.
                    mErr = NR_RegGetEntry( mReg,(RKEY)baseKey,(char*)path, *result, len );
                    // Convert status.
                    if( mErr == REGERR_OK ) {
                        // Convert size in bytes to array dimension.
                        *len /= sizeof(int32);
                    } else {
                        rv = regerr2nsresult( mErr );
                        // Free buffer that we allocated(error will tell caller not to).
                        PR_Free( *result );
                        *result = 0;
                    }
                } else {
                    rv = NS_ERROR_OUT_OF_MEMORY;
                }
            } else {
                // They asked for the wrong type of value.
                rv = NS_ERROR_REG_BADTYPE;
            }
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*------------------------- mozRegistry::SetIntArray ---------------------------
| Store the given integer array at the given point in the registry.  The       |
| length given is the size of the array, we have to convert that to the        |
| size in bytes in order to use NR_RegSetEntry.                                |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::SetIntArray( Key baseKey, const char *path, const int32 *value, uint32 len ) {
    nsresult rv = NS_OK;
    // Convert array dimension to byte count.
    uint32 size = len * sizeof(int32);
    // Set contents.
    mErr = NR_RegSetEntry( mReg,(RKEY)baseKey,(char*)path,
                           REGTYPE_ENTRY_INT32_ARRAY,(void*)value, size );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*------------------------- mozRegistry::AddSubtree ----------------------------
| Add a new registry subkey with the specified name, using NR_RegAddKey.       |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::AddSubtree( Key baseKey, const char *path, Key *result ) {
    nsresult rv = NS_OK;
    // Add the subkey.
    mErr = NR_RegAddKey( mReg,(RKEY)baseKey,(char*)path,(RKEY*)result );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*------------------------ mozRegistry::RemoveSubtree --------------------------
| Deletes the subtree at a given location using NR_RegDeleteKey.               |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::RemoveSubtree( Key baseKey, const char *path ) {
    nsresult rv = NS_OK;
    // Delete the subkey.
    mErr = NR_RegDeleteKey( mReg,(RKEY)baseKey,(char*)path );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*------------------------- mozRegistry::GetSubtree ----------------------------
| Returns a mozIRegistry::Key(RKEY) for a given key/path.  The key is         |
| obtained using NR_RegGetKey.                                                 |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::GetSubtree( Key baseKey, const char *path, Key *result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place for the result.
    if( result ) {
        // Get key.
        mErr = NR_RegGetKey( mReg,(RKEY)baseKey,(char*)path,(RKEY*)result );
        // Convert result.
        rv = regerr2nsresult( mErr );
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*---------------------- mozRegistry::EnumerateSubtrees ------------------------
| Allocate a mozRegSubtreeEnumerator object and return it to the caller.       |
| We construct the enumerator using the registry handle from this registry     |
| object, the user-specified registry key, and indicate that we don't want     |
| to recurse down subtrees.  No libreg functions are invoked at this point     |
|(that will happen when the enumerator member functions are called).          |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::EnumerateSubtrees( Key baseKey, nsIEnumerator **result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        *result = new mozRegSubtreeEnumerator( mReg,(RKEY)baseKey, PR_FALSE );
        // Check for success.
        if( *result ) {
            // Bump refcnt on behalf of caller.
 (*result)->AddRef();
        } else {
            // Unable to allocate space for the enumerator object.
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*-------------------- mozRegistry::EnumerateAllSubtrees -----------------------
| Same as EnumerateSubtrees but we pass PR_TRUE to request that the            |
| enumerator object descend subtrees when it is used.                          |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::EnumerateAllSubtrees( Key baseKey, nsIEnumerator **result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        *result = new mozRegSubtreeEnumerator( mReg,(RKEY)baseKey, PR_TRUE );
        // Check for success.
        if( *result ) {
            // Bump refcnt on behalf of caller.
 (*result)->AddRef();
        } else {
            // Unable to allocate space for the enumerator object.
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*------------------------ mozRegistry::GetValueType ---------------------------
| Gets the type from the registry using the NR_GetEntryInfo libreg API.        |
| The result is transferred to the uint32 value passed in (with conversion     |
| to the appropriate mozIRegistry::DataType value).                            |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::GetValueType( Key baseKey, const char *path, uint32 *result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        // Get registry info into local structure.
        REGINFO info = { sizeof info, 0, 0 };
        mErr = NR_RegGetEntryInfo( mReg,(RKEY)baseKey,(char*)path, &info );
        if( mErr == REGERR_OK ) {
            // Copy info to user's result value.
            reginfo2DataType( info, *result );
        } else {
            rv = regerr2nsresult( mErr );
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*----------------------- mozRegistry::GetValueLength --------------------------
| Gets the registry value info via NR_RegGetEntryInfo.  The length is          |
| converted to the proper "units" via reginfo2Length.                          |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::GetValueLength( Key baseKey, const char *path, uint32 *result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        // Get registry info into local structure.
        REGINFO info = { sizeof info, 0, 0 };
        mErr = NR_RegGetEntryInfo( mReg,(RKEY)baseKey,(char*)path, &info );
        if( mErr == REGERR_OK ) {
            // Copy info to user's result value.
            reginfo2Length( info, *result );
        } else {
            rv = regerr2nsresult( mErr );
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*----------------------- mozRegistry::EnumerateValues -------------------------
| Allocates and returns an instance of mozRegValueEnumerator constructed in    |
| a similar fashion as the mozRegSubtreeEnumerator is allocated/returned by    |
| EnumerateSubtrees.                                                           |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::EnumerateValues( Key baseKey, nsIEnumerator **result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        *result = new mozRegValueEnumerator( mReg,(RKEY)baseKey );
        // Check for success.
        if( *result ) {
            // Bump refcnt on behalf of caller.
            (*result)->AddRef();
        } else {
            // Unable to allocate space for the enumerator object.
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*--------------------- mozRegistry::GetCurrentUserName ------------------------
| Simple wrapper for NR_RegGetUsername.                                        |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::GetCurrentUserName( char **result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        // Get the user name.
        mErr = NR_RegGetUsername( result );
        // Convert the result.
        rv = regerr2nsresult( mErr );
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*--------------------- mozRegistry::SetCurrentUserName ------------------------
| Simple wrapper for NR_RegSetUsername.                                        |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::SetCurrentUserName( const char *name ) {
    nsresult rv = NS_OK;
    // Set the user name.
    mErr = NR_RegSetUsername( name );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*---------------------------- mozRegistry::Pack -------------------------------
| Simple wrapper for NR_RegPack.  We don't set up any callback.                |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistry::Pack() {
    nsresult rv = NS_OK;
    // Pack the registry.
    mErr = NR_RegPack( mReg, 0, 0 );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*------------- mozRegSubtreeEnumerator::mozRegSubtreeEnumerator ---------------
| The ctor simply stashes all the information that will be needed to enumerate |
| the subkeys.                                                                 |
------------------------------------------------------------------------------*/
mozRegSubtreeEnumerator::mozRegSubtreeEnumerator( HREG hReg, RKEY rKey, PRBool all )
    : mReg( hReg ), mKey( rKey ), mEnum( 0 ), mNext( 0 ),
      mStyle( all ? REGENUM_DESCEND : 0 ), mDone( PR_FALSE ), mErr( -1 ) {
    NS_INIT_REFCNT();
    return;
}

/*---------------------- mozRegSubtreeEnumerator::First ------------------------
| Set mEnum to 0; this will cause the next NR_RegEnum call to go to            |
| the beginning.  We then do a Next() call in order to do a "lookahead" to     |
| properly detect an empty list (i.e., set the mDone flag).                    |
------------------------------------------------------------------------------*/
nsresult mozRegSubtreeEnumerator::First() {
    nsresult rv = NS_OK;
    // Reset "done" flag.
    mDone = PR_FALSE;
    // Go to beginning.
    mEnum = mNext = 0;
    // Lookahead so mDone flag gets set for empty list.
    rv = Next();
    return rv;
}

/*---------------------- mozRegSubtreeEnumerator::Last -------------------------
| This can't be implemented using the libreg functions.                        |
------------------------------------------------------------------------------*/
nsresult mozRegSubtreeEnumerator::Last() {
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    return rv;
}

/*---------------------- mozRegSubtreeEnumerator::Next -------------------------
| First, we check if we've already advanced to the end by checking the  mDone  |
| flag.                                                                        |
|                                                                              |
| We advance mEnum to the next enumeration value which is in the mNext         |
| lookahead buffer.  We must then call advance to lookahead and properly set   |
| the isDone flag.                                                             |
------------------------------------------------------------------------------*/
nsresult mozRegSubtreeEnumerator::Next() {
    nsresult rv = NS_OK;
    // Check for at end.
    if ( !mDone ) {
        // Advance to next spot.
        mEnum = mNext;
        // Lookahead so mDone is properly set (and to update mNext).
        rv = advance();
    } else {
        // Set result accordingly.
        rv = regerr2nsresult( REGERR_NOMORE );
    }
    return rv;
}

/*--------------------- mozRegSubtreeEnumerator::advance -----------------------
| Advance mNext to next subkey using NR_RegEnumSubkeys.  We set mDone if       |
| there are no more subkeys.                                                   |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegSubtreeEnumerator::advance() {
    char name[MAXREGPATHLEN];
    uint32 len = sizeof name;
    mErr = NR_RegEnumSubkeys( mReg, mKey, &mNext, name, len, mStyle );
    // See if we ran off end.
    if( mErr == REGERR_NOMORE ) {
        // Remember we've run off end.
        mDone = PR_TRUE;
    }
    // Convert result.
    nsresult rv = regerr2nsresult( mErr );
    return rv;
};

/*---------------------- mozRegSubtreeEnumerator::Prev -------------------------
| This can't be implemented on top of libreg.                                  |
------------------------------------------------------------------------------*/
nsresult mozRegSubtreeEnumerator::Prev() {
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    return rv;
}

/*------------------- mozRegSubtreeEnumerator::CurrentItem ---------------------
| Allocates and returns a new instance of class mozRegistryNode.  The node     |
| object will hold the curent mEnum value so it can obtain its name from       |
| the registry when asked.                                                     |
------------------------------------------------------------------------------*/
nsresult mozRegSubtreeEnumerator::CurrentItem( nsISupports **result) {
    nsresult rv = NS_OK;
    // Make sure there is a place to put the result.
    if( result ) {
        *result = new mozRegistryNode( mReg, mKey, mEnum );
        if( *result ) {
 (*result)->AddRef();
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*--------------------- mozRegSubtreeEnumerator::IsDone ------------------------
| Simply return mDone.                                                         |
------------------------------------------------------------------------------*/
nsresult mozRegSubtreeEnumerator::IsDone() {
    nsresult rv = mDone;
    return rv;
}


/*--------------- mozRegValueEnumerator::mozRegValueEnumerator -----------------
| Delegates everything to the base class constructor.                          |
------------------------------------------------------------------------------*/
mozRegValueEnumerator::mozRegValueEnumerator( HREG hReg, RKEY rKey )
    : mozRegSubtreeEnumerator( hReg, rKey, PR_FALSE ) {
    return;
}


/*-------------------- mozRegValueEnumerator::CurrentItem ----------------------
| As the mozRegSubtreeEnumerator counterpart, but allocates an object of       |
| class mozRegistryValue.                                                      |
------------------------------------------------------------------------------*/
nsresult mozRegValueEnumerator::CurrentItem( nsISupports **result ) {
    nsresult rv = NS_OK;
    // Make sure there is a place to put the result.
    if( result ) {
        *result = new mozRegistryValue( mReg, mKey, mEnum );
        if( *result ) {
            (*result)->AddRef();
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*---------------------- mozRegValueEnumerator::advance ------------------------
| Advance mNext to next subkey using NR_RegEnumEntries.  We set mDone if       |
| there are no more entries.                                                   |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegValueEnumerator::advance() {
    char name[MAXREGNAMELEN];
    uint32 len = sizeof name;
    REGINFO info = { sizeof info, 0, 0 };
    mErr = NR_RegEnumEntries( mReg, mKey, &mNext, name, len, &info );
    // See if we ran off end.
    if( mErr == REGERR_NOMORE ) {
        // Remember we've run off end.
        mDone = PR_TRUE;
    }
    // Convert result.
    nsresult rv = regerr2nsresult( mErr );
    return rv;
};


/*--------------------- mozRegistryNode::mozRegistryNode -----------------------
| Store the arguments in the corresponding data members and initialize         |
| the other data members.  We defer the libreg calls till we're asked for      |
| our name.  We use mErr==-1 to indicate we haven't fetched the name yet.      |
------------------------------------------------------------------------------*/
mozRegistryNode::mozRegistryNode( HREG hReg, RKEY key, REGENUM slot )
    : mReg( hReg ), mKey( key ), mEnum( slot ), mErr( -1 ) {
    NS_INIT_REFCNT();
    return;
}


/*-------------------------------- PR_strdup -----------------------------------
| Utility function that does PR_Malloc and copies argument string.  Caller     |
| must do PR_Free.                                                             |
------------------------------------------------------------------------------*/
static char *PR_strdup( const char *in ) {
    char *result = (char*)PR_Malloc( strlen( in ) + 1 );
    if ( result ) {
        strcpy( result, in );
    }
    return result;
}

/*------------------------- mozRegistryNode::GetName ---------------------------
| If we haven't fetched it yet, get the name of the corresponding subkey now,  |
| using NR_RegEnumSubkeys.                                                     |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistryNode::GetName( char **result ) {
    nsresult rv = NS_OK;
    // Make sure there is a place to put the result.
    if( result ) {
        // Test whether we haven't tried to get it yet.
        if( mErr == -1 ) {
            REGENUM temp = mEnum;
            // Get name.
            mErr = NR_RegEnumSubkeys( mReg, mKey, &temp, mName, sizeof mName, PR_FALSE );
        }
        // Convert result from prior libreg call.
        rv = regerr2nsresult( mErr );            
        if( rv == NS_OK || rv == NS_ERROR_REG_NO_MORE ) {
            // worked, return actual result.
            *result = PR_strdup( mName );
            if ( *result ) {
                rv = NS_OK;
            } else {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}



/*-------------------- mozRegistryValue::mozRegistryValue ----------------------
| Implemented the same way as the mozRegistryNode ctor.                        |
------------------------------------------------------------------------------*/
mozRegistryValue::mozRegistryValue( HREG hReg, RKEY key, REGENUM slot )
    : mReg( hReg ), mKey( key ), mEnum( slot ), mErr( -1 ) {
    NS_INIT_REFCNT();
    return;
}

/*------------------------ mozRegistryValue::GetName ---------------------------
| See mozRegistryNode::GetName; we use NR_RegEnumEntries in this case.         |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistryValue::GetName( char **result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        // Ensure we've got the info we need.
        rv = getInfo();            
        if( rv == NS_OK || rv == NS_ERROR_REG_NO_MORE ) {
            // worked, return actual result.
            *result = PR_strdup( mName );
            if ( *result ) {
                rv = NS_OK;
            } else {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*---------------------- mozRegistryValue::GetValueType ------------------------
| We test if we've got the info already.  If not, we git it by calling         |
| getInfo.  We calculate the result by converting the REGINFO type field to    |
| a mozIRegistry::DataType value (using reginfo2DataType).                     |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistryValue::GetValueType( uint32 *result ) {
    nsresult rv = NS_OK;
    // Make sure we have room for th result.
    if( result ) {
        // Make sure we've got the info we need.
        rv = getInfo();
        // Check if it worked.
        if( rv == NS_OK ) {
            // Convert result from REGINFO to mozIRegistry::ValueInfo.
            reginfo2DataType( mInfo, *result );
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*--------------------- mozRegistryValue::GetValueLength -----------------------
| We test if we've got the info already.  If not, we git it by calling         |
| getInfo.  We calculate the result by converting the REGINFO type field to    |
| a mozIRegistry::DataType value (using reginfo2Length).                       |
------------------------------------------------------------------------------*/
NS_IMETHODIMP mozRegistryValue::GetValueLength( uint32 *result ) {
    nsresult rv = NS_OK;
    // Make sure we have room for th result.
    if( result ) {
        // Make sure we've got the info we need.
        rv = getInfo();
        // Check if it worked.
        if( rv == NS_OK ) {
            // Convert result from REGINFO to length.
            reginfo2Length( mInfo, *result );
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*------------------------ mozRegistryValue::getInfo ---------------------------
| Call NR_RegEnumEntries to set the mInfo/mName data members.                  |
------------------------------------------------------------------------------*/
nsresult mozRegistryValue::getInfo() {
    nsresult rv = NS_OK;
    // Test whether we haven't tried to get it yet.
    if( mErr == -1 ) {
        REGENUM temp = mEnum;
        // Get name and info.
        mErr = NR_RegEnumEntries( mReg, mKey, &temp, mName, sizeof mName, &mInfo );
        // Convert result.
        rv = regerr2nsresult( mErr );            
    }
    return rv;
}


mozRegistryFactory::mozRegistryFactory() {
    NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(mozRegistryFactory, kIFactoryIID);

NS_IMETHODIMP
mozRegistryFactory::CreateInstance(nsISupports *aOuter,
                                   const nsIID &aIID,
                                   void **aResult) {
    nsresult rv = NS_OK;
    mozRegistry* newRegistry;

    if(aResult == NULL) {
        return NS_ERROR_NULL_POINTER;
    } else {
        *aResult = NULL;
    }

    if(0 != aOuter) {
        return NS_ERROR_NO_AGGREGATION;
    }

    NS_NEWXPCOM(newRegistry, mozRegistry);

    if(newRegistry == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(newRegistry);
    rv = newRegistry->QueryInterface(aIID, aResult);
    NS_RELEASE(newRegistry);

    return rv;
}

nsresult
mozRegistryFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

// This is a temporary hack; needs work to support dynamic binding
// via nsRepository and support for multiple factories per DLL.
extern "C" NS_EXPORT nsresult
mozRegistry_GetFactory(const nsCID &cid, nsISupports* servMgr, nsIFactory** aFactory ) {
    nsresult rv = NS_OK;

    if( aFactory == 0 ) {
        return NS_ERROR_NULL_POINTER;
    } else {
        *aFactory = 0;
    }

    nsIFactory* inst = new mozRegistryFactory();
    if(0 == inst) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        NS_ADDREF(inst);
        *aFactory = inst;
    }

    return rv;
}
