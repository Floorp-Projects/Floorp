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
#include "nsStreamXferOp.h"

// ctor - save arguments in data members.
nsStreamXfer::nsStreamXferOp( const nsString &source, const nsString &target ) 
    : mSource( source ),
      mTarget( target ),
      mObserver( 0 ) {
    // Properly initialize refcnt.
    NS_INIT_REFCNT();
}

// dtor - standard
nsStreamXfer::~nsStreamXferOp() {
}

NS_IMETHODIMP
nsStreamXferOp::GetSource( char**result ) {
    nsresult rv = NS_OK;

    if ( result ) {
        *result = mSource.ToNewCString();
        if ( !*result ) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsStreamXferOp::GetTarget( char**result ) {
    nsresult rv = NS_OK;

    if ( result ) {
        *result = mTarget.ToNewCString();
        if ( !*result ) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsStreamXferOp::GetObserver( nsIObserver**result ) {
    nsresult rv = NS_OK;

    if ( result ) {
        *result = mObserver;
        NS_ADDREF( *result );
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsStreamXferOp::SetObserver( nsIObserver*aObserver ) {
    nsresult rv = NS_OK;

    mObserver = aObserver;

    return rv;
}

NS_IMETHODIMP
nsStreamXferOp::Start( void ) {
    nsresult rv = NS_OK;

    return rv;
}

NS_IMETHODIMP
nsStreamXferOp::Stop( void ) {
    nsresult rv = NS_OK;

    return rv;
}

// Generate standard implementation of AddRef/Release for nsStreamXferOp
NS_IMPL_ADDREF( nsStreamXferOp )
NS_IMPL_RELEASE( nsStreamXferOp )

