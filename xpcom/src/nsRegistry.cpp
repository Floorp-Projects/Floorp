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

#include "nsIRegistry.h"
#include "nsIEnumerator.h"
#include "NSReg.h"
#include "prmem.h"

/*-------------------------------- nsRegistry ----------------------------------
| This class implements the nsIRegistry interface using the functions          |
| provided by libreg (as declared in mozilla/modules/libreg/include/NSReg.h).  |
|                                                                              |
| Since that interface is designed to match the libreg function, this class    |
| is implemented with each member function being a simple wrapper for the      |
| corresponding libreg function.                                               |
------------------------------------------------------------------------------*/
struct nsRegistry : public nsIRegistry {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIRegistry interface functions.
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
    nsRegistry();
    virtual ~nsRegistry();

protected:
    HREG   mReg; // Registry handle.
    REGERR mErr; // Last libreg error code.
}; // nsRegistry


#include "nsIFactory.h"
/*----------------------------- nsRegistryFactory ------------------------------
| Class factory for nsRegistry objects.                                        |
------------------------------------------------------------------------------*/
struct nsRegistryFactory : public nsIFactory {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *,const nsIID &,void **);
    NS_IMETHOD LockFactory(PRBool aLock);

    // ctor
    nsRegistryFactory();
};


/*--------------------------- nsRegSubtreeEnumerator ---------------------------
| This class implements the nsIEnumerator interface and is used to implement   |
| the nsRegistry EnumerateSubtrees and EnumerateAllSubtrees functions.         |
------------------------------------------------------------------------------*/
struct nsRegSubtreeEnumerator : public nsIEnumerator {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIEnumerator interface functions.
    NS_IMETHOD First();
    NS_IMETHOD Next();
    NS_IMETHOD CurrentItem(nsISupports **aItem);
    NS_IMETHOD IsDone();

    // ctor/dtor
    nsRegSubtreeEnumerator( HREG hReg, RKEY rKey, PRBool all );

protected:
    NS_IMETHOD advance(); // Implementation file; does appropriate NR_RegEnum call.
    HREG    mReg;   // Handle to registry we're affiliated with.
    RKEY    mKey;   // Base key being enumerated.
    REGENUM mEnum;  // Corresponding libreg "enumerator".
    REGENUM mNext;  // Lookahead value.
    uint32  mStyle; // Style (indicates all or some);
    PRBool  mDone;  // Done flag.
    REGERR  mErr;   // Last libreg error code.
}; // nsRegSubtreeEnumerator


/*--------------------------- nsRegValueEnumerator -----------------------------
| This class is a variation on nsRegSubtreeEnumerator that allocates           |
| nsRegistryValue objects rather than nsRegistryNode objects.  It also         |
| overrides certain functions to make sure the "value" oriented libreg         |
| functions used rather than the subtree oriented ones.                        |
------------------------------------------------------------------------------*/
struct nsRegValueEnumerator : public nsRegSubtreeEnumerator {
    // Override CurrentItem to allocate nsRegistryValue objects.
    NS_IMETHOD CurrentItem( nsISupports **result );

    // Override advance() to use proper NR_RegEnumEntries.
    NS_IMETHOD advance();

    // ctor/dtor
    nsRegValueEnumerator( HREG hReg, RKEY rKey );
}; // nsRegValueEnumerator

/*------------------------------ nsRegistryNode --------------------------------
| This class implements the nsIRegistryNode interface.  Instances are         |
| allocated by nsRegSubtreeEnumerator::CurrentItem.                           |
------------------------------------------------------------------------------*/
struct nsRegistryNode : public nsIRegistryNode {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIRegistryNode interface functions.
    NS_IMETHOD GetName( char **result );

    // ctor
    nsRegistryNode( HREG hReg, RKEY key, REGENUM slot );
    
protected:
    HREG    mReg;  // Handle to registry this node is part of.
    RKEY    mKey;  // Key this node is under.
    REGENUM mEnum; // Copy of corresponding content of parent enumerator.
    REGERR  mErr;  // Last libreg error code.
    char    mName[MAXREGPATHLEN]; // Buffer to hold name.
}; // nsRegistryNode


/*------------------------------ nsRegistryValue -------------------------------
| This class implements the nsIRegistryValue interface.  Instances are         |
| allocated by nsRegValueEnumerator::CurrentItem.                              |
------------------------------------------------------------------------------*/
struct nsRegistryValue : public nsIRegistryValue {
    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIRegistryValue interface functions.
    NS_IMETHOD GetName( char **result );
    NS_IMETHOD GetValueType( uint32 *result );
    NS_IMETHOD GetValueLength( uint32 *result );

    // ctor
    nsRegistryValue( HREG hReg, RKEY key, REGENUM slot );

protected:
    nsresult getInfo(); // Get registry info.
    HREG    mReg;  // Handle to registry this node is part of.
    RKEY    mKey;  // Key this node is under.
    REGENUM mEnum; // Copy of corresponding content of parent enumerator.
    REGERR  mErr;  // Last libreg error code.
    REGINFO mInfo; // Value info.
    char    mName[MAXREGNAMELEN]; // Buffer to hold name.
}; // nsRegistryValue


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
| the corresponding nsIRegistry::DataType value.                              |
------------------------------------------------------------------------------*/
static void reginfo2DataType( const REGINFO &in, uint32 &out ) {
    // Transfer information, based on entry type.
    switch( in.entryType ) {
        case REGTYPE_ENTRY_STRING_UTF:
            out = nsIRegistry::String;
            //out.length = in.entryLength;
            break;

        case REGTYPE_ENTRY_INT32_ARRAY:
            out = nsIRegistry::Int32;
            // Convert length in bytes to array dimension.
            //out.length = in.entryLength / sizeof(int32);
            break;

        case REGTYPE_ENTRY_BYTES:
            out = nsIRegistry::Bytes;
            //out.length = in.entryLength;
            break;

        case REGTYPE_ENTRY_FILE:
            out = nsIRegistry::File;
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
static NS_DEFINE_IID(kIRegistryIID,      NS_IREGISTRY_IID);
static NS_DEFINE_IID(kIRegistryNodeIID,  NS_IREGISTRYNODE_IID);
static NS_DEFINE_IID(kIRegistryValueIID, NS_IREGISTRYVALUE_IID);

static NS_DEFINE_IID(kIEnumeratorIID, NS_IENUMERATOR_IID);

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

/*------------------------ nsISupports Implementation --------------------------
| This code generates the implementation of the nsISupports member functions   |
| for each class implemented in this file.                                     |
------------------------------------------------------------------------------*/
NS_IMPL_ISUPPORTS( nsRegistry,             kIRegistryIID      );
NS_IMPL_ISUPPORTS( nsRegSubtreeEnumerator, kIEnumeratorIID    );
NS_IMPL_ISUPPORTS( nsRegistryNode,         kIRegistryNodeIID  );
NS_IMPL_ISUPPORTS( nsRegistryValue,        kIRegistryValueIID );

/*-------------------------- nsRegistry::nsRegistry ----------------------------
| Vanilla nsRegistry constructor.  The first time called, it does              |
| NR_StartupRegistry.                                                          |
------------------------------------------------------------------------------*/
nsRegistry::nsRegistry() 
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

/*------------------------- nsRegistry::~nsRegistry ----------------------------
| The dtor closes the registry file(if open).                                  |
------------------------------------------------------------------------------*/
nsRegistry::~nsRegistry() {
    if( mReg ) {
        Close();
    }
    return;
}

/*----------------------------- nsRegistry::Open -------------------------------
| If the argument is null, delegate to OpenDefault, else open the registry     |
| file.  We first check to see if a registry file is already open and close    |
| it if so.                                                                    |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::Open( const char *regFile ) {
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

/*-------------------------- nsRegistry::OpenDefault ---------------------------
| Open the "default" registry; in the case of this libreg-based implementation |
| that is done by passing a null file name pointer to NR_RegOpen.              |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::OpenDefault() {
    // Ensure existing registry is closed.
    Close();
    // Open default registry.
    mErr = NR_RegOpen( 0, &mReg );
    // Convert the result.
    return regerr2nsresult( mErr );
}

/*----------------------------- nsRegistry::Close ------------------------------
| Tests the mReg handle and if non-null, closes the registry via NR_RegClose.  |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::Close() {
    mErr = REGERR_OK;
    if( mReg ) {
        mErr = NR_RegClose( mReg );
        mReg = 0;
    }
    return regerr2nsresult( mErr );
}

/*--------------------------- nsRegistry::GetString ----------------------------
| First, look for the entry using GetValueInfo.  If found, and it's a string,  |
| allocate space for it and fetch the value.                                   |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::GetString( Key baseKey, const char *path, char **result ) {
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

/*--------------------------- nsRegistry::SetString ----------------------------
| Simply sets the registry contents using NR_RegSetEntryString.                |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::SetString( Key baseKey, const char *path, const char *value ) {
    nsresult rv = NS_OK;
    // Set the contents.
    mErr = NR_RegSetEntryString( mReg,(RKEY)baseKey,(char*)path,(char*)value );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*---------------------------- nsRegistry::GetInt ------------------------------
| This function is just shorthand for fetching a 1-element int32 array.  We    |
| implement it "manually" using NR_RegGetEntry(versus calling GetIntArray)    |
| to save allocating a copy of the result.                                     |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::GetInt( Key baseKey, const char *path, int32 *result ) {
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

/*---------------------------- nsRegistry::SetInt ------------------------------
| Write out the value as a one-element int32 array, using NR_RegSetEntry.      |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::SetInt( Key baseKey, const char *path, int32 value ) {
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

/*--------------------------- nsRegistry::GetBytes -----------------------------
| Get the registry contents at specified location using NR_RegGetEntry.        |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::GetBytes( Key baseKey, const char *path, void **result, uint32 *len ) {
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

/*--------------------------- nsRegistry::SetBytes -----------------------------
| Set the contents at the specified registry location, using NR_RegSetEntry.   |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::SetBytes( Key baseKey, const char *path, void *value, uint32 len ) {
    nsresult rv = NS_OK;
    // Set contents.
    mErr = NR_RegSetEntry( mReg,(RKEY)baseKey,(char*)path,
                           REGTYPE_ENTRY_BYTES, value, len );
    // Convert result;
    rv = regerr2nsresult( mErr );
    return rv;
}

/*-------------------------- nsRegistry::GetIntArray ---------------------------
| Find out about the entry using GetValueInfo.  We check the type and then     |
| use NR_RegGetEntry to get the actual array.  We have to convert from the     |
| array dimension to number of bytes in the process.                           |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::GetIntArray( Key baseKey, const char *path, int32 **result, uint32 *len ) {
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

/*-------------------------- nsRegistry::SetIntArray ---------------------------
| Store the given integer array at the given point in the registry.  The       |
| length given is the size of the array, we have to convert that to the        |
| size in bytes in order to use NR_RegSetEntry.                                |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::SetIntArray( Key baseKey, const char *path, const int32 *value, uint32 len ) {
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

/*-------------------------- nsRegistry::AddSubtree ----------------------------
| Add a new registry subkey with the specified name, using NR_RegAddKey.       |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::AddSubtree( Key baseKey, const char *path, Key *result ) {
    nsresult rv = NS_OK;
    // Add the subkey.
    mErr = NR_RegAddKey( mReg,(RKEY)baseKey,(char*)path,(RKEY*)result );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*------------------------- nsRegistry::RemoveSubtree --------------------------
| Deletes the subtree at a given location using NR_RegDeleteKey.               |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::RemoveSubtree( Key baseKey, const char *path ) {
    nsresult rv = NS_OK;
    // Delete the subkey.
    mErr = NR_RegDeleteKey( mReg,(RKEY)baseKey,(char*)path );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*-------------------------- nsRegistry::GetSubtree ----------------------------
| Returns a nsIRegistry::Key(RKEY) for a given key/path.  The key is           |
| obtained using NR_RegGetKey.                                                 |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::GetSubtree( Key baseKey, const char *path, Key *result ) {
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

/*----------------------- nsRegistry::EnumerateSubtrees ------------------------
| Allocate a nsRegSubtreeEnumerator object and return it to the caller.        |
| We construct the enumerator using the registry handle from this registry     |
| object, the user-specified registry key, and indicate that we don't want     |
| to recurse down subtrees.  No libreg functions are invoked at this point     |
|(that will happen when the enumerator member functions are called).          |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::EnumerateSubtrees( Key baseKey, nsIEnumerator **result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        *result = new nsRegSubtreeEnumerator( mReg,(RKEY)baseKey, PR_FALSE );
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

/*--------------------- nsRegistry::EnumerateAllSubtrees -----------------------
| Same as EnumerateSubtrees but we pass PR_TRUE to request that the            |
| enumerator object descend subtrees when it is used.                          |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::EnumerateAllSubtrees( Key baseKey, nsIEnumerator **result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        *result = new nsRegSubtreeEnumerator( mReg,(RKEY)baseKey, PR_TRUE );
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

/*------------------------- nsRegistry::GetValueType ---------------------------
| Gets the type from the registry using the NR_GetEntryInfo libreg API.        |
| The result is transferred to the uint32 value passed in (with conversion     |
| to the appropriate nsIRegistry::DataType value).                             |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::GetValueType( Key baseKey, const char *path, uint32 *result ) {
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

/*------------------------ nsRegistry::GetValueLength --------------------------
| Gets the registry value info via NR_RegGetEntryInfo.  The length is          |
| converted to the proper "units" via reginfo2Length.                          |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::GetValueLength( Key baseKey, const char *path, uint32 *result ) {
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

/*------------------------ nsRegistry::EnumerateValues -------------------------
| Allocates and returns an instance of nsRegValueEnumerator constructed in     |
| a similar fashion as the nsRegSubtreeEnumerator is allocated/returned by     |
| EnumerateSubtrees.                                                           |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::EnumerateValues( Key baseKey, nsIEnumerator **result ) {
    nsresult rv = NS_OK;
    // Make sure we have a place to put the result.
    if( result ) {
        *result = new nsRegValueEnumerator( mReg,(RKEY)baseKey );
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

/*---------------------- nsRegistry::GetCurrentUserName ------------------------
| Simple wrapper for NR_RegGetUsername.                                        |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::GetCurrentUserName( char **result ) {
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

/*---------------------- nsRegistry::SetCurrentUserName ------------------------
| Simple wrapper for NR_RegSetUsername.                                        |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::SetCurrentUserName( const char *name ) {
    nsresult rv = NS_OK;
    // Set the user name.
    mErr = NR_RegSetUsername( name );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*----------------------------- nsRegistry::Pack -------------------------------
| Simple wrapper for NR_RegPack.  We don't set up any callback.                |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistry::Pack() {
    nsresult rv = NS_OK;
    // Pack the registry.
    mErr = NR_RegPack( mReg, 0, 0 );
    // Convert result.
    rv = regerr2nsresult( mErr );
    return rv;
}

/*-------------- nsRegSubtreeEnumerator::nsRegSubtreeEnumerator ----------------
| The ctor simply stashes all the information that will be needed to enumerate |
| the subkeys.                                                                 |
------------------------------------------------------------------------------*/
nsRegSubtreeEnumerator::nsRegSubtreeEnumerator( HREG hReg, RKEY rKey, PRBool all )
    : mReg( hReg ), mKey( rKey ), mEnum( 0 ), mNext( 0 ),
      mStyle( all ? REGENUM_DESCEND : 0 ), mDone( PR_FALSE ), mErr( -1 ) {
    NS_INIT_REFCNT();
    return;
}

/*----------------------- nsRegSubtreeEnumerator::First ------------------------
| Set mEnum to 0; this will cause the next NR_RegEnum call to go to            |
| the beginning.  We then do a Next() call in order to do a "lookahead" to     |
| properly detect an empty list (i.e., set the mDone flag).                    |
------------------------------------------------------------------------------*/
NS_IMETHODIMP
nsRegSubtreeEnumerator::First() {
    nsresult rv = NS_OK;
    // Reset "done" flag.
    mDone = PR_FALSE;
    // Go to beginning.
    mEnum = mNext = 0;
    // Lookahead so mDone flag gets set for empty list.
    rv = Next();
    return rv;
}

/*----------------------- nsRegSubtreeEnumerator::Next -------------------------
| First, we check if we've already advanced to the end by checking the  mDone  |
| flag.                                                                        |
|                                                                              |
| We advance mEnum to the next enumeration value which is in the mNext         |
| lookahead buffer.  We must then call advance to lookahead and properly set   |
| the isDone flag.                                                             |
------------------------------------------------------------------------------*/
NS_IMETHODIMP
nsRegSubtreeEnumerator::Next() {
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

/*---------------------- nsRegSubtreeEnumerator::advance -----------------------
| Advance mNext to next subkey using NR_RegEnumSubkeys.  We set mDone if       |
| there are no more subkeys.                                                   |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegSubtreeEnumerator::advance() {
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

/*-------------------- nsRegSubtreeEnumerator::CurrentItem ---------------------
| Allocates and returns a new instance of class nsRegistryNode.  The node      |
| object will hold the curent mEnum value so it can obtain its name from       |
| the registry when asked.                                                     |
------------------------------------------------------------------------------*/
NS_IMETHODIMP
nsRegSubtreeEnumerator::CurrentItem( nsISupports **result) {
    nsresult rv = NS_OK;
    // Make sure there is a place to put the result.
    if( result ) {
        *result = new nsRegistryNode( mReg, mKey, mEnum );
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

/*---------------------- nsRegSubtreeEnumerator::IsDone ------------------------
| Simply return mDone.                                                         |
------------------------------------------------------------------------------*/
NS_IMETHODIMP
nsRegSubtreeEnumerator::IsDone() {
    nsresult rv = mDone;
    return rv;
}


/*---------------- nsRegValueEnumerator::nsRegValueEnumerator ------------------
| Delegates everything to the base class constructor.                          |
------------------------------------------------------------------------------*/
nsRegValueEnumerator::nsRegValueEnumerator( HREG hReg, RKEY rKey )
    : nsRegSubtreeEnumerator( hReg, rKey, PR_FALSE ) {
    return;
}


/*--------------------- nsRegValueEnumerator::CurrentItem ----------------------
| As the nsRegSubtreeEnumerator counterpart, but allocates an object of        |
| class nsRegistryValue.                                                       |
------------------------------------------------------------------------------*/
NS_IMETHODIMP
nsRegValueEnumerator::CurrentItem( nsISupports **result ) {
    nsresult rv = NS_OK;
    // Make sure there is a place to put the result.
    if( result ) {
        *result = new nsRegistryValue( mReg, mKey, mEnum );
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

/*----------------------- nsRegValueEnumerator::advance ------------------------
| Advance mNext to next subkey using NR_RegEnumEntries.  We set mDone if       |
| there are no more entries.                                                   |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegValueEnumerator::advance() {
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


/*---------------------- nsRegistryNode::nsRegistryNode ------------------------
| Store the arguments in the corresponding data members and initialize         |
| the other data members.  We defer the libreg calls till we're asked for      |
| our name.  We use mErr==-1 to indicate we haven't fetched the name yet.      |
------------------------------------------------------------------------------*/
nsRegistryNode::nsRegistryNode( HREG hReg, RKEY key, REGENUM slot )
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

/*-------------------------- nsRegistryNode::GetName ---------------------------
| If we haven't fetched it yet, get the name of the corresponding subkey now,  |
| using NR_RegEnumSubkeys.                                                     |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistryNode::GetName( char **result ) {
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



/*--------------------- nsRegistryValue::nsRegistryValue -----------------------
| Implemented the same way as the nsRegistryNode ctor.                         |
------------------------------------------------------------------------------*/
nsRegistryValue::nsRegistryValue( HREG hReg, RKEY key, REGENUM slot )
    : mReg( hReg ), mKey( key ), mEnum( slot ), mErr( -1 ) {
    NS_INIT_REFCNT();
    return;
}

/*------------------------- nsRegistryValue::GetName ---------------------------
| See nsRegistryNode::GetName; we use NR_RegEnumEntries in this case.         |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistryValue::GetName( char **result ) {
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

/*----------------------- nsRegistryValue::GetValueType ------------------------
| We test if we've got the info already.  If not, we git it by calling         |
| getInfo.  We calculate the result by converting the REGINFO type field to    |
| a nsIRegistry::DataType value (using reginfo2DataType).                      |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistryValue::GetValueType( uint32 *result ) {
    nsresult rv = NS_OK;
    // Make sure we have room for th result.
    if( result ) {
        // Make sure we've got the info we need.
        rv = getInfo();
        // Check if it worked.
        if( rv == NS_OK ) {
            // Convert result from REGINFO to nsIRegistry::ValueInfo.
            reginfo2DataType( mInfo, *result );
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

/*---------------------- nsRegistryValue::GetValueLength -----------------------
| We test if we've got the info already.  If not, we git it by calling         |
| getInfo.  We calculate the result by converting the REGINFO type field to    |
| a nsIRegistry::DataType value (using reginfo2Length).                        |
------------------------------------------------------------------------------*/
NS_IMETHODIMP nsRegistryValue::GetValueLength( uint32 *result ) {
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

/*------------------------- nsRegistryValue::getInfo ---------------------------
| Call NR_RegEnumEntries to set the mInfo/mName data members.                  |
------------------------------------------------------------------------------*/
nsresult nsRegistryValue::getInfo() {
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


nsRegistryFactory::nsRegistryFactory() {
    NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(nsRegistryFactory, kIFactoryIID);

NS_IMETHODIMP
nsRegistryFactory::CreateInstance(nsISupports *aOuter,
                                   const nsIID &aIID,
                                   void **aResult) {
    nsresult rv = NS_OK;
    nsRegistry* newRegistry;

    if(aResult == NULL) {
        return NS_ERROR_NULL_POINTER;
    } else {
        *aResult = NULL;
    }

    if(0 != aOuter) {
        return NS_ERROR_NO_AGGREGATION;
    }

    NS_NEWXPCOM(newRegistry, nsRegistry);

    if(newRegistry == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(newRegistry);
    rv = newRegistry->QueryInterface(aIID, aResult);
    NS_RELEASE(newRegistry);

    return rv;
}

nsresult
nsRegistryFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

// This is a temporary hack; needs work to support dynamic binding
// via nsComponentManager and support for multiple factories per DLL.
extern "C" NS_EXPORT nsresult
nsRegistry_GetFactory(const nsCID &cid, nsISupports* servMgr, nsIFactory** aFactory ) {
    nsresult rv = NS_OK;

    if( aFactory == 0 ) {
        return NS_ERROR_NULL_POINTER;
    } else {
        *aFactory = 0;
    }

    nsIFactory* inst = new nsRegistryFactory();
    if(0 == inst) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        NS_ADDREF(inst);
        *aFactory = inst;
    }

    return rv;
}
