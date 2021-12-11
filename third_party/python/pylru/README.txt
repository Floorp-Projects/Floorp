

PyLRU
=====

A least recently used (LRU) cache for Python.

Introduction
============

Pylru implements a true LRU cache along with several support classes. The cache is efficient and written in pure Python. It works with Python 2.6+ including the 3.x series. Basic operations (lookup, insert, delete) all run in a constant amount of time. Pylru provides a cache class with a simple dict interface. It also provides classes to wrap any object that has a dict interface with a cache. Both write-through and write-back semantics are supported. Pylru also provides classes to wrap functions in a similar way, including a function decorator.

You can install pylru or you can just copy the source file pylru.py and use it directly in your own project. The rest of this file explains what the pylru module provides and how to use it. If you want to know more examine pylru.py. The code is straightforward and well commented.

Usage
=====

lrucache
--------

An lrucache object has a dictionary like interface and can be used in the same way::

    import pylru

    size = 100          # Size of the cache. The maximum number of key/value
                        # pairs you want the cache to hold.
    
    cache = pylru.lrucache(size)
                        # Create a cache object.
    
    value = cache[key]  # Lookup a value given its key.
    cache[key] = value  # Insert a key/value pair.
    del cache[key]      # Delete a value given its key.
                        #
                        # These three operations affect the order of the cache.
                        # Lookup and insert both move the key/value to the most
                        # recently used position. Delete (obviously) removes a
                        # key/value from whatever position it was in.
                        
    key in cache        # Test for membership. Does not affect the cache order.
    
    value = cache.peek(key)
                        # Lookup a value given its key. Does not affect the
                        # cache order.

    cache.keys()        # Return an iterator over the keys in the cache
    cache.values()      # Return an iterator over the values in the cache
    cache.items()       # Return an iterator over the (key, value) pairs in the
                        # cache.
                        #
                        # These calls have no effect on the cache order.
                        # lrucache is scan resistant when these calls are used.
                        # The iterators iterate over their respective elements
                        # in the order of most recently used to least recently
                        # used.
                        #
                        # WARNING - While these iterators do not affect the
                        # cache order the lookup, insert, and delete operations
                        # do. The result of changing the cache's order
                        # during iteration is undefined. If you really need to
                        # do something of the sort use list(cache.keys()), then
                        # loop over the list elements.
                        
    for key in cache:   # Caches support __iter__ so you can use them directly
        pass            # in a for loop to loop over the keys just like
                        # cache.keys()

    cache.size()        # Returns the size of the cache
    cache.size(x)       # Changes the size of the cache. x MUST be greater than
                        # zero. Returns the new size x.

    x = len(cache)      # Returns the number of items stored in the cache.
                        # x will be less than or equal to cache.size()

    cache.clear()       # Remove all items from the cache.


Lrucache takes an optional callback function as a second argument. Since the cache has a fixed size, some operations (such as an insertion) may cause the least recently used key/value pair to be ejected. If the optional callback function is given it will be called when this occurs. For example::

    import pylru

    def callback(key, value):
        print (key, value)    # A dumb callback that just prints the key/value

    size = 100
    cache = pylru.lrucache(size, callback)

    # Use the cache... When it gets full some pairs may be ejected due to
    # the fixed cache size. But, not before the callback is called to let you
    # know.

WriteThroughCacheManager
------------------------

Often a cache is used to speed up access to some other high latency object. For example, imagine you have a backend storage object that reads/writes from/to a remote server. Let us call this object *store*. If store has a dictionary interface a cache manager class can be used to compose the store object and an lrucache. The manager object exposes a dictionary interface. The programmer can then interact with the manager object as if it were the store. The manager object takes care of communicating with the store and caching key/value pairs in the lrucache object.

Two different semantics are supported, write-through (WriteThroughCacheManager class) and write-back (WriteBackCacheManager class). With write-through, lookups from the store are cached for future lookups. Insertions and deletions are updated in the cache and written through to the store immediately. Write-back works the same way, but insertions are updated only in the cache. These "dirty" key/value pair will only be updated to the underlying store when they are ejected from the cache or when a sync is performed. The WriteBackCacheManager class is discussed more below. 

The WriteThroughCacheManager class takes as arguments the store object you want to compose and the cache size. It then creates an LRU cache and automatically manages it::

    import pylru

    size = 100
    cached = pylru.WriteThroughCacheManager(store, size)
                        # Or
    cached = pylru.lruwrap(store, size)
                        # This is a factory function that does the same thing.

    # Now the object *cached* can be used just like store, except caching is
    # automatically handled.
    
    value = cached[key] # Lookup a value given its key.
    cached[key] = value # Insert a key/value pair.
    del cached[key]     # Delete a value given its key.
    
    key in cache        # Test for membership. Does not affect the cache order.

    cached.keys()       # Returns store.keys()
    cached.values()     # Returns store.values() 
    cached.items()      # Returns store.items()
                        #
                        # These calls have no effect on the cache order.
                        # The iterators iterate over their respective elements
                        # in the order dictated by store.
                        
    for key in cached:  # Same as store.keys()

    cached.size()       # Returns the size of the cache
    cached.size(x)      # Changes the size of the cache. x MUST be greater than
                        # zero. Returns the new size x.

    x = len(cached)     # Returns the number of items stored in the store.

    cached.clear()      # Remove all items from the store and cache.


WriteBackCacheManager
---------------------

Similar to the WriteThroughCacheManager class except write-back semantics are used to manage the cache. The programmer is responsible for one more thing as well. They MUST call sync() when they are finished. This ensures that the last of the "dirty" entries in the cache are written back. This is not too bad as WriteBackCacheManager objects can be used in with statements. More about that below::


    import pylru

    size = 100
    cached = pylru.WriteBackCacheManager(store, size)
                        # Or
    cached = pylru.lruwrap(store, size, True)
                        # This is a factory function that does the same thing.
                        
    value = cached[key] # Lookup a value given its key.
    cached[key] = value # Insert a key/value pair.
    del cached[key]     # Delete a value given its key.
    
    key in cache        # Test for membership. Does not affect the cache order.

                        
    cached.keys()       # Return an iterator over the keys in the cache/store
    cached.values()     # Return an iterator over the values in the cache/store
    cached.items()      # Return an iterator over the (key, value) pairs in the
                        # cache/store.
                        #
                        # The iterators iterate over a consistent view of the
                        # respective elements. That is, except for the order,
                        # the elements are the same as those returned if you
                        # first called sync() then called
                        # store.keys()[ or values() or items()]
                        #
                        # These calls have no effect on the cache order.
                        # The iterators iterate over their respective elements
                        # in arbitrary order.
                        #
                        # WARNING - While these iterators do not effect the
                        # cache order the lookup, insert, and delete operations
                        # do. The results of changing the cache's order
                        # during iteration is undefined. If you really need to
                        # do something of the sort use list(cached.keys()),
                        # then loop over the list elements.
                        
    for key in cached:  # Same as cached.keys()

    cached.size()       # Returns the size of the cache
    cached.size(x)      # Changes the size of the cache. x MUST be greater than
                        # zero. Returns the new size x.

    cached.clear()      # Remove all items from the store and cache.
    
    cached.sync()       # Make the store and cache consistent. Write all
                        # cached changes to the store that have not been
                        # yet.
                        
    cached.flush()      # Calls sync() then clears the cache.
    

To help the programmer ensure that the final sync() is called, WriteBackCacheManager objects can be used in a with statement::

    with pylru.WriteBackCacheManager(store, size) as cached:
        # Use cached just like you would store. sync() is called automatically
        # for you when leaving the with statement block.


FunctionCacheManager
---------------------

FunctionCacheManager allows you to compose a function with an lrucache. The resulting object can be called just like the original function, but the results are cached to speed up future calls. The fuction must have arguments that are hashable::

    import pylru

    def square(x):
        return x * x

    size = 100
    cached = pylru.FunctionCacheManager(square, size)

    y = cached(7)

    # The results of cached are the same as square, but automatically cached
    # to speed up future calls.

    cached.size()       # Returns the size of the cache
    cached.size(x)      # Changes the size of the cache. x MUST be greater than
                        # zero. Returns the new size x.

    cached.clear()      # Remove all items from the cache.



lrudecorator
------------

PyLRU also provides a function decorator. This is basically the same functionality as FunctionCacheManager, but in the form of a decorator::

    from pylru import lrudecorator

    @lrudecorator(100)
    def square(x):
        return x * x

    # The results of the square function are cached to speed up future calls.

    square.size()       # Returns the size of the cache
    square.size(x)      # Changes the size of the cache. x MUST be greater than
                        # zero. Returns the new size x.

    square.clear()      # Remove all items from the cache.
