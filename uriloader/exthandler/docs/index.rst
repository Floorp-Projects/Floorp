.. _external_helper_app_service:

External Helper App Service
===========================

The external helper app service is responsible for deciding how to handle an
attempt to load come content that cannot be loaded by the browser itself.

Part of this involves using the Handler Service which manages the users
preferences for what to do by default with different content.

When a Link is Clicked
----------------------

When a link in a page is clicked (or a form submitted) ``nsDocShell`` tests
whether the target protocol can be loaded by the browser itself, this is based
on the preferences under ``network.protocol-handler``. When the browser cannot
load the protocol it calls into ``nsExternalHelperAppService::LoadURI``.

Some validation checks are performed but ultimateley we look for a registered
protocol handler. First the OS is queried for an app registration for the
protocol and then the handler server is asked to fill in any user settings from
the internal database. If there were no settings from the handler service then
some defaults are applied in ``nsExternalHelperAppService::SetProtocolHandlerDefaults``.

If there is a default handler app chosen and the settings say to use it without
asking then that happens. If not a dialog s shown asking the user what they
want to do.

During a Load
-------------

When content is already being loaded the :ref:`URI Loader Service <uri_loader_service>`
determines whether the browser can handle the content or not. If not it calls
into the external helper app server through ``nsExternalHelperAppService::DoContent``.

The content type of the loading content is retrieved from the channel. A file
extension is also generated using the Content-Disposition header or if the load
is not a HTTP POST request the file extension is generated from the requested URL.

We then query the MIME Service for an nsIMIMEInfo to find information about
apps that can handle the content type or file extension based on OS and user
settings, :ref:`see below <mime_service>` for further details. The result is
used to create a ``nsExternalAppHandler`` which is then used as a stream listener
for the content.

The MIME info object contains settings that control whether to prompt the user
before doing anything and what the default action should be. If we need to ask
the user then a dialog is shown offering to let users cancel the load, save the
content to disk or send it to a registered application handler.

Assuming the load isn't canceled the content is streamed to disk using a background
file saver with a target ``nsITransfer``. The ``nsITransfer`` is responsible for
showing the download in the UI.

If the user opted to open the file with an application then once the transfer is
complete then ``nsIMIMEInfo::LaunchWithFile`` is used to
`launch the application <https://searchfox.org/mozilla-central/search?q=nsIMIMEInfo%3A%3ALaunchWithFile&path=>`_.

.. _mime_service:

MIME Service
------------

The MIME service is responsible for getting an ``nsIMIMEInfo`` object for a
content type or file extension:

1. Fills out an ``nsIMIMEInfo`` based on OS provided information. This is platform
   specific but should try to find the default application registered to handle
   the content.
2. Ask the handler service to fill out the ``nsIMIMEInfo`` with information held
   in browser settings. This will not overwrite a any application found from
   the OS.
3. If one has not been found already then try to find a type description from
   a `lookup table <https://searchfox.org/mozilla-central/search?q=extraMimeEntries[]&path=>`_
   or just by appending " File" to the file extension.
