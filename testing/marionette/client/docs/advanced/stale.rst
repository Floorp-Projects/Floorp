Dealing with Stale Elements
===========================
.. py:currentmodule:: marionette_driver.marionette

Marionette does not keep a live representation of the DOM saved. All it can do
is send commands to the Marionette server which queries the DOM on the client's
behalf. References to elements are also not passed from server to client. A
unique id is generated for each element that gets referenced and a mapping of
id to element object is stored on the server. When commands such as
:func:`~WebElement.click` are run, the client sends the element's id along
with the command. The server looks up the proper DOM element in its reference
table and executes the command on it.

In practice this means that the DOM can change state and Marionette will never
know until it sends another query. For example, look at the following HTML::

    <head>
    <script type=text/javascript>
        function addDiv() {
           var div = document.createElement("div");
           document.getElementById("container").appendChild(div);
        }
    </script>
    </head>

    <body>
        <div id="container">
        </div>
        <input id="button" type=button onclick="addDiv();">
    </body>

Care needs to be taken as the DOM is being modified after the page has loaded.
The following code has a race condition::

    button = client.find_element('id', 'button')
    button.click()
    assert len(client.find_elements('css selector', '#container div')) > 0


Explicit Waiting and Expected Conditions
----------------------------------------
.. py:currentmodule:: marionette_driver

To avoid the above scenario, manual synchronisation is needed. Waits are used
to pause program execution until a given condition is true. This is a useful
technique to employ when documents load new content or change after
``Document.readyState``'s value changes to "complete".

The :class:`Wait` helper class provided by Marionette avoids some of the
caveats of ``time.sleep(n)``. It will return immediately once the provided
condition evaluates to true.

To avoid the race condition in the above example, one could do::

    from marionette_driver import Wait

    button = client.find_element('id', 'button')
    button.click()

    def find_divs():
        return client.find_elements('css selector', '#container div')

    divs = Wait(client).until(find_divs)
    assert len(divs) > 0

This avoids the race condition. Because finding elements is a common condition
to wait for, it is built in to Marionette. Instead of the above, you could
write::

    from marionette_driver import Wait

    button = client.find_element('id', 'button')
    button.click()
    assert len(Wait(client).until(expected.elements_present('css selector', '#container div'))) > 0

For a full list of built-in conditions, see :mod:`~marionette_driver.expected`.
