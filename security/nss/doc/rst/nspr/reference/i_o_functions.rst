This chapter describes the NSPR functions used to perform operations
such as system access, normal file I/O, and socket (network) I/O.

For sample code that illustrates basic I/O operations, see `Introduction
to NSPR <Introduction_to_NSPR>`__. For information about the types most
commonly used with the functions described in this chapter, see `I/O
Types <I%2fO_Types>`__.

-  `Functions that Operate on
   Pathnames <#Functions_that_Operate_on_Pathnames>`__
-  `Functions that Act on File
   Descriptors <#Functions_that_Act_on_File_Descriptors>`__
-  `Directory I/O Functions <#Directory_I/O_Functions>`__
-  `Socket Manipulation Functions <#Socket_Manipulation_Functions>`__
-  `Converting Between Host and Network
   Addresses <#Converting_Between_Host_and_Network_Addresses>`__
-  `Memory-Mapped I/O Functions <#Memory-Mapped_I/O_Functions>`__
-  `Anonymous Pipe Function <#Anonymous_Pipe_Function>`__
-  `Polling Functions <#Polling_Functions>`__
-  `Pollable Events <#Pollable_Events>`__
-  `Manipulating Layers <#Manipulating_Layers>`__

.. _Functions_that_Operate_on_Pathnames:

Functions that Operate on Pathnames
-----------------------------------

A file or directory in a file system is specified by its pathname. NSPR
uses Unix-style pathnames, which are null-terminated character strings.
Only the ASCII character set is supported. The forward slash (/)
separates the directories in a pathname. NSPR converts the slashes in a
pathname to the directory separator of the native OS--for example,
backslash (\) on Windows and colon (:) on Mac OS--before passing it to
the native system calls.

Some file systems also differentiate drives or volumes.

-  ``PR_Open``
-  ``PR_Delete``
-  ``PR_GetFileInfo``
-  ``PR_GetFileInfo64``
-  ``PR_Rename``
-  ``PR_Access``

   -  type ``PRAccessHow``

.. _Functions_that_Act_on_File_Descriptors:

Functions that Act on File Descriptors
--------------------------------------

-  ``PR_Close``
-  ``PR_Read``
-  ``PR_Write``
-  ``PR_Writev``
-  ``PR_GetOpenFileInfo``
-  ``PR_GetOpenFileInfo64``
-  ``PR_Seek``
-  ``PR_Seek64``
-  ``PR_Available``
-  ``PR_Available64``
-  ``PR_Sync``
-  ``PR_GetDescType``
-  ``PR_GetSpecialFD``
-  ``PR_CreatePipe``

.. _Directory_I.2FO_Functions:

Directory I/O Functions
-----------------------

-  ``PR_OpenDir``
-  ``PR_ReadDir``
-  ``PR_CloseDir``
-  ``PR_MkDir``
-  ``PR_RmDir``

.. _Socket_Manipulation_Functions:

Socket Manipulation Functions
-----------------------------

The network programming interface presented here is a socket API modeled
after the popular Berkeley sockets. Differences include the following:

-  The blocking socket functions in NSPR take a timeout parameter.
-  Two new functions, ``PR_TransmitFile`` and ``PR_AcceptRead``, can
   exploit the new system calls of some operating systems for higher
   performance.

List of functions:

-  ``PR_OpenUDPSocket``
-  ``PR_NewUDPSocket`` ` <PR_NewUDPSocket>`__
-  ``PR_OpenTCPSocket``
-  ``PR_NewTCPSocket``
-  ``PR_ImportTCPSocket``
-  ``PR_Connect``
-  ``PR_ConnectContinue``
-  ``PR_Accept``
-  ``PR_Bind``
-  ``PR_Listen``
-  ``PR_Shutdown``
-  ``PR_Recv``
-  ``PR_Send``
-  ``PR_RecvFrom``
-  ``PR_SendTo``
-  ``PR_TransmitFile``
-  ``PR_AcceptRead``
-  ``PR_GetSockName``
-  ``PR_GetPeerName``
-  ``PR_GetSocketOption``
-  ``PR_SetSocketOption``

.. _Converting_Between_Host_and_Network_Addresses:

Converting Between Host and Network Addresses
---------------------------------------------

-  ``PR_ntohs``
-  ``PR_ntohl``
-  ``PR_htons``
-  ``PR_htonl``
-  ``PR_FamilyInet``

.. _Memory-Mapped_I.2FO_Functions:

Memory-Mapped I/O Functions
---------------------------

The memory-mapped I/O functions allow sections of a file to be mapped to
memory regions, allowing read-write accesses to the file to be
accomplished by normal memory accesses.

Memory-mapped I/O functions are currently implemented for Unix, Linux,
Mac OS X, and Win32 only.

-  ``PR_CreateFileMap``
-  ``PR_MemMap``
-  ``PR_MemUnmap``
-  ``PR_CloseFileMap``

.. _Anonymous_Pipe_Function:

Anonymous Pipe Function
-----------------------

-  ``PR_CreatePipe``

.. _Polling_Functions:

Polling Functions
-----------------

This section describes two of the most important polling functions
provided by NSPR:

-  ``PR_Poll``
-  ``PR_GetConnectStatus``

.. _Pollable_Events:

Pollable Events
---------------

A pollable event is a special kind of file descriptor. The only I/O
operation you can perform on a pollable event is to poll it with the
``PR_POLL_READ`` flag. You cannot read from or write to a pollable
event.

The purpose of a pollable event is to combine event waiting with I/O
waiting in a single ``PR_Poll`` call. Pollable events are implemented
using a pipe or a pair of TCP sockets connected via the loopback
address, therefore setting and/or waiting for pollable events are
expensive operating system calls. Do not use pollable events for general
thread synchronization; use condition variables instead.

A pollable event has two states: set and unset. Events are not queued,
so there is no notion of an event count. A pollable event is either set
or unset.

-  ``PR_NewPollableEvent``
-  ``PR_DestroyPollableEvent``
-  ``PR_SetPollableEvent``
-  ``PR_WaitForPollableEvent``

One can call ``PR_Poll`` with the ``PR_POLL_READ`` flag on a pollable
event. Whe the pollable event is set, ``PR_Poll`` returns the the
``PR_POLL_READ`` flag set in the out_flags.

.. _Manipulating_Layers:

Manipulating Layers
-------------------

File descriptors may be layered. For example, SSL is a layer on top of a
reliable bytestream layer such as TCP.

Each type of layer has a unique identity, which is allocated by the
runtime. The layer implementor should associate the identity with all
layers of that type. It is then possible to scan the chain of layers and
find a layer that one recognizes and therefore predict that it will
implement a desired protocol.

A layer can be pushed onto or popped from an existing stack of layers.
The file descriptor of the top layer can be passed to NSPR I/O
functions, which invoke the appropriate version of the I/O methods
polymorphically.

NSPR defines three identities:

.. code:: eval

   #define PR_INVALID_IO_LAYER (PRDescIdentity)-1
   #define PR_TOP_IO_LAYER (PRDescIdentity)-2
   #define PR_NSPR_IO_LAYER (PRDescIdentity)0

-  ``PR_INVALID_IO_LAYER``: An invalid layer identify (for error
   return).
-  ``PR_TOP_IO_LAYER``: The identity of the top of the stack.
-  ``PR_NSPR_IO_LAYER``: The identity for the layer implemented by NSPR.

``PR_TOP_IO_LAYER`` may be used as a shorthand for identifying the
topmost layer of an existing stack. For example, the following lines of
code are equivalent:

| ``rv = PR_PushIOLayer(stack, PR_TOP_IO_LAYER, my_layer);``
| ``rv = PR_PushIOLayer(stack, PR_GetLayersIdentity(stack), my_layer);``

-  ``PR_GetUniqueIdentity``
-  ``PR_GetNameForIdentity``
-  ``PR_GetLayersIdentity``
-  ``PR_GetIdentitiesLayer``
-  ``PR_GetDefaultIOMethods``
-  ``PR_CreateIOLayerStub``
-  ``PR_PushIOLayer``
-  ``PR_PopIOLayer``
