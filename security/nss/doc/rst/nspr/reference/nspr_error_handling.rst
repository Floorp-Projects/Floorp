This chapter describes the functions for retrieving and setting errors
and the error codes set by NSPR.

-  `Error Type <#Error_Type>`__
-  `Error Functions <#Error_Functions>`__
-  `Error Codes <#Error_Codes>`__

For information on naming conventions for NSPR types, functions, and
macros, see `NSPR Naming
Conventions <Introduction_to_NSPR#NSPR_Naming_Conventions>`__.

.. _Error_Type:

Error Type
----------

 - :ref:`PRErrorCode`

.. _Error_Functions:

Error Functions
---------------

 - :ref:`PR_SetError`
 - :ref:`PR_SetErrorText`
 - :ref:`PR_GetError`
 - :ref:`PR_GetOSError`
 - :ref:`PR_GetErrorTextLength`
 - :ref:`PR_GetErrorText`

.. _Error_Codes:

Error Codes
-----------

Error codes defined in ``prerror.h``:

``PR_OUT_OF_MEMORY_ERROR``
   Insufficient memory to perform request.
``PR_BAD_DESCRIPTOR_ERROR``
   The file descriptor used as an argument in the preceding function is
   invalid.
``PR_WOULD_BLOCK_ERROR``
   The operation would have blocked, which conflicts with the semantics
   that have been established.
``PR_ACCESS_FAULT_ERROR``
   One of the arguments of the preceding function specified an invalid
   memory address.
``PR_INVALID_METHOD_ERROR``
   The preceding function is invalid for the type of file descriptor
   used.
``PR_ILLEGAL_ACCESS_ERROR``
   One of the arguments of the preceding function specified an invalid
   memory address.
``PR_UNKNOWN_ERROR``
   Some unknown error has occurred.
``PR_PENDING_INTERRUPT_ERROR``
   The operation terminated because another thread has interrupted it
   with :ref:`PR_Interrupt`.
``PR_NOT_IMPLEMENTED_ERROR``
   The preceding function has not been implemented.
``PR_IO_ERROR``
   The preceding I/O function encountered some sort of an error, perhaps
   an invalid device.
``PR_IO_TIMEOUT_ERROR``
   The I/O operation has not completed in the time specified for the
   preceding function.
``PR_IO_PENDING_ERROR``
   An I/O operation has been attempted on a file descriptor that is
   currently busy with another operation.
``PR_DIRECTORY_OPEN_ERROR``
   The directory could not be opened.
``PR_INVALID_ARGUMENT_ERROR``
   One or more of the arguments to the function is invalid.
``PR_ADDRESS_NOT_AVAILABLE_ERROR``
   The network address (:ref:`PRNetAddr`) is not available (probably in
   use).
``PR_ADDRESS_NOT_SUPPORTED_ERROR``
   The type of network address specified is not supported.
``PR_IS_CONNECTED_ERROR``
   An attempt to connect on an already connected network file
   descriptor.
``PR_BAD_ADDRESS_ERROR``
   The network address specified is invalid (as reported by the
   network).
``PR_ADDRESS_IN_USE_ERROR``
   Network address specified (:ref:`PRNetAddr`) is in use.
``PR_CONNECT_REFUSED_ERROR``
   The peer has refused to allow the connection to be established.
``PR_NETWORK_UNREACHABLE_ERROR``
   The network address specifies a host that is unreachable (perhaps
   temporary).
``PR_CONNECT_TIMEOUT_ERROR``
   The connection attempt did not complete in a reasonable period of
   time.
``PR_NOT_CONNECTED_ERROR``
   The preceding function attempted to use connected semantics on a
   network file descriptor that was not connected.
``PR_LOAD_LIBRARY_ERROR``
   Failure to load a dynamic library.
``PR_UNLOAD_LIBRARY_ERROR``
   Failure to unload a dynamic library.
``PR_FIND_SYMBOL_ERROR``
   Symbol could not be found in the specified library.
``PR_INSUFFICIENT_RESOURCES_ERROR``
   There are insufficient system resources to process the request.
``PR_DIRECTORY_LOOKUP_ERROR``
   A directory lookup on a network address has failed.
``PR_TPD_RANGE_ERROR``
   Attempt to access a thread-private data index that is out of range of
   any index that has been allocated to the process.
``PR_PROC_DESC_TABLE_FULL_ERROR``
   The process' table for holding open file descriptors is full.
``PR_SYS_DESC_TABLE_FULL_ERROR``
   The system's table for holding open file descriptors has been
   exceeded.
``PR_NOT_SOCKET_ERROR``
   An attempt to use a non-network file descriptor on a network-only
   operation.
``PR_NOT_TCP_SOCKET_ERROR``
   Attempt to perform a TCP specific function on a non-TCP file
   descriptor.
``PR_SOCKET_ADDRESS_IS_BOUND_ERRO``
   Attempt to bind an address to a TCP file descriptor that is already
   bound.
``PR_NO_ACCESS_RIGHTS_ERROR``
   Calling thread does not have privilege to perform the operation
   requested.
``PR_OPERATION_NOT_SUPPORTED_ERRO``
   The requested operation is not supported by the platform.
``PR_PROTOCOL_NOT_SUPPORTED_ERROR``
   The host operating system does not support the protocol requested.
``PR_REMOTE_FILE_ERROR``
   Access to the remote file has been severed.
``PR_BUFFER_OVERFLOW_ERROR``
   The value retrieved is too large to be stored in the buffer provided.
``PR_CONNECT_RESET_ERROR``
   The (TCP) connection has been reset by the peer.
``PR_RANGE_ERROR``
   Unused.
``PR_DEADLOCK_ERROR``
   Performing the requested operation would have caused a deadlock. The
   deadlock was avoided.
``PR_FILE_IS_LOCKED_ERROR``
   An attempt to acquire a lock on a file has failed because the file is
   already locked.
``PR_FILE_TOO_BIG_ERROR``
   Completing the write or seek operation would have resulted in a file
   larger than the system could handle.
``PR_NO_DEVICE_SPACE_ERROR``
   The device for storing the file is full.
``PR_PIPE_ERROR``
   Unused.
``PR_NO_SEEK_DEVICE_ERROR``
   Unused.
``PR_IS_DIRECTORY_ERROR``
   Attempt to perform a normal file operation on a directory.
``PR_LOOP_ERROR``
   Symbolic link loop.
``PR_NAME_TOO_LONG_ERROR``
   Filename is longer than allowed by the host operating system.
``PR_FILE_NOT_FOUND_ERROR``
   The requested file was not found.
``PR_NOT_DIRECTORY_ERROR``
   Attempt to perform directory specific operations on a normal file.
``PR_READ_ONLY_FILESYSTEM_ERROR``
   Attempt to write to a read-only file system.
``PR_DIRECTORY_NOT_EMPTY_ERROR``
   Attempt to delete a directory that is not empty.
``PR_FILESYSTEM_MOUNTED_ERROR``
   Attempt to delete or rename a file object while the file system is
   busy.
``PR_NOT_SAME_DEVICE_ERROR``
   Request to rename a file to a file system on another device.
``PR_DIRECTORY_CORRUPTED_ERROR``
   The directory object in the file system is corrupted.
``PR_FILE_EXISTS_ERROR``
   Attempt to create or rename a file when the new name is already being
   used.
``PR_MAX_DIRECTORY_ENTRIES_ERROR``
   Attempt to add new filename to directory would exceed the limit
   allowed.
``PR_INVALID_DEVICE_STATE_ERROR``
   The device was in an invalid state to complete the desired operation.
``PR_DEVICE_IS_LOCKED_ERROR``
   The device needed to perform the desired request is locked.
``PR_NO_MORE_FILES_ERROR``
   There are no more entries in the directory.
``PR_END_OF_FILE_ERROR``
   Unexpectedly encountered end of file (Mac OS only).
``PR_FILE_SEEK_ERROR``
   An unexpected seek error (Mac OS only).
``PR_FILE_IS_BUSY_ERROR``
   The file is busy and the operation cannot be performed.
``PR_IN_PROGRESS_ERROR``
   The operation is still in progress (probably a nonblocking connect).
``PR_ALREADY_INITIATED_ERROR``
   The (retried) operation has already been initiated (probably a
   nonblocking connect).
``PR_GROUP_EMPTY_ERROR``
   The wait group is empty.
``PR_INVALID_STATE_ERROR``
   The attempted operation is on an object that was in an improper state
   to perform the request.
``PR_MAX_ERROR``
   Placeholder for the end of the list.
