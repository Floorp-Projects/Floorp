XPIDL
=====

**XPIDL** is an Interface Description Language used to specify XPCOM interface
classes.

Interface Description Languages (IDL) are used to describe interfaces in a
language- and machine-independent way. IDLs make it possible to define
interfaces which can then be processed by tools to autogenerate
language-dependent interface specifications.

An xpidl file is essentially just a series of declarations. At the top level,
we can define typedefs, native types, or interfaces. Interfaces may
furthermore contain typedefs, natives, methods, constants, or attributes.
Most declarations can have properties applied to them.

Types
-----

There are three ways to make types: a typedef, a native, or an interface. In
addition, there are a few built-in native types. The built-in native types
are those listed under the type_spec production above. The following is the
correspondence table:

=========================== =============== =========================== ============================ ======================= =======================
IDL Type                    Javascript Type C++ in parameter            C++ out parameter            Rust in parameter       Rust out parameter
=========================== =============== =========================== ============================ ======================= =======================
``boolean``                 boolean         ``bool``                    ``bool*``                    ``bool``                ``*mut bool``
``char``                    string          ``char``                    ``char*``                    ``c_char``              ``*mut c_char``
``double``                  number          ``double``                  ``double*``                  ``f64``                 ``*mut f64``
``float``                   number          ``float``                   ``float*``                   ``f32``                 ``*mut f32``
``long``                    number          ``int32_t``                 ``int32_t*``                 ``i32``                 ``*mut i32``
``long long``               number          ``int64_t``                 ``int64_t*``                 ``i64``                 ``*mut i64``
``octet``                   number          ``uint8_t``                 ``uint8_t*``                 ``u8``                  ``*mut u8``
``short``                   number          ``uint16_t``                ``uint16_t*``                ``u16``                 ``*mut u16``
``string`` [#strptr]_       string          ``const char*``             ``char**``                   ``*const c_char``       ``*mut *mut c_char``
``unsigned long``           number          ``uint32_t``                ``uint32_t*``                ``u32``                 ``*mut u32``
``unsigned long long``      number          ``uint64_t``                ``uint64_t*``                ``u64``                 ``*mut u64``
``unsigned short``          number          ``uint16_t``                ``uint16_t*``                ``u16``                 ``*mut u16``
``wchar``                   string          ``char16_t``                ``char16_t*``                ``i16``                 ``*mut i16``
``wstring`` [#strptr]_      string          ``const char16_t*``         ``char16_t**``               ``*const i16``          ``*mut *mut i16``
``MozExternalRefCountType`` number          ``MozExternalRefCountType`` ``MozExternalRefCountType*`` ``u32``                 ``*mut u32``
``Array<T>`` [#array]_      array           ``const nsTArray<T>&``      ``nsTArray<T>&``             ``*const ThinVec<T>``   ``*mut ThinVec<T>``
=========================== =============== =========================== ============================ ======================= =======================

.. [#strptr]

    Prefer using the string class types such as ``AString``, ``AUTF8String``
    or ``ACString`` to this type. The behaviour of these types is documented
    more in the :ref:`String Guide <stringguide.xpidl>`

.. [#array]

    The C++ or Rust exposed type ``T`` will be an owned variant. (e.g.
    ``ns[C]String``, ``RefPtr<T>``, or ``uint32_t``)

    ``string``, ``wstring``, ``[ptr] native`` and ``[ref] native`` are
    unsupported as element types.


In addition to this list, nearly every IDL file includes ``nsrootidl.idl`` in
some fashion, which also defines the following types:

======================= ======================= ======================= ======================= ======================= =======================
IDL Type                Javascript Type         C++ in parameter        C++ out parameter       Rust in parameter       Rust out parameter
======================= ======================= ======================= ======================= ======================= =======================
``PRTime``              number                  ``uint64_t``            ``uint64_t*``           ``u64``                 ``*mut u64``
``nsresult``            number                  ``nsresult``            ``nsresult*``           ``u32`` [#rsresult]_    ``*mut u32``
``size_t``              number                  ``uint32_t``            ``uint32_t*``           ``u32``                 ``*mut u32``
``voidPtr``             N/A                     ``void*``               ``void**``              ``*mut c_void``         ``*mut *mut c_void``
``charPtr``             N/A                     ``char*``               ``char**``              ``*mut c_char``         ``*mut *mut c_char``
``unicharPtr``          N/A                     ``char16_t*``           ``char16_t**``          ``*mut i16``            ``*mut *mut i16``
``nsIDRef``             ID object               ``const nsID&``         ``nsID*``               ``*const nsID``         ``*mut nsID``
``nsIIDRef``            ID object               ``const nsIID&``        ``nsIID*``              ``*const nsIID``        ``*mut nsIID``
``nsCIDRef``            ID object               ``const nsCID&``        ``nsCID*``              ``*const nsCID``        ``*mut nsCID``
``nsIDPtr``             ID object               ``const nsID*``         ``nsID**``              ``*const nsID``         ``*mut *mut nsID``
``nsIIDPtr``            ID object               ``const nsIID*``        ``nsIID**``             ``*const nsIID``        ``*mut *mut nsIID``
``nsCIDPtr``            ID object               ``const nsCID*``        ``nsCID**``             ``*const nsCID``        ``*mut *mut nsCID``
``nsID``                N/A                     ``nsID``                ``nsID*``               N/A                     N/A
``nsIID``               N/A                     ``nsIID``               ``nsIID*``              N/A                     N/A
``nsCID``               N/A                     ``nsCID``               ``nsCID*``              N/A                     N/A
``nsQIResult``          object                  ``void*``               ``void**``              ``*mut c_void``         ``*mut *mut c_void``
``AUTF8String`` [#str]_ string                  ``const nsACString&``   ``nsACString&``         ``*const nsACString``   ``*mut nsACString``
``ACString`` [#str]_    string                  ``const nsACString&``   ``nsACString&``         ``*const nsACString``   ``*mut nsACString``
``AString`` [#str]_     string                  ``const nsAString&``    ``nsAString&``          ``*const nsAString``    ``*mut nsAString``
``jsval``               any                     ``HandleValue``         ``MutableHandleValue``  N/A                     N/A
``jsid``                N/A                     ``jsid``                ``jsid*``               N/A                     N/A
``Promise``             Promise object          ``dom::Promise*``       ``dom::Promise**``      N/A                     N/A
======================= ======================= ======================= ======================= ======================= =======================

.. [#rsresult]

    A bare ``u32`` is only for bare ``nsresult`` in/outparams in XPIDL. The
    result should be wrapped as the ``nserror::nsresult`` type.

.. [#str]

    The behaviour of these types is documented more in the :ref:`String Guide
    <stringguide.xpidl>`

Typedefs in IDL are basically as they are in C or C++: you define first the
type that you want to refer to and then the name of the type. Types can of
course be one of the fundamental types, or any other type declared via a
typedef, interface, or a native type.

Native types are types which correspond to a given C++ type. Most native
types are not scriptable: if it is not present in the list above, then it is
certainly not scriptable (some of the above, particularly jsid, are not
scriptable).

The contents of the parentheses of a native type declaration (although native
declarations without parentheses are parsable, I do not trust that they are
properly handled by the xpidl handlers) is a string equivalent to the C++
type. XPIDL itself does not interpret this string, it just literally pastes
it anywhere the native type is used. The interpretation of the type can be
modified by using the ``[ptr]`` or ``[ref]`` attributes on the native
declaration. Other attributes are only intended for use in ``nsrootidl.idl``.

WebIDL Interfaces
~~~~~~~~~~~~~~~~~

WebIDL interfaces are also valid XPIDL types. To declare a WebIDL interface in
XPIDL, write:

.. code-block:: JavaScript

    webidl InterfaceName;

WebIDL types will be passed as ``mozilla::dom::InterfaceName*`` when used as
in-parameters, as ``mozilla::dom::InterfaceName**`` when used as out or
inout-parameters, and as ``RefPtr<mozilla::dom::InterfaceName>`` when used as
an array element.

.. note::

    Other WebIDL types (e.g. dictionaries, enums, and unions) are not currently
    supported.

Constants and CEnums
~~~~~~~~~~~~~~~~~~~~

Constants must be attached to an interface. The only constants supported are
those which become integer types when compiled to source code; string constants
and floating point constants are currently not supported.

Often constants are used to describe a set of enum values. In cases like this
the ``cenum`` construct can be used to group constants together. Constants
grouped in a ``cenum`` will be reflected as-if they were declared directly on
the interface, in Rust and Javascript code.

.. code-block:: JavaScript

   cenum MyCEnum : 8 {
     eSomeValue,  // starts at 0
     eSomeOtherValue,
   };

The number after the enum name, like ``: 8`` in the example above, defines the
width of enum values with the given type. The cenum's type may be referenced in
xpidl as ``nsIInterfaceName_MyCEnum``.

Interfaces
----------

Interfaces are basically a collection of constants, methods, and attributes.
Interfaces can inherit from one-another, and every interface must eventually
inherit from ``nsISupports``.

Interface Attributes
~~~~~~~~~~~~~~~~~~~~

Interfaces may have the following attributes:

``uuid``
````````

The internal unique identifier for the interface. it must be unique, and the
uuid must be generated when creating the interface. After that, it doesn't need
to be changed any more.

Online tools such as http://mozilla.pettay.fi/cgi-bin/mozuuid.pl can help
generate UUIDs for new interfaces.

``builtinclass``
````````````````

JavaScript classes are forbidden from implementing this interface. All child
interfaces must also be marked with this property.

``function``
````````````

The JavaScript implementation of this interface may be a function that is
invoked on property calls instead of an object with the given property

``scriptable``
``````````````

This interface is usable by JavaScript classes. Must inherit from a
``scriptable`` interface.

``rust_sync``
`````````````

This interface is safe to use from multiple threads concurrently. All child
interfaces must also be marked with this property. Interfaces marked this way
must be either non-scriptable or ``builtinclass``, and must use threadsafe
reference counting.

Interfaces marked as ``rust_sync`` will implement the ``Sync`` trait in Rust.
For more details on what that means, read the trait's documentation:
https://doc.rust-lang.org/nightly/std/marker/trait.Sync.html.

Methods and Attributes
~~~~~~~~~~~~~~~~~~~~~~

Interfaces declare a series of attributes and methods. Attributes in IDL are
akin to JavaScript properties, in that they are a getter and (optionally) a
setter pair. In JavaScript contexts, attributes are exposed as a regular
property access, while native code sees attributes as a Get and possibly a Set
method.

Attributes can be declared readonly, in which case setting causes an error to
be thrown in script contexts and native contexts lack the Set method, by using
the ``readonly`` keyword.

To native code, on attribute declared ``attribute type foo;`` is syntactic
sugar for the declaration of two methods ``type getFoo();`` and ``void
setFoo(in type foo);``. If ``foo`` were declared readonly, the latter method
would not be present.  Attributes support all of the properties of methods with
the exception of ``optional_argc``, as this does not make sense for attributes.

There are some special rules for attribute naming. As a result of vtable
munging by the MSVC++ compiler, an attribute with the name ``IID`` is
forbidden.  Also like methods, if the first character of an attribute is
lowercase in IDL, it is made uppercase in native code only.

Methods define a return type and a series of in and out parameters. When called
from a JavaScript context, they invocation looks as it is declared for the most
part; some parameter properties can adjust what the code looks like. The calls
are more mangled in native contexts.

An important attribute for methods and attributes is scriptability. A method or
attribute is scriptable if it is declared in a ``scriptable`` interface and it
lacks a ``noscript`` or ``notxpcom`` property. Any method that is not
scriptable can only be accessed by native code. However, ``scriptable`` methods
must contain parameters and a return type that can be translated to script: any
native type, save a few declared in ``nsrootidl.idl`` (see above), may not be
used in a scriptable method or attribute. An exception to the above rule is if
a ``nsQIResult`` parameter has the ``iid_is`` property (a special case for some
QueryInterface-like operations).

Methods and attributes are mangled on conversion to native code. If a method is
declared ``notxpcom``, the mangling of the return type is prevented, so it is
called mostly as it looks. Otherwise, the return type of the native method is
``nsresult``, and the return type acts as a final outparameter if it is not
``void``.  The name is translated so that the first character is
unconditionally uppercase; subsequent characters are unaffected. However, the
presence of the ``binaryname`` property allows the user to select another name
to use in native code (to avoid conflicts with other functions). For example,
the method ``[binaryname(foo)] void bar();`` becomes ``nsresult Foo()`` in
native code (note that capitalization is still applied). However, the
capitalization is not applied when using ``binaryname`` with attributes; i.e.,
``[binaryname(foo)] readonly attribute Quux bar;`` becomes ``Getfoo(Quux**)``
in native code.

The ``implicit_jscontext`` and ``optional_argc`` parameters are properties
which help native code implementations determine how the call was made from
script. If ``implicit_jscontext`` is present on a method, then an additional
``JSContext* cx`` parameter is added just after the regular list which receives
the context of the caller. If ``optional_argc`` is present, then an additional
``uint8_t _argc`` parameter is added at the end which receives the number of
optional arguments that were actually used (obviously, you need to have an
optional argument in the first place). Note that if both properties are set,
the ``JSContext* cx`` is added first, followed by the ``uint8_t _argc``, and
then ending with return value parameter. Finally, as an exception to everything
already mentioned, for attribute getters and setters the ``JSContext *cx``
comes before any other arguments.

Another native-only property is ``nostdcall``. Normally, declarations are made
in the stdcall ABI on Windows to be ABI-compatible with COM interfaces. Any
non-scriptable method or attribute with ``nostdcall`` instead uses the
``thiscall`` ABI convention. Methods without this property generally use
``NS_IMETHOD`` in their declarations and ``NS_IMETHODIMP`` in their definitions
to automatically add in the stdcall declaration specifier on requisite
compilers; those that use this method may use a plain ``nsresult`` instead.

Another property, ``infallible``, is attribute-only. When present, it causes an
infallible C++ getter function definition to be generated for the attribute
alongside the normal fallible C++ getter declaration. It should only be used if
the fallible getter will be infallible in practice (i.e. always return
``NS_OK``) for all possible implementations. This infallible getter contains
code that calls the fallible getter, asserts success, and returns the gotten
value directly. The point of using this property is to make C++ code nicer -- a
call to the infallible getter is more concise and readable than a call to the
fallible getter. This property can only be used for attributes having built-in
or interface types, and within classes that are marked with ``builtinclass``.
The latter restriction is because C++ implementations of fallible getters can
be audited for infallibility, but JS implementations can always throw (e.g. due
to OOM).

The ``must_use`` property is useful if the result of a method call or an
attribute get/set should always (or usually) be checked, which is frequently
the case.  (e.g. a method that opens a file should almost certainly have its
result checked.) This property will cause ``[[nodiscard]]`` to be added to the
generated function declarations, which means certain compilers (e.g. clang and
GCC) will reports errors if these results are not used.

Method Parameters
~~~~~~~~~~~~~~~~~

Each method parameter can be specified in one of three modes: ``in``, ``out``,
or ``inout``. An ``out`` parameter is essentially an auxiliary return value,
although these are moderately cumbersome to use from script contexts and should
therefore be avoided if reasonable. An ``inout`` parameter is an in parameter
whose value may be changed as a result of the method; these parameters are
rather annoying to use and should generally be avoided if at all possible.

``out`` and ``inout`` parameters are reflected as objects having the ``.value``
property which contains the real value of the parameter; the ``value``
attribute is missing in the case of ``out`` parameters and is initialized to
the passed-in-value for ``inout`` parameters. The script code needs to set this
property to assign a value to the parameter. Regular ``in`` parameters are
reflected more or less normally, with numeric types all representing numbers,
booleans as ``true`` or ``false``, the various strings (including ``AString``
etc.) as a JavaScript string, and ``nsID`` types as a ``Components.ID``
instance. In addition, the ``jsval`` type is translated as the appropriate
JavaScript value (since a ``jsval`` is the internal representation of all
JavaScript values), and parameters with the ``nsIVeriant`` interface have their
types automatically boxed and unboxed as appropriate.

The equivalent representations of all IDL types in native code is given in the
earlier tables; parameters of type ``inout`` follow their ``out`` form. Native
code should pay particular attention to not passing in null values for out
parameters (although some parts of the codebase are known to violate this, it
is strictly enforced at the JS<->native barrier).

Representations of types additionally depend on some of the many types of
properties they may have. The ``array`` property turns the parameter into an array;
the parameter must also have a corresponding ``size_is`` property whose argument is
the parameter that has the size of the array. In native code, the type gains
another pointer indirection, and JavaScript arrays are used in script code.
Script code callers can ignore the value of array parameter, but implementers
must still set the values appropriately.

.. note::

    Prefer using the ``Array<T>`` builtin over the ``[array]`` attribute for
    new code. It is more ergonomic to use from both JS and C++. In the future,
    ``[array]`` may be deprecated and removed.

The ``const`` and ``shared`` properties are special to native code. As its name
implies, the ``const`` property makes its corresponding argument ``const``. The
``shared`` property is only meaningful for ``out`` or ``inout`` parameters and
it means that the pointer value should not be freed by the caller. Only simple
native pointer types like ``string``, ``wstring``, and ``octetPtr`` may be
declared shared.  The shared property also makes its corresponding argument
const.

The ``retval`` property indicates that the parameter is actually acting as the
return value, and it is only the need to assign properties to the parameter
that is causing it to be specified as a parameter. It has no effect on native
code, but script code uses it like a regular return value. Naturally, a method
which contains a ``retval`` parameter must be declared ``void``, and the
parameter itself must be an ``out`` parameter and the last parameter.

Other properties are the ``optional`` and ``iid_is`` property. The ``optional``
property indicates that script code may omit the property without problems; all
subsequent parameters must either by optional themselves or the retval
parameter. Note that optional out parameters still pass in a variable for the
parameter, but its value will be ignored. The ``iid_is`` parameter indicates
that the real IID of an ``nsQIResult`` parameter may be found in the
corresponding parameter, to allow script code to automatically unbox the type.

Not all type combinations are possible. Native types with the various string
properties are all forbidden from being used as an ``inout`` parameter or as an
``array`` parameter. In addition, native types with the ``nsid`` property but
lacking either a ``ptr`` or ``ref`` property are forbidden unless the method is
``notxpcom`` and it is used as an ``in`` parameter.

Ownership Rules
```````````````

For types that reference heap-allocated data (strings, arrays, interface
pointers, etc), you must follow the XPIDL data ownership conventions in order
to avoid memory corruption and security vulnerabilities:

* For ``in`` parameters, the caller allocates and deallocates all data. If the
  callee needs to use the data after the call completes, it must make a private
  copy of the data, or, in the case of interface pointers, ``AddRef`` it.
* For ``out`` parameters, the callee creates the data, and transfers ownership
  to the caller. For buffers, the callee allocates the buffer with ``malloc``,
  and the caller frees the buffer with ``free``. For interface pointers, the
  callee does the ``AddRef`` on behalf of the caller, and the caller must call
  ``Release``. This manual reference/memory management should be performed
  using the ``getter_AddRefs`` and ``getter_Transfers`` helpers in new code.
* For ``inout`` parameters, the callee must clean up the old data if it chooses
  to replace it. Buffers must be deallocated with ``free``, and interface
  pointers must be ``Release``'d. Afterwards, the above rules for ``out``
  apply.
* ``shared`` out-parameters should not be freed, as they are intended to refer
  to constant string literals.
