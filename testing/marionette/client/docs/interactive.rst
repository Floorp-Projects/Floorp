Using the Client Interactively
==============================

Once you installed the client and have Marionette running, you can fire
up your favourite interactive python environment and start playing with
Marionette. Let's use a typical python shell:

.. parsed-literal::
   python

First, import Marionette:

.. parsed-literal::
   from marionette_driver.marionette import Marionette

Now create the client for this session. Assuming you're using the default
port on a Marionette instance running locally:

.. parsed-literal::
   client = Marionette(host='127.0.0.1', port=2828)
   client.start_session()

This will return some id representing your session id. Now that you've
established a connection, let's start doing interesting things:

.. parsed-literal::
   client.navigate("http://www.mozilla.org")

Now you're at mozilla.org! You can even verify it using the following:

.. parsed-literal::
   client.get_url()

You can execute Javascript code in the scope of the web page:

.. parsed-literal::
   client.execute_script("return window.document.title;")

This will you return the title of the web page as set in the head section
of the HTML document.

Also you can find elements and click on those. Let's say you want to get
the first link:

.. parsed-literal::
   from marionette_driver import By
   first_link = client.find_element(By.TAG_NAME, "a")

first_link now holds a reference to the first link on the page. You can click it:

.. parsed-literal::
   first_link.click()
