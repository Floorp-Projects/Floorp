/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
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
const Cu = Components.utils;

const PR_UINT32_MAX = 0xffffffff;

////////////////////////////////////////////////////////////////////////////////
//// NetUtil Object

this.NetUtil = {
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
     * Asynchronously opens a source and fetches the response.  A source can be
     * an nsIURI, nsIFile, string spec, nsIChannel, or nsIInputStream.  The
     * provided callback will get an input stream containing the response, the
     * result code, and a reference to the request.
     *
     * @param aSource
     *        The nsIURI, nsIFile, string spec, nsIChannel, or nsIInputStream
     *        to open.
     * @param aCallback
     *        The callback function that will be notified upon completion.  It
     *        will get two arguments:
     *        1) An nsIInputStream containing the data from aSource, if any.
     *        2) The status code from opening the source.
     *        3) Reference to the nsIRequest.
     */
    asyncFetch: function NetUtil_asyncOpen(aSource, aCallback)
    {
        if (!aSource || !aCallback) {
            let exception = new Components.Exception(
                "Must have a source and a callback",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
        }

        // Create a pipe that will create our output stream that we can use once
        // we have gotten all the data.
        let pipe = Cc["@mozilla.org/pipe;1"].
                   createInstance(Ci.nsIPipe);
        pipe.init(true, true, 0, PR_UINT32_MAX, null);

        // Create a listener that will give data to the pipe's output stream.
        let listener = Cc["@mozilla.org/network/simple-stream-listener;1"].
                       createInstance(Ci.nsISimpleStreamListener);
        listener.init(pipe.outputStream, {
            onStartRequest: function(aRequest, aContext) {},
            onStopRequest: function(aRequest, aContext, aStatusCode) {
                pipe.outputStream.close();
                aCallback(pipe.inputStream, aStatusCode, aRequest);
            }
        });

        // Input streams are handled slightly differently from everything else.
        if (aSource instanceof Ci.nsIInputStream) {
            let pump = Cc["@mozilla.org/network/input-stream-pump;1"].
                       createInstance(Ci.nsIInputStreamPump);
            pump.init(aSource, -1, -1, 0, 0, true);
            pump.asyncRead(listener, null);
            return;
        }

        let channel = aSource;
        if (!(channel instanceof Ci.nsIChannel)) {
            channel = this.newChannel(aSource);
        }

        channel.asyncOpen(listener, null);
    },

    /**
     * Constructs a new URI for the given spec, character set, and base URI, or
     * an nsIFile.
     *
     * @param aTarget
     *        The string spec for the desired URI or an nsIFile.
     * @param aOriginCharset [optional]
     *        The character set for the URI.  Only used if aTarget is not an
     *        nsIFile.
     * @param aBaseURI [optional]
     *        The base URI for the spec.  Only used if aTarget is not an
     *        nsIFile.
     *
     * @return an nsIURI object.
     */
    newURI: function NetUtil_newURI(aTarget, aOriginCharset, aBaseURI)
    {
        if (!aTarget) {
            let exception = new Components.Exception(
                "Must have a non-null string spec or nsIFile object",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
        }

        if (aTarget instanceof Ci.nsIFile) {
            return this.ioService.newFileURI(aTarget);
        }

        return this.ioService.newURI(aTarget, aOriginCharset, aBaseURI);
    },

    /**
     * Constructs a new channel for the given spec, character set, and base URI,
     * or nsIURI, or nsIFile.
     *
     * @param aWhatToLoad
     *        The string spec for the desired URI, an nsIURI, or an nsIFile.
     * @param aOriginCharset [optional]
     *        The character set for the URI.  Only used if aWhatToLoad is a
     *        string.
     * @param aBaseURI [optional]
     *        The base URI for the spec.  Only used if aWhatToLoad is a string.
     *
     * @return an nsIChannel object.
     */
    newChannel: function NetUtil_newChannel(aWhatToLoad, aOriginCharset,
                                            aBaseURI)
    {
        if (!aWhatToLoad) {
            let exception = new Components.Exception(
                "Must have a non-null string spec, nsIURI, or nsIFile object",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
        }

        let uri = aWhatToLoad;
        if (!(aWhatToLoad instanceof Ci.nsIURI)) {
            // We either have a string or an nsIFile that we'll need a URI for.
            uri = this.newURI(aWhatToLoad, aOriginCharset, aBaseURI);
        }

        return this.ioService.newChannelFromURI(uri);
    },

    /**
     * Reads aCount bytes from aInputStream into a string.
     *
     * @param aInputStream
     *        The input stream to read from.
     * @param aCount
     *        The number of bytes to read from the stream.
     * @param aOptions [optional]
     *        charset
     *          The character encoding of stream data.
     *        replacement
     *          The character to replace unknown byte sequences.
     *          If unset, it causes an exceptions to be thrown.
     *
     * @return the bytes from the input stream in string form.
     *
     * @throws NS_ERROR_INVALID_ARG if aInputStream is not an nsIInputStream.
     * @throws NS_BASE_STREAM_WOULD_BLOCK if reading from aInputStream would
     *         block the calling thread (non-blocking mode only).
     * @throws NS_ERROR_FAILURE if there are not enough bytes available to read
     *         aCount amount of data.
     * @throws NS_ERROR_ILLEGAL_INPUT if aInputStream has invalid sequences
     */
    readInputStreamToString: function NetUtil_readInputStreamToString(aInputStream,
                                                                      aCount,
                                                                      aOptions)
    {
        if (!(aInputStream instanceof Ci.nsIInputStream)) {
            let exception = new Components.Exception(
                "First argument should be an nsIInputStream",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
        }

        if (!aCount) {
            let exception = new Components.Exception(
                "Non-zero amount of bytes must be specified",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
        }

        if (aOptions && "charset" in aOptions) {
          let cis = Cc["@mozilla.org/intl/converter-input-stream;1"].
                    createInstance(Ci.nsIConverterInputStream);
          try {
            // When replacement is set, the character that is unknown sequence 
            // replaces with aOptions.replacement character.
            if (!("replacement" in aOptions)) {
              // aOptions.replacement isn't set.
              // If input stream has unknown sequences for aOptions.charset,
              // throw NS_ERROR_ILLEGAL_INPUT.
              aOptions.replacement = 0;
            }

            cis.init(aInputStream, aOptions.charset, aCount,
                     aOptions.replacement);
            let str = {};
            cis.readString(-1, str);
            cis.close();
            return str.value;
          }
          catch (e) {
            // Adjust the stack so it throws at the caller's location.
            throw new Components.Exception(e.message, e.result,
                                           Components.stack.caller, e.data);
          }
        }

        let sis = Cc["@mozilla.org/scriptableinputstream;1"].
                  createInstance(Ci.nsIScriptableInputStream);
        sis.init(aInputStream);
        try {
            return sis.readBytes(aCount);
        }
        catch (e) {
            // Adjust the stack so it throws at the caller's location.
            throw new Components.Exception(e.message, e.result,
                                           Components.stack.caller, e.data);
        }
    },

    /**
     * Returns a reference to nsIIOService.
     *
     * @return a reference to nsIIOService.
     */
    get ioService()
    {
        delete this.ioService;
        return this.ioService = Cc["@mozilla.org/network/io-service;1"].
                                getService(Ci.nsIIOService);
    },
};

////////////////////////////////////////////////////////////////////////////////
//// Initialization

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// Define our lazy getters.
XPCOMUtils.defineLazyServiceGetter(this, "ioUtil", "@mozilla.org/io-util;1",
                                   "nsIIOUtil");
