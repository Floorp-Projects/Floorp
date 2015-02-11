/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

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
    asyncCopy: function NetUtil_asyncCopy(aSource, aSink,
                                          aCallback = null)
    {
        if (!aSource || !aSink) {
            let exception = new Components.Exception(
                "Must have a source and a sink",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
        }

        // make a stream copier
        var copier = Cc["@mozilla.org/network/async-stream-copier;1"].
            createInstance(Ci.nsIAsyncStreamCopier2);
        copier.init(aSource, aSink,
                    null /* Default event target */,
                    0 /* Default length */,
                    true, true /* Auto-close */);

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
        copier.QueryInterface(Ci.nsIAsyncStreamCopier).asyncCopy(observer, null);
        return copier;
    },

    /**
     * Asynchronously opens a source and fetches the response.  While the fetch
     * is asynchronous, I/O may happen on the main thread.  When reading from
     * a local file, prefer using "OS.File" methods instead.
     *
     * @param aSource
     *        This argument can be one of the following:
     *         - An options object that will be passed to NetUtil.newChannel.
     *         - An existing nsIChannel.
     *         - An existing nsIInputStream.
     *        Using an nsIURI, nsIFile, or string spec directly is deprecated.
     * @param aCallback
     *        The callback function that will be notified upon completion.  It
     *        will get these arguments:
     *        1) An nsIInputStream containing the data from aSource, if any.
     *        2) The status code from opening the source.
     *        3) Reference to the nsIRequest.
     */
    asyncFetch: function NetUtil_asyncFetch(aSource, aCallback)
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

        try {
            channel.asyncOpen(listener, null);
        }
        catch (e) {
            let exception = new Components.Exception(
                "Failed to open input source '" + channel.originalURI.spec + "'",
                e.result,
                Components.stack.caller,
                aSource,
                e
            );
            throw exception;
        }
    },

    /**
     * @deprecated Use asyncFecth({ ...options... }, callback) instead.
     */
    asyncFetch2: function NetUtil_asyncFetch2(aSource,
                                              aCallback,
                                              aLoadingNode,
                                              aLoadingPrincipal,
                                              aTriggeringPrincipal,
                                              aSecurityFlags,
                                              aContentPolicyType)
    {
        if (!aSource || !aCallback) {
            let exception = new Components.Exception(
                "Must have a source and a callback",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
        }

        // if aSource is an instance of an nsIChannel, all the individual
        // args to create a loadInfo must be "undefined".
        if (aSource instanceof Ci.nsIChannel) {
          if ((typeof aLoadingNode != "undefined") ||
              (typeof aLoadingPrincipal != "undefined") ||
              (typeof aTriggeringPrincipal != "undefined") ||
              (typeof aSecurityFlags != "undefined") ||
              (typeof aContentPolicyType != "undefined")) {

            let exception = new Components.Exception(
                "LoadInfo arguments must be undefined",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
            throw exception;
          }
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
            channel = this.newChannel2(aSource,
                                       "",   // aOriginCharset
                                       null, // aBaseURI
                                       aLoadingNode,
                                       aLoadingPrincipal,
                                       aTriggeringPrincipal,
                                       aSecurityFlags,
                                       aContentPolicyType);

        }

        try {
            channel.asyncOpen(listener, null);
        }
        catch (e) {
            let exception = new Components.Exception(
                "Failed to open input source '" + channel.originalURI.spec + "'",
                e.result,
                Components.stack.caller,
                aSource,
                e
            );
            throw exception;
        }
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
     * Constructs a new channel for the given source.
     *
     * Keep in mind that URIs coming from a webpage should *never* use the
     * systemPrincipal as the loadingPrincipal.
     *
     * @param aWhatToLoad
     *        This argument used to be a string spec for the desired URI, an
     *        nsIURI, or an nsIFile.  Now it should be an options object with
     *        the following properties:
     *        {
     *          uri:
     *            The full URI spec string or nsIURI to create the channel for.
     *            Note that this cannot be an nsIFile and you cannot specify a
     *            non-default charset or base URI.  Call NetUtil.newURI first if
     *            you need to construct an URI using those options.
     *          loadingNode:
     *            The loadingDocument of the channel.
     *            The element or document where the result of this request will
     *            be used.  This is the document/element that will get access to
     *            the result of this request.  For example for an image load,
     *            it's the document in which the image will be loaded.  And for
     *            a CSS stylesheet it's the document whose rendering will be
     *            affected by the stylesheet.
     *            If possible, pass in the element which is performing the load.
     *            But if the load is coming from a JS API (such as
     *            XMLHttpRequest) or if the load might be coalesced across
     *            multiple elements (such as for <img>) then pass in the
     *            Document node instead.
     *            For loads that are not related to any document, such as loads
     *            coming from addons or internal browser features, omit this
     *            property and specify a loadingPrincipal or
     *            loadUsingSystemPrincipal instead.
     *          loadingPrincipal:
     *            The loadingPrincipal of the channel.
     *            The principal of the document where the result of this request
     *            will be used.
     *            This is generally the principal of the loadingNode.  However
     *            for loads where loadingNode is omitted this argument still
     *            needs to be passed.  For example for loads from a WebWorker,
     *            pass the principal of that worker.  For loads from an addon or
     *            from internal browser features, pass the system principal.
     *            This principal should almost always be the system principal if
     *            loadingNode is omitted, in which case you can use the
     *            useSystemPrincipal property.  The only exception to this is
     *            for loads from WebWorkers since they don't have any nodes to
     *            be passed as loadingNode.
     *            Please note, loadingPrincipal is *not* the principal of the
     *            resource being loaded, but rather the principal of the context
     *            where the resource will be used.
     *          loadUsingSystemPrincipal:
     *            Set this to true to use the system principal as
     *            loadingPrincipal.  This must be omitted if loadingPrincipal or
     *            loadingNode are present.
     *            This should be used with care as it skips security checks.
     *          triggeringPrincipal:
     *            The triggeringPrincipal of the load.
     *            The triggeringPrincipal is the principal of the resource that
     *            caused this particular URL to be loaded.
     *            Most likely the triggeringPrincipal and the loadingPrincipal
     *            are identical, in which case the triggeringPrincipal can be
     *            left out.  In some cases the loadingPrincipal and the
     *            triggeringPrincipal are different however, e.g. a stylesheet
     *            may import a subresource.  In that case the principal of the
     *            stylesheet which contains the import command is the
     *            triggeringPrincipal, and the principal of the document whose
     *            rendering is affected is the loadingPrincipal.
     *          securityFlags:
     *            The securityFlags of the channel.
     *            Any of the securityflags defined in nsILoadInfo.idl.
     *          contentPolicyType:
     *            The contentPolicyType of the channel.
     *            Any of the content types defined in nsIContentPolicy.idl.
     *        }
     * @param aOriginCharset [deprecated]
     *        The character set for the URI.  Only used if aWhatToLoad is a
     *        string, which is a deprecated API.  Must be undefined otherwise.
     *        Use NetUtil.newURI if you need to use this option.
     * @param aBaseURI [deprecated]
     *        The base URI for the spec.  Only used if aWhatToLoad is a string,
     *        which is a deprecated API.  Must be undefined otherwise.  Use
     *        NetUtil.newURI if you need to use this option.
     *
     * @return an nsIChannel object.
     */
    newChannel: function NetUtil_newChannel(aWhatToLoad, aOriginCharset,
                                            aBaseURI)
    {
        // Check for the deprecated API first.
        if (typeof aWhatToLoad == "string" ||
            (aWhatToLoad instanceof Ci.nsIFile) ||
            (aWhatToLoad instanceof Ci.nsIURI)) {

            let uri = (aWhatToLoad instanceof Ci.nsIURI)
                      ? aWhatToLoad
                      : this.newURI(aWhatToLoad, aOriginCharset, aBaseURI);

            return this.ioService.newChannelFromURI(uri);
        }

        // We are using the updated API, that requires only the options object.
        if (typeof aWhatToLoad != "object" ||
            aOriginCharset !== undefined ||
            aBaseURI !== undefined) {

            throw new Components.Exception(
                "newChannel requires a single object argument",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
        }

        let { uri,
              loadingNode,
              loadingPrincipal,
              loadUsingSystemPrincipal,
              triggeringPrincipal,
              securityFlags,
              contentPolicyType } = aWhatToLoad;

        if (!uri) {
            throw new Components.Exception(
                "newChannel requires the 'uri' property on the options object.",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
        }

        if (typeof uri == "string") {
            uri = this.newURI(uri);
        }

        if (!loadingNode && !loadingPrincipal && !loadUsingSystemPrincipal) {
            throw new Components.Exception(
                "newChannel requires at least one of the 'loadingNode'," +
                " 'loadingPrincipal', or 'loadUsingSystemPrincipal'" +
                " properties on the options object.",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
        }

        if (loadUsingSystemPrincipal === true) {
            if (loadingNode || loadingPrincipal) {
                throw new Components.Exception(
                    "newChannel does not accept 'loadUsingSystemPrincipal'" +
                    " if the 'loadingNode' or 'loadingPrincipal' properties" +
                    " are present on the options object.",
                    Cr.NS_ERROR_INVALID_ARG,
                    Components.stack.caller
                );
            }
            loadingPrincipal = Services.scriptSecurityManager
                                       .getSystemPrincipal();
        } else if (loadUsingSystemPrincipal !== undefined) {
            throw new Components.Exception(
                "newChannel requires the 'loadUsingSystemPrincipal'" +
                " property on the options object to be 'true' or 'undefined'.",
                Cr.NS_ERROR_INVALID_ARG,
                Components.stack.caller
            );
        }

        if (securityFlags === undefined) {
            securityFlags = Ci.nsILoadInfo.SEC_NORMAL;
        }

        if (contentPolicyType === undefined) {
            if (!loadUsingSystemPrincipal) {
                throw new Components.Exception(
                    "newChannel requires the 'contentPolicyType' property on" +
                    " the options object unless loading from system principal.",
                    Cr.NS_ERROR_INVALID_ARG,
                    Components.stack.caller
                );
            }
            contentPolicyType = Ci.nsIContentPolicy.TYPE_OTHER;
        }

        return this.ioService.newChannelFromURI2(uri,
                                                 loadingNode || null,
                                                 loadingPrincipal || null,
                                                 triggeringPrincipal || null,
                                                 securityFlags,
                                                 contentPolicyType);
    },

    /**
     * @deprecated Use newChannel({ ...options... }) instead.
     */
    newChannel2: function NetUtil_newChannel2(aWhatToLoad,
                                              aOriginCharset,
                                              aBaseURI,
                                              aLoadingNode,
                                              aLoadingPrincipal,
                                              aTriggeringPrincipal,
                                              aSecurityFlags,
                                              aContentPolicyType)
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

        return this.ioService.newChannelFromURI2(uri,
                                                 aLoadingNode,
                                                 aLoadingPrincipal,
                                                 aTriggeringPrincipal,
                                                 aSecurityFlags,
                                                 aContentPolicyType);
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
