This chapter describes the most common NSPR types, enumerations, and
structures used with the functions described in `I/O
Functions <I%2f%2fO_Functions>`__ and `Network
Addresses <Network_Addresses>`__. These include the types used for
system access, normal file I/O, and socket (network) I/O.

Types unique to a particular function are described with the function
itself.

For sample code that illustrates basic I/O operations, see `Introduction
to NSPR <Introduction_to_NSPR>`__.

-  `Directory Type <#Directory_Type>`__
-  `File Descriptor Types <#File_Descriptor_Types>`__
-  `File Info Types <#File_Info_Types>`__
-  `Network Address Types <#Network_Address_Types>`__
-  `Types Used with Socket Options
   Functions <#Types_Used_with_Socket_Options_Functions>`__
-  `Type Used with Memory-Mapped
   I/O <#Type_Used_with_Memory-Mapped_I/O>`__
-  `Offset Interpretation for Seek
   Functions <#Offset_Interpretation_for_Seek_Functions>`__

.. _Directory_Type:

Directory Type
--------------

-  ``PRDir``

.. _File_Descriptor_Types:

File Descriptor Types
---------------------

NSPR represents I/O objects, such as open files and sockets, by file
descriptors of type ``PRFileDesc``. This section introduces
``PRFileDesc`` and related types.

-  ``PRFileDesc``
-  ``PRIOMethods``
-  ``PRFilePrivate``
-  ``PRDescIdentity``

Note that the NSPR documentation follows the Unix convention of using
the term\ *files* to refer to many kinds of I/O objects. To refer
specifically to the files in a file system (that is, disk files), this
documentation uses the term\ *normal files*.

``PRFileDesc`` has an object-oriented flavor. An I/O function on a
``PRFileDesc`` structure is carried out by invoking the corresponding
"method" in the I/O methods table (a structure of type ``PRIOMethods``)
of the ``PRFileDesc`` structure (the "object"). Different kinds of I/O
objects (such as files and sockets) have different I/O methods tables,
thus implementing different behavior in response to the same I/O
function call.

NSPR supports the implementation of layered I/O. Each layer is
represented by a ``PRFileDesc`` structure, and the ``PRFileDesc``
structures for the layers are chained together. Each ``PRFileDesc``
structure has a field (of type ``PRDescIdentity``) to identify itself in
the layers. For example, the Netscape implementation of the Secure
Sockets Layer (SSL) protocol is implemented as an I/O layer on top of
NSPR's socket layer.

.. _File_Info_Types:

File Info Types
---------------

-  ``PRFileInfo``
-  ``PRFileInfo64``
-  ``PRFileType``

.. _Network_Address_Types:

Network Address Types
---------------------

-  ``PRNetAddr``
-  ``PRIPv6Addr``

.. _Types_Used_with_Socket_Options_Functions:

Types Used with Socket Options Functions
----------------------------------------

-  ``PRSocketOptionData``
-  ``PRSockOption``
-  ``PRLinger``
-  ``PRMcastRequest``

.. _Type_Used_with_Memory-Mapped_I.2FO:

Type Used with Memory-Mapped I/O
--------------------------------

-  ``PRFileMap``

.. _Offset_Interpretation_for_Seek_Functions:

Offset Interpretation for Seek Functions
----------------------------------------

-  ``PRSeekWhence``
