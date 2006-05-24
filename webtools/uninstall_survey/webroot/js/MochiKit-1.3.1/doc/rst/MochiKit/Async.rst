.. title:: MochiKit.Async - manage asynchronous tasks

Name
====

MochiKit.Async - manage asynchronous tasks


Synopsis
========

::

    var url = "/src/b/bo/bob/MochiKit.Async/META.json";
    /*

        META.json looks something like this:

        {"name": "MochiKit", "version": "0.5"}

    */
    var d = loadJSONDoc(url);
    var gotMetadata = function (meta) {
        if (MochiKit.Async.VERSION == meta.version) {
            alert("You have the newest MochiKit.Async!");
        } else {
            alert("MochiKit.Async " 
                + meta.version
                + " is available, upgrade!");
        }
    };
    var metadataFetchFailed = function (err) {
      alert("The metadata for MochiKit.Async could not be fetched :(");
    };
    d.addCallbacks(gotMetadata, metadataFetchFailed);
    
  
Description
===========

MochiKit.Async provides facilities to manage asynchronous
(as in AJAX [1]_) tasks. The model for asynchronous computation
used in this module is heavily inspired by Twisted [2]_.


Dependencies
============

- :mochiref:`MochiKit.Base`


Overview
========

Deferred
--------

The Deferred constructor encapsulates a single value that
is not available yet. The most important example of this
in the context of a web browser would be an ``XMLHttpRequest``
to a server. The importance of the Deferred is that it
allows a consistent API to be exposed for all asynchronous
computations that occur exactly once.

The producer of the Deferred is responsible for doing all
of the complicated work behind the scenes. This often
means waiting for a timer to fire, or waiting for an event
(e.g. ``onreadystatechange`` of ``XMLHttpRequest``). 
It could also be coordinating several events (e.g.
``XMLHttpRequest`` with a timeout, or several Deferreds
(e.g. fetching a set of XML documents that should be 
processed at the same time).

Since these sorts of tasks do not respond immediately, the
producer of the Deferred does the following steps before
returning to the consumer:

1. Create a ``new`` :mochiref:`Deferred();` object and keep a reference
   to it, because it will be needed later when the value is
   ready.
2. Setup the conditions to create the value requested (e.g.
   create a new ``XMLHttpRequest``, set its 
   ``onreadystatechange``).
3. Return the :mochiref:`Deferred` object.

Since the value is not yet ready, the consumer attaches
a function to the Deferred that will be called when the
value is ready. This is not unlike ``setTimeout``, or
other similar facilities you may already be familiar with.
The consumer can also attach an "errback" to the
:mochiref:`Deferred`, which is a callback for error handling.

When the value is ready, the producer simply calls
``myDeferred.callback(theValue)``. If an error occurred,
it should call ``myDeferred.errback(theValue)`` instead.
As soon as this happens, the callback that the consumer
attached to the :mochiref:`Deferred` is called with ``theValue``
as the only argument.

There are quite a few additional "advanced" features
baked into :mochiref:`Deferred`, such as cancellation and 
callback chains, so take a look at the API
reference if you would like to know more!

API Reference
=============

Errors
------

:mochidef:`AlreadyCalledError`:

    Thrown by a :mochiref:`Deferred` if ``.callback`` or
    ``.errback`` are called more than once.


:mochidef:`BrowserComplianceError`:

    Thrown when the JavaScript runtime is not capable of performing
    the given function. Currently, this happens if the browser
    does not support ``XMLHttpRequest``.


:mochidef:`CancelledError`:

    Thrown by a :mochiref:`Deferred` when it is cancelled,
    unless a canceller is present and throws something else.


:mochidef:`GenericError`:

    Results passed to ``.fail`` or ``.errback`` of a :mochiref:`Deferred`
    are wrapped by this ``Error`` if ``!(result instanceof Error)``.


:mochidef:`XMLHttpRequestError`:

    Thrown when an ``XMLHttpRequest`` does not complete successfully
    for any reason. The ``req`` property of the error is the failed
    ``XMLHttpRequest`` object, and for convenience the ``number``
    property corresponds to ``req.status``.


Constructors
------------

:mochidef:`Deferred()`:

    Encapsulates a sequence of callbacks in response to a value that
    may not yet be available. This is modeled after the Deferred class
    from Twisted [3]_.

.. _`Twisted`: http://twistedmatrix.com/

    Why do we want this?  JavaScript has no threads, and even if it did,
    threads are hard. Deferreds are a way of abstracting non-blocking
    events, such as the final response to an ``XMLHttpRequest``.

    The sequence of callbacks is internally represented as a list
    of 2-tuples containing the callback/errback pair. For example,
    the following call sequence::

        var d = new Deferred();
        d.addCallback(myCallback);
        d.addErrback(myErrback);
        d.addBoth(myBoth);
        d.addCallbacks(myCallback, myErrback);

    is translated into a :mochiref:`Deferred` with the following internal
    representation::

        [
            [myCallback, null],
            [null, myErrback],
            [myBoth, myBoth],
            [myCallback, myErrback]
        ]

    The :mochiref:`Deferred` also keeps track of its current status (fired).
    Its status may be one of the following three values:
    
        
        ===== ================================
        Value Condition
        ===== ================================
        -1    no value yet (initial condition)
        0     success
        1     error
        ===== ================================
    
    A :mochiref:`Deferred` will be in the error state if one of the following
    conditions are met:
    
    1. The result given to callback or errback is "``instanceof Error``"
    2. The callback or errback threw while executing. If the thrown object
       is not ``instanceof Error``, it will be wrapped with
       :mochiref:`GenericError`.

    Otherwise, the :mochiref:`Deferred` will be in the success state. The state
    of the :mochiref:`Deferred` determines the next element in the callback
    sequence to run.

    When a callback or errback occurs with the example deferred chain, something
    equivalent to the following will happen (imagine that exceptions are caught
    and returned as-is)::

        // d.callback(result) or d.errback(result)
        if (!(result instanceof Error)) {
            result = myCallback(result);
        }
        if (result instanceof Error) {
            result = myErrback(result);
        }
        result = myBoth(result);
        if (result instanceof Error) {
            result = myErrback(result);
        } else {
            result = myCallback(result);
        }
    
    The result is then stored away in case another step is added to the
    callback sequence. Since the :mochiref:`Deferred` already has a value
    available, any new callbacks added will be called immediately.

    There are two other "advanced" details about this implementation that are 
    useful:

    Callbacks are allowed to return :mochiref:`Deferred` instances,
    so you can build complicated sequences of events with (relative) ease.

    The creator of the :mochiref:`Deferred` may specify a canceller. The
    canceller is a function that will be called if
    :mochiref:`Deferred.prototype.cancel` is called before the
    :mochiref:`Deferred` fires. You can use this to allow an
    ``XMLHttpRequest`` to be cleanly cancelled, for example. Note that
    cancel will fire the :mochiref:`Deferred` with a
    :mochiref:`CancelledError` (unless your canceller throws or returns
    a different ``Error``), so errbacks should be prepared to handle that
    ``Error`` gracefully for cancellable :mochiref:`Deferred` instances.


:mochidef:`Deferred.prototype.addBoth(func)`:

    Add the same function as both a callback and an errback as the
    next element on the callback sequence. This is useful for code
    that you want to guarantee to run, e.g. a finalizer.

    If additional arguments are given, then ``func`` will be replaced
    with :mochiref:`MochiKit.Base.partial.apply(null, arguments)`. This
    differs from `Twisted`_, because the result of the callback or
    errback will be the *last* argument passed to ``func``.

    If ``func`` returns a :mochiref:`Deferred`, then it will be chained
    (its value or error will be passed to the next callback). Note that
    once the returned ``Deferred`` is chained, it can no longer accept new
    callbacks.


:mochidef:`Deferred.prototype.addCallback(func[, ...])`:

    Add a single callback to the end of the callback sequence.

    If additional arguments are given, then ``func`` will be replaced
    with :mochiref:`MochiKit.Base.partial.apply(null, arguments)`. This
    differs from `Twisted`_, because the result of the callback will
    be the *last* argument passed to ``func``.

    If ``func`` returns a :mochiref:`Deferred`, then it will be chained
    (its value or error will be passed to the next callback). Note that
    once the returned ``Deferred`` is chained, it can no longer accept new
    callbacks.

:mochidef:`Deferred.prototype.addCallbacks(callback, errback)`:

    Add separate callback and errback to the end of the callback
    sequence. Either callback or errback may be ``null``,
    but not both.

    If ``callback`` or ``errback`` returns a :mochiref:`Deferred`,
    then it will be chained (its value or error will be passed to the
    next callback). Note that once the returned ``Deferred`` is chained,
    it can no longer accept new callbacks.

:mochidef:`Deferred.prototype.addErrback(func)`:

    Add a single errback to the end of the callback sequence.

    If additional arguments are given, then ``func`` will be replaced
    with :mochiref:`MochiKit.Base.partial.apply(null, arguments)`. This
    differs from `Twisted`_, because the result of the errback will
    be the *last* argument passed to ``func``.

    If ``func`` returns a :mochiref:`Deferred`, then it will be chained
    (its value or error will be passed to the next callback). Note that
    once the returned ``Deferred`` is chained, it can no longer accept new
    callbacks.

:mochidef:`Deferred.prototype.callback([result])`:

    Begin the callback sequence with a non-``Error`` result. Result
    may be any value except for a :mochiref:`Deferred`.
    
    Either ``.callback`` or ``.errback`` should
    be called exactly once on a :mochiref:`Deferred`.


:mochidef:`Deferred.prototype.cancel()`:

    Cancels a :mochiref:`Deferred` that has not yet received a value,
    or is waiting on another :mochiref:`Deferred` as its value.

    If a canceller is defined, the canceller is called.
    If the canceller did not return an ``Error``, or there
    was no canceller, then the errback chain is started
    with :mochiref:`CancelledError`.
        

:mochidef:`Deferred.prototype.errback([result])`:

    Begin the callback sequence with an error result.
    Result may be any value except for a :mochiref:`Deferred`,
    but if ``!(result instanceof Error)``, it will be wrapped
    with :mochiref:`GenericError`.

    Either ``.callback`` or ``.errback`` should
    be called exactly once on a :mochidef:`Deferred`.


:mochidef:`DeferredLock()`:

    A lock for asynchronous systems.

    The ``locked`` property of a :mochiref:`DeferredLock` will be ``true`` if
    it locked, ``false`` otherwise. Do not change this property.


:mochidef:`DeferredLock.prototype.acquire()`:

    Attempt to acquire the lock. Returns a :mochiref:`Deferred` that fires on
    lock acquisition with the :mochiref:`DeferredLock` as the value.
    If the lock is locked, then the :mochiref:`Deferred` goes into a waiting
    list.


:mochidef:`DeferredLock.prototype.release()`:
    
    Release the lock. If there is a waiting list, then the first
    :mochiref:`Deferred` in that waiting list will be called back.


:mochidef:`DeferredList(list, [fireOnOneCallback, fireOnOneErrback, consumeErrors, canceller])`:

    Combine a list of :mochiref:`Deferred` into one. Track the callbacks and 
    return a list of (success, result) tuples, 'success' being a boolean 
    indicating whether result is a normal result or an error.

    Once created, you have access to all :mochiref:`Deferred` methods, like
    addCallback, addErrback, addBoth. The behaviour can be changed by the
    following options:

    ``fireOnOneCallback``:
        Flag for launching the callback once the first Deferred of the list
        has returned.

    ``fireOnOneErrback``:
        Flag for calling the errback at the first error of a Deferred.

    ``consumeErrors``:
        Flag indicating that any errors raised in the Deferreds should be
        consumed by the DeferredList.

    Example::

        // We need to fetch data from 2 different urls
        var d1 = loadJSONDoc(url1);
        var d2 = loadJSONDoc(url2);
        var l1 = new DeferredList([d1, d2], false, false, true);
        l1.addCallback(function (resultList) {
            MochiKit.Base.map(function (result) {
                if (result[0]) {
                    alert("Data is here: " + result[1]);
                } else {
                    alert("Got an error: " + result[1]);
                }
            }, resultList);
        });
        

Functions
---------

:mochidef:`callLater(seconds, func[, args...])`:

    Call ``func(args...)`` after at least ``seconds`` seconds have elapsed.
    This is a convenience method for::

        func = partial.apply(extend(null, arguments, 1));
        return wait(seconds).addCallback(function (res) { return func() });

    Returns a cancellable :mochiref:`Deferred`.


:mochidef:`doSimpleXMLHttpRequest(url[, queryArguments...])`:

    Perform a simple ``XMLHttpRequest`` and wrap it with a
    :mochiref:`Deferred` that may be cancelled. 

    Note that currently, only ``200`` (OK) and ``304``
    (NOT_MODIFIED) are considered success codes at this time, other
    status codes will result in an errback with an ``XMLHttpRequestError``.

    ``url``:
        The URL to GET

    ``queryArguments``:
        If this function is called with more than one argument, a ``"?"``
        and the result of :mochiref:`MochiKit.Base.queryString` with
        the rest of the arguments are appended to the URL.

        For example, this will do a GET request to the URL
        ``http://example.com?bar=baz``::
        
            doSimpleXMLHttpRequest("http://example.com", {bar: "baz"});

    *returns*:
        :mochiref:`Deferred` that will callback with the ``XMLHttpRequest``
        instance on success
    

:mochidef:`evalJSONRequest(req)`:

    Evaluate a JSON [4]_ ``XMLHttpRequest``

    ``req``:
        The request whose ``.responseText`` property is to be evaluated

    *returns*:
        A JavaScript object


:mochidef:`fail([result])`:

    Return a :mochiref:`Deferred` that has already had ``.errback(result)``
    called.

    See ``succeed`` documentation for rationale.

    ``result``:
        The result to give to :mochiref:`Deferred.prototype.errback(result)`.

    *returns*:
        A ``new`` :mochiref:`Deferred()`


:mochidef:`gatherResults(deferreds)`:

    A convenience function that returns a :mochiref:`DeferredList`
    from the given ``Array`` of :mochiref:`Deferred` instances
    that will callback with an ``Array`` of just results when
    they're available, or errback on the first array.


:mochidef:`getXMLHttpRequest()`:

    Return an ``XMLHttpRequest`` compliant object for the current
    platform.

    In order of preference:

    - ``new XMLHttpRequest()``
    - ``new ActiveXObject('Msxml2.XMLHTTP')``
    - ``new ActiveXObject('Microsoft.XMLHTTP')``
    - ``new ActiveXObject('Msxml2.XMLHTTP.4.0')``


:mochidef:`maybeDeferred(func[, argument...])`:

    Call a ``func`` with the given arguments and ensure the result is a
    :mochiref:`Deferred`.

    ``func``:
        The function to call.

    *returns*:
        A new :mochiref:`Deferred` based on the call to ``func``. If ``func``
        does not naturally return a :mochiref:`Deferred`, its result or error
        value will be wrapped by one.


:mochidef:`loadJSONDoc(url)`:

    Do a simple ``XMLHttpRequest`` to a URL and get the response
    as a JSON [4]_ document.

    ``url``:
        The URL to GET

    *returns*:
        :mochiref:`Deferred` that will callback with the evaluated JSON [4]_
        response upon successful ``XMLHttpRequest``


:mochidef:`sendXMLHttpRequest(req[, sendContent])`:

    Set an ``onreadystatechange`` handler on an ``XMLHttpRequest`` object
    and send it off. Will return a cancellable :mochiref:`Deferred` that will
    callback on success.
    
    Note that currently, only ``200`` (OK) and ``304``
    (NOT_MODIFIED) are considered success codes at this time, other
    status codes will result in an errback with an ``XMLHttpRequestError``.

    ``req``:
        An preconfigured ``XMLHttpRequest`` object (open has been called).

    ``sendContent``:
        Optional string or DOM content to send over the ``XMLHttpRequest``.

    *returns*:
        :mochiref:`Deferred` that will callback with the ``XMLHttpRequest``
        instance on success.


:mochidef:`succeed([result])`:

    Return a :mochiref:`Deferred` that has already had ``.callback(result)``
    called.

    This is useful when you're writing synchronous code to an asynchronous
    interface: i.e., some code is calling you expecting a :mochiref:`Deferred`
    result, but you don't actually need to do anything asynchronous. Just
    return ``succeed(theResult)``.

    See ``fail`` for a version of this function that uses a failing
    :mochiref:`Deferred` rather than a successful one.

    ``result``:
        The result to give to :mochiref:`Deferred.prototype.callback(result)`

    *returns*:
        a ``new`` :mochiref:`Deferred`

   
:mochidef:`wait(seconds[, res])`:

    Return a new cancellable :mochiref:`Deferred` that will ``.callback(res)``
    after at least ``seconds`` seconds have elapsed.


See Also
========

.. [1] AJAX, Asynchronous JavaScript and XML: http://en.wikipedia.org/wiki/AJAX
.. [2] Twisted, an event-driven networking framework written in Python: http://twistedmatrix.com/
.. [3] Twisted Deferred Reference: http://twistedmatrix.com/projects/core/documentation/howto/defer.html
.. [4] JSON, JavaScript Object Notation: http://json.org/


Authors
=======

- Bob Ippolito <bob@redivi.com>


Copyright
=========

Copyright 2005 Bob Ippolito <bob@redivi.com>. This program is dual-licensed
free software; you can redistribute it and/or modify it under the terms of the
`MIT License`_ or the `Academic Free License v2.1`_.

.. _`MIT License`: http://www.opensource.org/licenses/mit-license.php
.. _`Academic Free License v2.1`: http://www.opensource.org/licenses/afl-2.1.php
