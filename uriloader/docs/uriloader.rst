.. _uri_loader_service:

URI Loader Service
==================

As its name might suggest the URI loader service is responsible for loading URIs
but it is also responsible for deciding how to handle that content, whether to
display it as part of a DOM window or hand it off to something else.

It is generally used when loading content for display to the user, normally from
``nsDocShell`` for display as a webpage or ``nsObjectLoadingContent`` for display inside
a webpage's ``<object>`` tag. The normal entrypoint is through ``nsIURILoader::OpenURI``.

The URI loader starts the load and registers an ``nsDocumentOpenInfo`` as a stream
listener for the content. Once headers have been received `DispatchContent <https://searchfox.org/mozilla-central/search?q=nsDocumentOpenInfo%3A%3ADispatchContent&path=>`_
then decides what to do with the content as it may need to be handled by something
other than the caller. It uses a few criteria to decide this including:

* Content-Type header.
* Content-Disposition header.
* Load flags.

Part of this handling may include running the content through a registered stream
converter to convert the content type from one to another. This is done through
the `stream converter service <https://searchfox.org/mozilla-central/source/netwerk/streamconv>`_.
When this happens a new ``nsDocumentOpenInfo`` is created to handle the new content
in the same way as the current content.

The rough flow goes as follows (though note that this can vary depending on the
flags passed to the loader service):

1. The caller may provide an ``nsIURIContentListener`` which can offer to handle
   the content type or a content type that we can convert the original type to).
   If so the load is passed off to the listener.
2. Global instances of ``nsIURIContentListener`` can be registered with the URI
   loader service so these are consulted in the same way.
3. Global instances of ``nsIURIContentListener`` can be registered in the category
   manager so these are consulted in the same way.
4. Global instances of ``nsIContentHandler`` can be registered. If one agrees to
   handle the content then the load is handed over to it.
5. We attempt to convert the content to a different type.
6. The load is handed over to the :ref:`External Helper App Service <external_helper_app_service>`.

For the most part the process ends at step 1 because nsDocShell passes a ``nsDSURIContentListener``
for the ``nsIURIContentListener`` consulted first and it accepts most of the
`web content types <https://searchfox.org/mozilla-central/source/layout/build/components.conf>`_.
