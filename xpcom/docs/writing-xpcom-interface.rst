.. _writing_xpcom_interface:

Tutorial for Writing a New XPCOM Interface
==========================================

High Level Overview
-------------------

In order to write code that works in native code (C++, Rust), and JavaScript contexts, it's necessary to have a mechanism to do so. For chrome privileged contexts, this is the XPCOM Interface Class.

This mechanism starts with an :ref:`XPIDL` file to define the shape of the interface. In the `build system`_, this file is processed, and `Rust`_ and `C++`_ code is automatically generated.

.. _build system: https://searchfox.org/mozilla-central/source/xpcom/idl-parser/xpidl
.. _Rust: https://searchfox.org/mozilla-central/source/__GENERATED__/dist/xpcrs/rt
.. _C++: https://searchfox.org/mozilla-central/source/__GENERATED__/dist/include

Next, the interface's methods and attributes must be implemented. This can be done through either a JSM module, or through a C++ interface class. Once these steps are done, the new files must be added to the appropriate :code:`moz.build` files to ensure the build system knows how to find them and process them.

Often these XPCOM components are wired into the :code:`Services` JavaScript object to allow for ergonomic access to the interface. For example, open the `Browser Console`_ and type :code:`Services.` to interactively access these components.

.. _Browser Console: https://developer.mozilla.org/en-US/docs/Tools/Browser_Console

From C++, components can be accessed via :code:`mozilla::components::ComponentName::Create()` using the :code:`name` option in the :code:`components.conf`.

While :code:`Services` and :code:`mozilla::components` are the preferred means of accessing components, many are accessed through the historical (and somewhat arcane) :code:`createInstance` mechanism. New usage of these mechanisms should be avoided if possible.

.. code:: javascript

    let component = Cc["@mozilla.org/component-name;1"].createInstance(
      Ci.nsIComponentName
    );

.. code:: c++

    nsCOMPtr<nsIComponentName> component = do_CreateInstance(
      "@mozilla.org/component-name;1");

Writing an XPIDL
----------------

First decide on a name. Conventionally the interfaces are prefixed with :code:`nsI` (historically Netscape) or :code:`mozI` as they are defined in the global namespace. While the interface is global, the implementation of an interface can be defined in a namespace with no prefix. Historically many component implementations still use the :code:`ns` prefixes (notice that the :code:`I` was dropped), but this convention is no longer needed.

This tutorial assumes the component is located at :code:`path/to` with the name :code:`ComponentName`. The interface name will be :code:`nsIComponentName`, while the implementation will be :code:`mozilla::ComponentName`.

To start, create an :ref:`XPIDL` file:

.. code:: bash

    touch path/to/nsIComponentName.idl

And hook it up to the :code:`path/to/moz.build`

.. code:: python

    XPIDL_SOURCES += [
        "nsIComponentName.idl",
    ]

Next write the initial :code:`.idl` file: :code:`path/to/nsIComponentName.idl`

.. _contract_ids:
.. code:: c++

    /* This Source Code Form is subject to the terms of the Mozilla Public
     * License, v. 2.0. If a copy of the MPL was not distributed with this
     * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

    // This is the base include which defines nsISupports. This class defines
    // the QueryInterface method.
    #include "nsISupports.idl"

    // `scriptable` designates that this object will be used with JavaScript
    // `uuid`       The example below uses a UUID with all Xs. Replace the Xs with
    //              your own UUID generated here:
    //              http://mozilla.pettay.fi/cgi-bin/mozuuid.pl

    /**
     * Make sure to document your interface.
     */
    [scriptable, uuid(xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)]
    interface nsIComponentName : nsISupports {

      // Fill out your definition here. This example attribute only returns a bool.

      /**
       * Make sure to document your attributes.
       */
      readonly attribute bool isAlive;
    };

This definition only includes one attribute, :code:`isAlive`, which will demonstrate that we've done our work correctly at the end. For a more comprehensive guide for this syntax, see the :ref:`XPIDL` docs.

Once :code:`./mach build` is run, the XPIDL parser will read this file, and give any warnings if the syntax is wrong. It will then auto-generate the C++ (or Rust) code for us. For this example the generated :code:`nsIComponentName` class will be located in:

:code:`{obj-directory}/dist/include/nsIComponentName.h`

It might be useful to check out what was automatically generated here, or see the existing `generated C++ header files on SearchFox <https://searchfox.org/mozilla-central/source/__GENERATED__/dist/>`_.

Writing the C++ implementation
------------------------------

Now we have a definition for an interface, but no implementation. The interface could be backed by a JavaScript implementation using a JSM, but for this example we'll use a C++ implementation.

Add the C++ sources to :code:`path/to/moz.build`

.. code:: python

    EXPORTS.mozilla += [
        "ComponentName.h",
    ]

    UNIFIED_SOURCES += [
        "ComponentName.cpp",
    ]

Now write the header: :code:`path/to/ComponentName.h`

.. code:: c++

    /* This Source Code Form is subject to the terms of the Mozilla Public
     * License, v. 2.0. If a copy of the MPL was not distributed with this
     * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
    #ifndef mozilla_nsComponentName_h__
    #define mozilla_nsComponentName_h__

    // This will pull in the header auto-generated by the .idl file:
    // {obj-directory}/dist/include/nsIComponentName.h
    #include "nsIComponentName.h"

    // The implementation can be namespaced, while the XPCOM interface is globally namespaced.
    namespace mozilla {

    // Notice how the class name does not need to be prefixed, as it is defined in the
    // `mozilla` namespace.
    class ComponentName final : public nsIComponentName {
      // This first macro includes the necessary information to use the base nsISupports.
      // This includes the QueryInterface method.
      NS_DECL_ISUPPORTS

      // This second macro includes the declarations for the attributes. There is
      // no need to duplicate these declarations.
      //
      // In our case it includes a declaration for the isAlive attribute:
      //   GetIsAlive(bool *aIsAlive)
      NS_DECL_NSICOMPONENTNAME

     public:
      ComponentName() = default;

     private:
      // A private destructor must be declared.
      ~ComponentName() = default;
    };

    }  // namespace mozilla

    #endif

Now write the definitions: :code:`path/to/ComponentName.cpp`

.. code:: c++

    /* This Source Code Form is subject to the terms of the Mozilla Public
     * License, v. 2.0. If a copy of the MPL was not distributed with this
     * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

    #include "ComponentName.h"

    namespace mozilla {

    // Use the macro to inject all of the definitions for nsISupports.
    NS_IMPL_ISUPPORTS(ComponentName, nsIComponentName)

    // This is the actual implementation of the `isAlive` attribute. Note that the
    // method name is somewhat different than the attribute. We specified "read-only"
    // in the attribute, so only a getter, not a setter was defined for us. Here
    // the name was adjusted to be `GetIsAlive`.
    //
    // Another common detail of implementing an XPIDL interface is that the return values
    // are passed as out parameters. The methods are treated as fallible, and the return
    // value is an `nsresult`. See the XPIDL documentation for the full nitty gritty
    // details.
    //
    // A common way to know the exact function signature for a method implementation is
    // to copy and paste from existing examples, or inspecting the generated file
    // directly: {obj-directory}/dist/include/nsIComponentName.h
    NS_IMETHODIMP
    ComponentName::GetIsAlive(bool* aIsAlive) {
      *aIsAlive = true;
      return NS_OK;
    }

    } // namespace: mozilla

Registering the component
-------------------------

At this point, the component should be correctly written, but it's not registered with the component system. In order to this, we'll need to create or modify the :code:`components.conf`.

.. code:: bash

    touch path/to/components.conf


Now update the :code:`moz.build` to point to it.

.. code:: python

    XPCOM_MANIFESTS += [
        "components.conf",
    ]

It is probably worth reading over :ref:`defining_xpcom_components`, but the following config will be sufficient to hook up our component to the :code:`Services` object.
Services should also be added to ``tools/lint/eslint/eslint-plugin-mozilla/lib/services.json``.
The easiest way to do that is to copy from ``<objdir>/xpcom/components/services.json``.

.. code:: python

    Classes = [
        {
            # This CID is the ID for component entries, and needs a separate UUID from
            # the .idl file. Replace the Xs with a uuid from:
            # http://mozilla.pettay.fi/cgi-bin/mozuuid.pl
            'cid': '{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}',
            'interfaces': ['nsIComponentName'],

            # A contract ID is a human-readable identifier for an _implementation_ of
            # an XPCOM interface.
            #
            # "@mozilla.org/process/environment;1"
            #  ^^^^^^^^^^^^ ^^^^^^^ ^^^^^^^^^^^ ^
            #  |            |       |           |
            #  |            |       |           The version number, usually just 1.
            #  |            |       Component name
            #  |            Module
            #  Domain
            #
            # This design goes back to a time when XPCOM was intended to be a generalized
            # solution for the Gecko Runtime Environment (GRE). At this point most (if
            # not all) of mozilla-central has an @mozilla domain.
            'contract_ids': ['@mozilla.org/component-name;1'],

            # This is the name of the C++ type that implements the interface.
            'type': 'mozilla::ComponentName',

            # The header file to pull in for the implementation of the interface.
            'headers': ['path/to/ComponentName.h'],

            # In order to hook up this interface to the `Services` object, we can
            # provide the "js_name" parameter. This is an ergonomic way to access
            # the component.
            'js_name': 'componentName',
        },
    ]

At this point the full :code:`moz.build` file should look like:

.. code:: python

    # -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
    # vim: set filetype=python:
    # This Source Code Form is subject to the terms of the Mozilla Public
    # License, v. 2.0. If a copy of the MPL was not distributed with this
    # file, You can obtain one at http://mozilla.org/MPL/2.0/.

    XPIDL_SOURCES += [
        "nsIComponentName.idl",
    ]

    XPCOM_MANIFESTS += [
        "components.conf",
    ]

    EXPORTS.mozilla += [
        "ComponentName.h",
    ]

    UNIFIED_SOURCES += [
        "ComponentName.cpp",
    ]

This completes the implementation of a basic XPCOM Interface using C++. The component should be available via the `Browser Console`_ or other chrome contexts.

.. code:: javascript

    console.log(Services.componentName.isAlive);
    > true
