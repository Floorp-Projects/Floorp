/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef ns4xPluginStream_h__
#define ns4xPluginStream_h__

#define _UINT32
#define _INT32

#include "jri.h"                 // XXX should be jni.h
#include "nsplugin.h"
#include "ns4xPluginInstance.h"

////////////////////////////////////////////////////////////////////////

/**
 * A 5.0 wrapper for a 4.x plugin stream.
 */
class ns4xPluginStream : public nsIPluginStream
{
public:

    /**
     * Construct a new 4.x plugin stream associated with the specified
     * instance and stream peer.
     */
    ns4xPluginStream();
    ~ns4xPluginStream();

    NS_DECL_ISUPPORTS

    /**
     * Do internal initialization. This actually calls into the 4.x plugin
     * to create the stream, and may fail (which is why it's separate from
     * the constructor).
     */
    NS_METHOD
    Initialize(ns4xPluginInstance* instance, nsIPluginStreamPeer* peer);

    ////////////////////////////////////////////////////////////////////////
    // nsIPluginStream methods

    // (Corresponds to NPP_Write and NPN_Write.)
    NS_IMETHOD
    Write(const char* buffer, PRUint32 offset, PRUint32 len, PRUint32 *aWriteCount);

    // (Corresponds to NPP_NewStream's stype return parameter.)
    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result);

    // (Corresponds to NPP_StreamAsFile.)
    NS_IMETHOD
    AsFile(const char* fname);

    NS_IMETHOD
    Close(void);

    ////////////////////////////////////////////////////////////////////////
    // Methods specific to ns4xPluginStream

protected:

    /**
     * The plugin instance to which this stream belongs.
     */
    ns4xPluginInstance* fInstance;

    /**
     * The peer associated with this stream.
     */
    nsIPluginStreamPeer* fPeer;

    /**
     * The type of stream, for the peer's use.
     */
    nsPluginStreamType fStreamType;

    /**
     * The 4.x-style structure used to contain stream information.
     * This is what actually gets used to communicate with the plugin.
     */
    NPStream fNPStream;

    /** 
     * Set to <b>TRUE</b> if the peer implements
     * <b>NPISeekablPluginStreamPeer</b>.
     */
    PRBool fSeekable;

    /** 
     * Tracks the position in the content that is being
     * read. 4.x-style plugins expect to be told the offset in the
     * buffer that they should read <i>to</i>, even though it's always
     * done serially.
     */
    PRUint32 fPosition;
};


#endif // ns4xPluginStream_h__
