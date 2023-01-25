=======================================
How to make a C++ class cycle collected
=======================================

Should my class be cycle collected?
===================================

First, you need to decide if your class should be cycle
collected. There are three main criteria:

* It can be part of a cycle of strong references, including
  refcounted objects and JS. Usually, this happens when it can hold
  alive and be held alive by cycle collected objects or JS.

* It must be refcounted.

* It must be single threaded. The cycle collector can only work with
  objects that are used on a single thread. The main thread and DOM
  worker and worklet threads each have their own cycle collectors.

If your class meets the first criteria but not the second, then
whatever class uniquely owns it should be cycle collected, assuming
that is refcounted, and this class should be traversed and unlinked as
part of that.

The cycle collector supports both nsISupports and non-nsISupports
(known as "native" in CC nomenclature) refcounting. However, we do not
support native cycle collection in the presence of inheritance, if two
classes related by inheritance need different CC
implementations. (This is because we use QueryInterface to find the
right CC implementation for an object.)

Once you've decided to make a class cycle collected, there are a few
things you need to add to your implementation:

* Cycle collected refcounting. Special refcounting is needed so that
  the CC can tell when an object is created, used, or destroyed, so
  that it can determine if an object is potentially part of a garbage
  cycle.

* Traversal. Once the CC has decided an object **might** be garbage,
  it needs to know what other cycle collected objects it holds strong
  references to. This is done with a "traverse" method.

* Unlinking. Once the CC has decided that an object **is** garbage, it
  needs to break the cycles by clearing out all strong references to
  other cycle collected objects. This is done with an "unlink"
  method. This usually looks very similar to the traverse method.

The traverse and unlink methods, along with other methods needed for
cycle collection, are defined on a special inner class object, called a
"participant", for performance reasons. The existence of the
participant is mostly hidden behind macros so you shouldn't need
to worry about it.

Next, we'll go over what the declaration and definition of these parts
look like. (Spoiler: there are lots of `ALL_CAPS` macros.) This will
mostly cover the most common variants. If you need something slightly
different, you should look at the location of the declaration of the
macros we mention here and see if the variants already exist.


Reference counting
==================

nsISupports
-----------

If your class inherits from nsISupports, you'll need to add
`NS_DECL_CYCLE_COLLECTING_ISUPPORTS` to the class declaration. This
will declare the QueryInterface (QI), AddRef and Release methods you
need to implement nsISupports, as well as the actual refcount field.

In the `.cpp` file for your class you'll have to define the
QueryInterface, AddRef and Release methods. The ins and outs of
defining the QI method is out-of-scope for this document, but you'll
need to use the special cycle collection variants of the macros, like
`NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION`. (This is because we use
the nsISupports system to define a special interface used to
dynamically find the correct CC participant for the object.)

Finally, you'll have to actually define the AddRef and Release methods
for your class. If your class is called `MyClass`, then you'd do this
with the declarations `NS_IMPL_CYCLE_COLLECTING_ADDREF(MyClass)` and
`NS_IMPL_CYCLE_COLLECTING_RELEASE(MyClass)`.

non-nsISupports
---------------

If your class does **not** inherit from nsISupports, you'll need to
add `NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING` to the class
declaration. This will give inline definitions for the AddRef and
Release methods, as well as the actual refcount field.

Cycle collector participant
===========================

Next we need to declare and define the cycle collector
participant. This is mostly boilerplate hidden behind macros, but you
will need to specify which fields are to be traversed and unlinked
because they are strong references to cycle collected objects.

Declaration
-----------

First, we need to add a declaration for the participant. As before,
let's say your class is `MyClass`.

The basic way to declare this for an nsISupports class is
`NS_DECL_CYCLE_COLLECTION_CLASS(MyClass)`.

If your class inherits from multiple classes that inherit from
nsISupports classes, say `Parent1` and `Parent2`, then you'll need to
use `NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(MyClass, Parent1)` to
tell the CC to cast to nsISupports via `Parent1`. You probably want to
pick the first class it inherits from. (The cycle collector needs to be
able to cast `MyClass*` to `nsISupports*`.)

Another situation you might encounter is that your nsISupports class
inherits from another cycle collected class `CycleCollectedParent`. In
that case, your participant declaration will look like
`NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MyClass,
CycleCollectedParent)`. (This is needed so that the CC can cast from
nsISupports down to `MyClass`.) Note that we do not support inheritance
for non-nsISupports classes.

If your class is non-nsISupports, then you'll need to use the `NATIVE`
family of macros, like
`NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(MyClass)`.

In addition to these modifiers, these different variations have
further `SCRIPT_HOLDER` variations which are needed if your class
holds alive JavaScript objects. This is because the tracing of JS
objects held alive by this class must be declared separately from the
tracing of C++ objects held alive by this class so that the garbage
collector can also use the tracing. For example,
`NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(MyClass,
Parent1)`.

There are also `WRAPPERCACHE` variants of the macros which you need to
use if your class is wrapper cached. These are effectively a
specialized form of `SCRIPT_HOLDER`, as a cached wrapper is a
JS object held alive by the C++ object. For example,
`NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(MyClass,
Parent1)`.

There is yet another variant of these macros, `SKIPPABLE`. This
document won't go into detail here about how this works, but the basic
idea is that a class can tell the CC when it is definitely alive,
which lets the CC skip it. This is a very important optimization for
things like DOM elements in active documents, but a new class you are
making cycle collected is likely not common enough to worry about.

Implementation
--------------

Finally, you must write the actual implementation of the CC
participant, in the .cpp file for your class. This will define the
traverse and unlink methods, and some other random helper
functions. In the simplest case, this can be done with a single macro
like this: `NS_IMPL_CYCLE_COLLECTION(MyClass, mField1, mField2,
mField3)`, where `mField1` and the rest are the names of the fields of
your class that are strong references to cycle collected
objects. There is some template magic that says how many common types
like RefPtr, nsCOMPtr, and even some arrays, should be traversed and
unlinked. There’s also a variant `NS_IMPL_CYCLE_COLLECTION_INHERITED`,
which you should use when there’s a parent class that is also cycle
collected, to ensure that fields of the parent class are traversed and
unlinked. The name of that parent class is passed in as the second
argument. If either of these work, then you are done. Your class is
now cycle collected. Note that this does not work for fields that are
JS objects.

However, if that doesn’t work, you’ll have to get into the details a
bit more. A good place to start is by copying the definition of
`NS_IMPL_CYCLE_COLLECTION`.

For a script holder method, you also need to define a trace method in
addition to the traverse and unlink, using
`NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN` and other similar
macros. You'll need to include all of the JS fields that your class
holds alive.  The trace method will be used by the GC as well as the
CC, so if you miss something you can end up with use-after-free
crashes. You'll also need to call `mozilla::HoldJSObjects(this);` in
the ctor for your class, and `mozilla::DropJSObjects(this);` in the
dtor. This will register (and unregister) each instance of your object
with the JS runtime, to ensure that it gets traced properly. This
does not apply if you have a wrapper cached class that does not have
any additional JS fields, as nsWrapperCache deals with all of that
for you.
