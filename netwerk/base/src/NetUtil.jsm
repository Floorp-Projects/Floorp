/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4 et filetype=javascript
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Boris Zbarsky <bzbarsky@mit.edu> (original author)
 *    Shawn Wilsher <me@shawnwilsher.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

let EXPORTED_SYMBOLS = [
  "NetUtil",
];

/**
 * Necko utilities
 */

////////////////////////////////////////////////////////////////////////////////
//// Constants

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

////////////////////////////////////////////////////////////////////////////////
//// NetUtil Object

const NetUtil = {
    /**
     * Function to perform simple async copying from aSource (an input stream)
     * to aSink (an output stream).  The copy will happen on some background
     * thread.  Both streams will be closed when the copy completes.
     *
     * @param aSource
     *        The input stream to read from
     * @param aSink
     *        The output stream to write to
     * @param aCallback [optional]
     *        A function that will be called at copy completion with a single
     *        argument: the nsresult status code for the copy operation.
     *
     * @return An nsIRequest representing the copy operation (for example, this
     *         can be used to cancel the copying).  The consumer can ignore the
     *         return value if desired.
     */
    asyncCopy: function NetUtil_asyncCopy(aSource, aSink, aCallback)
    {
        if (!aSource || !aSink) {
            let exception = new Components.Exception(
                "Must have a source and a sink",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
        }

        var sourceBuffered = ioUtil.inputStreamIsBuffered(aSource);
        var sinkBuffered = ioUtil.outputStreamIsBuffered(aSink);

        var ostream = aSink;
        if (!sourceBuffered && !sinkBuffered) {
            // wrap the sink in a buffered stream.
            ostream = Cc["@mozilla.org/network/buffered-output-stream;1"].
                      createInstance(Ci.nsIBufferedOutputStream);
            ostream.init(aSink, 0x8000);
            sinkBuffered = true;
        }

        // make a stream copier
        var copier = Cc["@mozilla.org/network/async-stream-copier;1"].
            createInstance(Ci.nsIAsyncStreamCopier);

        // Initialize the copier.  The 0x8000 should match the size of the
        // buffer our buffered stream is using, for best performance.  If we're
        // not using our own buffered stream, that's ok too.  But maybe we
        // should just use the default net segment size here?
        copier.init(aSource, ostream, null, sourceBuffered, sinkBuffered,
                    0x8000, true, true);

        var observer;
        if (aCallback) {
            observer = {
                onStartRequest: function(aRequest, aContext) {},
                onStopRequest: function(aRequest, aContext, aStatusCode) {
                    aCallback(aStatusCode);
                }
            }
        } else {
            observer = null;
        }

        // start the copying
        copier.asyncCopy(observer, null);
        return copier;
    },

    /**
     * Constructs a new URI for the given spec, character set, and base URI.
     *
     * @param aSpec
     *        The spec for the desired URI.
     * @param aOriginCharset [optional]
     *        The character set for the URI.
     * @param aBaseURI [optional]
     *        The base URI for the spec.
     *
     * @return an nsIURI object.
     */
    newURI: function NetUtil_newURI(aSpec, aOriginCharset, aBaseURI)
    {
        if (!aSpec) {
            let exception = new Components.Exception(
                "Must have a non-null spec",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
        }

        return ioService.newURI(aSpec, aOriginCharset, aBaseURI);
    },
};

////////////////////////////////////////////////////////////////////////////////
//// Initialization

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// Define our lazy getters.
XPCOMUtils.defineLazyServiceGetter(this, "ioUtil", "@mozilla.org/io-util;1",
                                   "nsIIOUtil");
XPCOMUtils.defineLazyServiceGetter(this, "ioService",
                                   "@mozilla.org/network/io-service;1",
                                   "nsIIOService");
