==============
Prompt Service
==============

The `nsIPromptService` provides methods for opening various types of prompts.
See the `interface documentation <nsIPromptService-reference.html>`_ for a list
of prompt types.
Every prompt method has 3 different versions:

- **Prompt by window**:
  This is considered the legacy way of prompting and only works if the window
  you want to prompt for is in the same process.
  Only supports window prompts.

- **Prompt by browsing context (synchronous)**:
  Use a browsing context as parent for the prompt. Works cross process from
  parent and content process.

- **Prompt by browsing context (asynchronous)**:
  Returns a promise which resolves once the prompt closes.


The synchronous prompt methods use call by reference (XPCOM `inout` or `out`) to
return the updated prompt arguments to the caller.
When prompting async the arguments are passed in by value. Prompt results are
returned in an `nsIPropertyBag` when the Promise resolves.


.. note::
    If you don't provide a parent window or browsing context the prompt service
    will fallback to a window prompt.
    The same goes for browsing contexts of chrome windows, because there is no
    clear association to a browser / tab.


Examples
--------

JavaScript Sync
~~~~~~~~~~~~~~~

Here is an example of opening a confirm prompt from JavaScript. We are in the
parent process and we want to show a tab prompt:

.. code-block:: javascript

    // Get the browsing context for the currently selected tab
    let browsingContext = gBrowser.selectedBrowser.browsingContext;

    // Specify prompt type, can be MODAL_TYPE_TAB, MODAL_TYPE_CONTENT,
    // MODAL_TYPE_WINDOW
    let modalType = Services.prompt.MODAL_TYPE_TAB;

    // Object for checkbox state to pass by reference.
    let check = { value: false };

    // Prompt synchronously and store result
    let confirmed = Services.prompt.confirmCheckBC(browsingContext, modalType,
    "My Title", "Hello, World!", "Check this box if you agree", check);

    // check.value now contains final checkbox state
    // confirmed is a boolean which indicates whether the user pressed ok (true)
    // or cancel (false)
    console.debug("User checked checkbox?", check.value);
    console.debug("User confirmed prompt?", confirmed);


JavaScript Async
~~~~~~~~~~~~~~~~

The same prompt as above, but called async:

.. code-block:: javascript

    // Get the browsing context for the currently selected tab
    let browsingContext = gBrowser.selectedBrowser.browsingContext;

    // Specify prompt type, can be MODAL_TYPE_TAB, MODAL_TYPE_CONTENT,
    // MODAL_TYPE_WINDOW
    let modalType = Services.prompt.MODAL_TYPE_TAB;

    // Note that the checkbox state variable is not an object in this case,
    because we get the checkbox result via the result object.
    let check = false;

    // Prompt asynchronously and await result
    let propBag = await Services.prompt.asyncConfirmCheck(browsingContext,
                                                          modalType,
                                                          "My Title",
                                                          "Hello, World!",
                                                          "Check this box if you agree",
                                                          check);

    let ok = propBag.getProperty("ok");
    let checked = propBag.getProperty("checked");

    // ok is the boolean indicating if the user clicked "ok" (true) or
    // "cancel" (false).
    // checked is a boolean indicating the final checkbox state
    console.debug("User checked checkbox?", checked);
    console.debug("User confirmed prompt?", ok);


C++ Sync
~~~~~~~~

.. code-block:: cpp

      nsCOMPtr<nsIPromptService> promptSvc =
        do_GetService("@mozilla.org/prompter;1");

      if(!promptSvc) {
        // Error handling
        return;
      }

      // Assuming you have the browsing context as a member.
      // You might need to get the browsing context from somewhere else.
      BrowsingContext* bc = mBrowsingContext;

      bool ok;
      bool checked = false;

      nsresult rv = promptSvc->confirmCheck(mBrowsingContext,
                                            nsIPromptService::MODAL_TYPE_TAB,
                                            "My Title"_ns
                                            "Hello, World!"_ns,
                                            "Check this box if you agree"_ns,
                                            &checked, &ok);

      // ok is the boolean indicating if the user clicked "ok" (true) or
      // "cancel" (false).
      // checked is a boolean indicating the final checkbox state

C++ Async
~~~~~~~~~

.. code-block:: cpp

      nsCOMPtr<nsIPromptService> promptSvc =
        do_GetService("@mozilla.org/prompter;1");

      if(!promptSvc) {
        // Error handling
        return;
      }

      bool checked = false;
      Promise* promise;

      // Assuming you have the browsing context as a member.
      // You might need to get the browsing context from somewhere else.
      BrowsingContext* bc = mBrowsingContext;

      // As opposed to the sync case, here we pass the checked flag by value
      nsresult rv = promptSvc->confirmCheckAsync(mBrowsingContext,
                                                 nsIPromptService::MODAL_TYPE_TAB, "My Title"_ns,
                                                 "Hello, World!"_ns,
                                                 "Check this box if you agree"_ns,
                                                 checked, promise);

      // Attach a promise handler
      RefPtr<PromptHandler> handler = new PromptHandler(promise);
      promise->AppendNativeHandler(handler);

Then, in your promise handler callback function:

.. code-block:: cpp

      void PromptHandler::ResolvedCallback(JSContext* aCx,
                                       JS::Handle<JS::Value> aValue) {
        JS::Rooted<JSObject*> detailObj(aCx, &aValue.toObject());

        // Convert the JSObject back to a property bag
        nsresult rv;
        nsCOMPtr<nsIPropertyBag2> propBag;
        rv = UnwrapArg<nsIPropertyBag2>(aCx, detailObj, getter_AddRefs(propBag));
        if (NS_FAILED(rv)) return;

        bool ok;
        bool checked;
        propBag->GetPropertyAsBool(u"ok"_ns, &ok);
        propBag->GetPropertyAsBool(u"checked"_ns, &checked);

        // ok is the boolean indicating if the user clicked "ok" (true) or
        // "cancel" (false).
        // checked is a boolean indicating the final checkbox state.
      }





For a full list of prompt methods check
`nsIPromptService reference <nsIPromptService-reference.html>`_.
