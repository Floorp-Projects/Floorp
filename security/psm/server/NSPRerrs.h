/* General NSPR 2.0 errors */
/* Caller must #include "prerror.h" */

ER2( PR_OUT_OF_MEMORY_ERROR, 	"Memory allocation attempt failed." )
ER2( PR_BAD_DESCRIPTOR_ERROR, 	"Invalid file descriptor." )
ER2( PR_WOULD_BLOCK_ERROR, 	"The operation would have blocked." )
ER2( PR_ACCESS_FAULT_ERROR, 	"Invalid memory address argument." )
ER2( PR_INVALID_METHOD_ERROR, 	"Invalid function for file type." )
ER2( PR_ILLEGAL_ACCESS_ERROR, 	"Invalid memory address argument." )
ER2( PR_UNKNOWN_ERROR, 		"Some unknown error has occurred." )
ER2( PR_PENDING_INTERRUPT_ERROR,"Operation interrupted by another thread." )
ER2( PR_NOT_IMPLEMENTED_ERROR, 	"function not implemented." )
ER2( PR_IO_ERROR, 		"I/O function error." )
ER2( PR_IO_TIMEOUT_ERROR, 	"I/O operation timed out." )
ER2( PR_IO_PENDING_ERROR, 	"I/O operation on busy file descriptor." )
ER2( PR_DIRECTORY_OPEN_ERROR, 	"The directory could not be opened." )
ER2( PR_INVALID_ARGUMENT_ERROR, "Invalid function argument." )
ER2( PR_ADDRESS_NOT_AVAILABLE_ERROR, "Network address not available (in use?)." )
ER2( PR_ADDRESS_NOT_SUPPORTED_ERROR, "Network address type not supported." )
ER2( PR_IS_CONNECTED_ERROR, 	"Already connected." )
ER2( PR_BAD_ADDRESS_ERROR, 	"Network address is invalid." )
ER2( PR_ADDRESS_IN_USE_ERROR, 	"Local Network address is in use." )
ER2( PR_CONNECT_REFUSED_ERROR, 	"Connection refused by peer." )
ER2( PR_NETWORK_UNREACHABLE_ERROR, "Network address is presently unreachable." )
ER2( PR_CONNECT_TIMEOUT_ERROR, 	"Connection attempt timed out." )
ER2( PR_NOT_CONNECTED_ERROR, 	"Network file descriptor is not connected." )
ER2( PR_LOAD_LIBRARY_ERROR, 	"Failure to load dynamic library." )
ER2( PR_UNLOAD_LIBRARY_ERROR, 	"Failure to unload dynamic library." )
ER2( PR_FIND_SYMBOL_ERROR, 	
"Symbol not found in any of the loaded dynamic libraries." )
ER2( PR_INSUFFICIENT_RESOURCES_ERROR, "Insufficient system resources." )
ER2( PR_DIRECTORY_LOOKUP_ERROR, 	
"A directory lookup on a network address has failed." )
ER2( PR_TPD_RANGE_ERROR, 		
"Attempt to access a TPD key that is out of range." )
ER2( PR_PROC_DESC_TABLE_FULL_ERROR, "Process open FD table is full." )
ER2( PR_SYS_DESC_TABLE_FULL_ERROR, "System open FD table is full." )
ER2( PR_NOT_SOCKET_ERROR, 	
"Network operation attempted on non-network file descriptor." )
ER2( PR_NOT_TCP_SOCKET_ERROR, 	
"TCP-specific function attempted on a non-TCP file descriptor." )
ER2( PR_SOCKET_ADDRESS_IS_BOUND_ERROR, "TCP file descriptor is already bound." )
ER2( PR_NO_ACCESS_RIGHTS_ERROR, "Access Denied." )
ER2( PR_OPERATION_NOT_SUPPORTED_ERROR, 
"The requested operation is not supported by the platform." )
ER2( PR_PROTOCOL_NOT_SUPPORTED_ERROR, 
"The host operating system does not support the protocol requested." )
ER2( PR_REMOTE_FILE_ERROR, 	"Access to the remote file has been severed." )
ER2( PR_BUFFER_OVERFLOW_ERROR, 	
"The value requested is too large to be stored in the data buffer provided." )
ER2( PR_CONNECT_RESET_ERROR, 	"TCP connection reset by peer." )
ER2( PR_RANGE_ERROR, 		"Unused." )
ER2( PR_DEADLOCK_ERROR, 	"The operation would have deadlocked." )
ER2( PR_FILE_IS_LOCKED_ERROR, 	"The file is already locked." )
ER2( PR_FILE_TOO_BIG_ERROR, 	
"Write would result in file larger than the system allows." )
ER2( PR_NO_DEVICE_SPACE_ERROR, 	"The device for storing the file is full." )
ER2( PR_PIPE_ERROR, 		"Unused." )
ER2( PR_NO_SEEK_DEVICE_ERROR, 	"Unused." )
ER2( PR_IS_DIRECTORY_ERROR, 	
"Cannot perform a normal file operation on a directory." )
ER2( PR_LOOP_ERROR, 		"Symbolic link loop." )
ER2( PR_NAME_TOO_LONG_ERROR, 	"File name is too long." )
ER2( PR_FILE_NOT_FOUND_ERROR, 	"File not found." )
ER2( PR_NOT_DIRECTORY_ERROR, 	
"Cannot perform directory operation on a normal file." )
ER2( PR_READ_ONLY_FILESYSTEM_ERROR, 
"Cannot write to a read-only file system." )
ER2( PR_DIRECTORY_NOT_EMPTY_ERROR, 
"Cannot delete a directory that is not empty." )
ER2( PR_FILESYSTEM_MOUNTED_ERROR, 
"Cannot delete or rename a file object while the file system is busy." )
ER2( PR_NOT_SAME_DEVICE_ERROR, 	
"Cannot rename a file to a file system on another device." )
ER2( PR_DIRECTORY_CORRUPTED_ERROR, 
"The directory object in the file system is corrupted." )
ER2( PR_FILE_EXISTS_ERROR, 	
"Cannot create or rename a filename that already exists." )
ER2( PR_MAX_DIRECTORY_ENTRIES_ERROR, 
"Directory is full.  No additional filenames may be added." )
ER2( PR_INVALID_DEVICE_STATE_ERROR, 
"The required device was in an invalid state." )
ER2( PR_DEVICE_IS_LOCKED_ERROR, "The device is locked." )
ER2( PR_NO_MORE_FILES_ERROR, 	"No more entries in the directory." )
ER2( PR_END_OF_FILE_ERROR, 	"Encountered end of file." )
ER2( PR_FILE_SEEK_ERROR, 	"Seek error." )
ER2( PR_FILE_IS_BUSY_ERROR, 	"The file is busy." )
ER2( PR_IN_PROGRESS_ERROR,
"Operation is still in progress (probably a non-blocking connect)." )
ER2( PR_ALREADY_INITIATED_ERROR,
"Operation has already been initiated (probably a non-blocking connect)." )

#ifdef PR_GROUP_EMPTY_ERROR
ER2( PR_GROUP_EMPTY_ERROR, 	"The wait group is empty." )
#endif

#ifdef PR_INVALID_STATE_ERROR
ER2( PR_INVALID_STATE_ERROR, 	"Object state improper for request." )
#endif

ER2( PR_MAX_ERROR, 		"Placeholder for the end of the list" )
