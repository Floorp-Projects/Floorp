.. _Introduction_to_NSPR:

`Introduction to NSPR </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR>`__
--------------------------------------------------------------------------------

-  `NSPR Naming
   Conventions </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#NSPR_Naming_Conventions>`__
-  `NSPR
   Threads </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#NSPR_Threads>`__

   -  `Thread
      Scheduling </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#Thread_Scheduling>`__

      -  `Setting Thread
         Priorities </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#Setting_Thread_Priorities>`__
      -  `Preempting
         Threads </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#Preempting_Threads>`__
      -  `Interrupting
         Threads </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#Interrupting_Threads>`__

-  `NSPR Thread
   Synchronization </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#NSPR_Thread_Synchronization>`__

   -  `Locks and
      Monitors </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#Locks_and_Monitors>`__
   -  `Condition
      Variables </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#Condition_Variables>`__

-  `NSPR Sample
   Code </Mozilla/Projects/NSPR/Reference/Introduction_to_NSPR#NSPR_Sample_Code>`__

.. _NSPR_Types:

`NSPR Types </Mozilla/Projects/NSPR/Reference/NSPR_Types>`__
------------------------------------------------------------

-  `Calling Convention
   Types </Mozilla/Projects/NSPR/Reference/NSPR_Types#Calling_Convention_Types>`__
-  `Algebraic
   Types </Mozilla/Projects/NSPR/Reference/NSPR_Types#Algebraic_Types>`__

   -  `8-, 16-, and 32-bit Integer
      Types </Mozilla/Projects/NSPR/Reference/NSPR_Types#8-.2C_16-.2C_and_32-bit_Integer_Types>`__

      -  `Signed
         Integers </Mozilla/Projects/NSPR/Reference/NSPR_Types#Signed_Integers>`__
      -  `Unsigned
         Integers </Mozilla/Projects/NSPR/Reference/NSPR_Types#Unsigned_Integers>`__

   -  `64-bit Integer
      Types </Mozilla/Projects/NSPR/Reference/NSPR_Types#64-bit_Integer_Types>`__
   -  `Floating-Point Integer
      Type </Mozilla/Projects/NSPR/Reference/NSPR_Types#Floating-Point_Number_Type>`__
   -  `Native OS Integer
      Types </Mozilla/Projects/NSPR/Reference/NSPR_Types#Native_OS_Integer_Types>`__

-  `Miscellaneous
   Types </Mozilla/Projects/NSPR/Reference/NSPR_Types#Miscellaneous_Types>`__

   -  `Size
      Type </Mozilla/Projects/NSPR/Reference/NSPR_Types#Size_Type>`__
   -  `Pointer Difference
      Types </Mozilla/Projects/NSPR/Reference/NSPR_Types#Pointer_Difference_Types>`__
   -  `Boolean
      Types </Mozilla/Projects/NSPR/Reference/NSPR_Types#Boolean_Types>`__
   -  `Status Type for Return
      Values </Mozilla/Projects/NSPR/Reference/NSPR_Types#Status_Type_for_Return_Values>`__

.. _Threads:

`Threads </Mozilla/Projects/NSPR/Reference/Threads>`__
------------------------------------------------------

-  `Threading Types and
   Constants </Mozilla/Projects/NSPR/Reference/Threads#Threading_Types_and_Constants>`__
-  `Threading
   Functions </Mozilla/Projects/NSPR/Reference/Threads#Threading_Functions>`__

   -  `Creating, Joining, and Identifying
      Threads </Mozilla/Projects/NSPR/Reference/Threads#Creating.2C_Joining.2C_and_Identifying_Threads>`__
   -  `Controlling Thread
      Priorities </Mozilla/Projects/NSPR/Reference/Threads#Controlling_Thread_Priorities>`__
   -  `Controlling Per-Thread Private
      Data </Mozilla/Projects/NSPR/Reference/Threads#Controlling_Per-Thread_Private_Data>`__
   -  `Interrupting and
      Yielding </Mozilla/Projects/NSPR/Reference/Threads#Interrupting_and_Yielding>`__
   -  `Setting Global Thread
      Concurrency </Mozilla/Projects/NSPR/Reference/Threads#Setting_Global_Thread_Concurrency>`__
   -  `Getting a Thread's
      Scope </Mozilla/Projects/NSPR/Reference/Threads#Getting_a_Thread.27s_Scope>`__

.. _Process_Initialization:

`Process Initialization </Mozilla/Projects/NSPR/Reference/Process_Initialization>`__
------------------------------------------------------------------------------------

-  `Identity and
   Versioning </Mozilla/Projects/NSPR/Reference/Process_Initialization#Identity_and_Versioning>`__

   -  `Name and Version
      Constants </Mozilla/Projects/NSPR/Reference/Process_Initialization#Name_and_Version_Constants>`__

-  `Initialization and
   Cleanup </Mozilla/Projects/NSPR/Reference/Process_Initialization#Initialization_and_Cleanup>`__
-  `Module
   Initialization </Mozilla/Projects/NSPR/Reference/Process_Initialization#Module_Initialization>`__

.. _Locks:

`Locks </Mozilla/Projects/NSPR/Reference/Locks>`__
--------------------------------------------------

-  `Lock Type </Mozilla/Projects/NSPR/Reference/Locks#Lock_Type>`__
-  `Lock
   Functions </Mozilla/Projects/NSPR/Reference/Locks#Lock_Functions>`__

.. _Condition_Variables:

`Condition Variables </Mozilla/Projects/NSPR/Reference/Condition_Variables>`__
------------------------------------------------------------------------------

-  `Condition Variable
   Type </Mozilla/Projects/NSPR/Reference/Condition_Variables#Condition_Variable_Type>`__
-  `Condition Variable
   Functions </Mozilla/Projects/NSPR/Reference/Condition_Variables#Condition_Variable_Functions>`__

.. _Monitors:

`Monitors </Mozilla/Projects/NSPR/Reference/Monitors>`__
--------------------------------------------------------

-  `Monitor
   Type </Mozilla/Projects/NSPR/Reference/Monitors#Monitor_Type>`__
-  `Monitor
   Functions </Mozilla/Projects/NSPR/Reference/Monitors#Monitor_Functions>`__

.. _Cached_Monitors:

`Cached Monitors </Mozilla/Projects/NSPR/Reference/Cached_Monitors>`__
----------------------------------------------------------------------

-  `Cached Monitor
   Functions </Mozilla/Projects/NSPR/Reference/Cached_Monitors#Cached_Monitor_Functions>`__

.. _I.2FO_Types:

`I/O Types </Mozilla/Projects/NSPR/Reference/I_O_Types>`__
----------------------------------------------------------

-  `Directory
   Type </Mozilla/Projects/NSPR/Reference/I_O_Types#Directory_Type>`__
-  `File Descriptor
   Types </Mozilla/Projects/NSPR/Reference/I_O_Types#File_Descriptor_Types>`__
-  `File Info
   Types </Mozilla/Projects/NSPR/Reference/I_O_Types#File_Info_Types>`__
-  `Network Address
   Types </Mozilla/Projects/NSPR/Reference/I_O_Types#Network_Address_Types>`__
-  `Types Used with Socket Options
   Functions </Mozilla/Projects/NSPR/Reference/I_O_Types#Types_Used_with_Socket_Options_Functions>`__
-  `Type Used with Memory-Mapped
   I/O </Mozilla/Projects/NSPR/Reference/I_O_Types#Type_Used_with_Memory-Mapped_I.2FO>`__
-  `Offset Interpretation for Seek
   Functions </Mozilla/Projects/NSPR/Reference/I_O_Types#Offset_Interpretation_for_Seek_Functions>`__

.. _I.2FO_Functions:

`I/O Functions </Mozilla/Projects/NSPR/Reference/I_O_Functions>`__
------------------------------------------------------------------

-  `Functions that Operate on
   Pathnames </Mozilla/Projects/NSPR/Reference/I_O_Functions#Functions_that_Operate_on_Pathnames>`__
-  `Functions that Act on File
   Descriptors </Mozilla/Projects/NSPR/Reference/I_O_Functions#Functions_that_Act_on_File_Descriptors>`__
-  `Directory I/O
   Functions </Mozilla/Projects/NSPR/Reference/I_O_Functions#Directory_I.2FO_Functions>`__
-  `Socket Manipulation
   Functions </Mozilla/Projects/NSPR/Reference/I_O_Functions#Socket_Manipulation_Functions>`__
-  `Converting Between Host and Network
   Addresses </Mozilla/Projects/NSPR/Reference/I_O_Functions#Converting_Between_Host_and_Network_Addresses>`__
-  `Memory-Mapped I/O
   Functions </Mozilla/Projects/NSPR/Reference/I_O_Functions#Memory-Mapped_I.2FO_Functions>`__
-  `Anonymous Pipe
   Function </Mozilla/Projects/NSPR/Reference/I_O_Functions#Anonymous_Pipe_Function>`__
-  `Polling
   Functions </Mozilla/Projects/NSPR/Reference/I_O_Functions#Polling_Functions>`__
-  `Pollable
   Events </Mozilla/Projects/NSPR/Reference/I_O_Functions#Pollable_Events>`__
-  `Manipulating
   Layers </Mozilla/Projects/NSPR/Reference/I_O_Functions#Manipulating_Layers>`__

.. _Network_Addresses:

`Network Addresses </Mozilla/Projects/NSPR/Reference/Network_Addresses>`__
--------------------------------------------------------------------------

-  `Network Address Types and
   Constants </Mozilla/Projects/NSPR/Reference/Network_Addresses#Network_Address_Types_and_Constants>`__
-  `Network Address
   Functions </Mozilla/Projects/NSPR/Reference/Network_Addresses#Network_Address_Functions>`__

.. _Atomic_Operations:

`Atomic Operations </Mozilla/Projects/NSPR/Reference/Atomic_Operations>`__
--------------------------------------------------------------------------

-  ``PR_AtomicIncrement``
-  ``PR_AtomicDecrement``
-  ``PR_AtomicSet``

.. _Interval_Timing:

`Interval Timing </Mozilla/Projects/NSPR/Reference/Interval_Timing>`__
----------------------------------------------------------------------

-  `Interval Time Type and
   Constants </Mozilla/Projects/NSPR/Reference/Interval_Timing#Interval_Time_Type_and_Constants>`__
-  `Interval
   Functions </Mozilla/Projects/NSPR/Reference/Interval_Timing#Interval_Functions>`__

.. _Date_and_Time:

`Date and Time </Mozilla/Projects/NSPR/Reference/Date_and_Time>`__
------------------------------------------------------------------

-  `Types and
   Constants </Mozilla/Projects/NSPR/Reference/Date_and_Time#Types_and_Constants>`__
-  `Time Parameter Callback
   Functions </Mozilla/Projects/NSPR/Reference/Date_and_Time#Time_Parameter_Callback_Functions>`__
-  `Functions </Mozilla/Projects/NSPR/Reference/Date_and_Time#Functions>`__

.. _Memory_Management_Operations:

`Memory Management Operations </Mozilla/Projects/NSPR/Reference/Memory_Management_Operations>`__
------------------------------------------------------------------------------------------------

-  `Memory Allocation
   Functions </Mozilla/Projects/NSPR/Reference/Memory_Management_Operations#Memory_Allocation_Functions>`__
-  `Memory Allocation
   Macros </Mozilla/Projects/NSPR/Reference/Memory_Management_Operations#Memory_Allocation_Macros>`__

.. _String_Operations:

`String Operations </Mozilla/Projects/NSPR/Reference/String_Operations>`__
--------------------------------------------------------------------------

-  ``PL_strlen``
-  ``PL_strcpy``
-  ``PL_strdup``
-  ``PL_strfree``

.. _Floating_Point_Number_to_String_Conversion:

`Floating Point Number to String Conversion </Mozilla/Projects/NSPR/Reference/Floating_Point_Number_to_String_Conversion>`__
----------------------------------------------------------------------------------------------------------------------------

-  ``PR_strtod``
-  ``PR_dtoa``
-  ``PR_cnvtf``

.. _Long_Long_.2864-bit.29_Integers:

Long Long (64-bit) Integers
---------------------------

.. _BitMaps:

BitMaps
-------

.. _Formatted_Printing:

Formatted Printing
------------------

.. _Linked_Lists:

`Linked Lists </Mozilla/Projects/NSPR/Reference/Linked_Lists>`__
----------------------------------------------------------------

-  `Linked List
   Types </Mozilla/Projects/NSPR/Reference/Linked_Lists#Linked_List_Types>`__

   -  ``PRCList``

-  `Linked List
   Macros </Mozilla/Projects/NSPR/Reference/Linked_Lists#Linked_List_Macros>`__

   -  ``PR_INIT_CLIST``
   -  ``PR_INIT_STATIC_CLIST``
   -  ``PR_APPEND_LINK``
   -  ``PR_INSERT_LINK``
   -  ``PR_NEXT_LINK``
   -  ``PR_PREV_LINK``
   -  ``PR_REMOVE_LINK``
   -  ``PR_REMOVE_AND_INIT_LINK``
   -  ``PR_INSERT_BEFORE``
   -  ``PR_INSERT_AFTER``

.. _Dynamic_Library_Linking:

`Dynamic Library Linking </Mozilla/Projects/NSPR/Reference/Dynamic_Library_Linking>`__
--------------------------------------------------------------------------------------

-  `Library Linking
   Types </Mozilla/Projects/NSPR/Reference/Dynamic_Library_Linking#Library_Linking_Types>`__

   -  ``PRLibrary``
   -  ``PRStaticLinkTable``

-  `Library Linking
   Functions </Mozilla/Projects/NSPR/Reference/Dynamic_Library_Linking#Library_Linking_Functions>`__

   -  ``PR_SetLibraryPath``
   -  ``PR_GetLibraryPath``
   -  ``PR_GetLibraryName``
   -  ``PR_FreeLibraryName``
   -  ``PR_LoadLibrary``
   -  ``PR_UnloadLibrary``
   -  ``PR_FindSymbol``
   -  ``PR_FindSymbolAndLibrary``
   -  `Finding Symbols Defined in the Main Executable
      Program </Mozilla/Projects/NSPR/Reference/Dynamic_Library_Linking#Finding_Symbols_Defined_in_the_Main_Executable_Program>`__

-  `Platform
   Notes </Mozilla/Projects/NSPR/Reference/Dynamic_Library_Linking#Platform_Notes>`__

   -  `Dynamic Library Search
      Path </Mozilla/Projects/NSPR/Reference/Dynamic_Library_Linking#Dynamic_Library_Search_Path>`__
   -  `Exporting Symbols from the Main Executable
      Program </Mozilla/Projects/NSPR/Reference/Dynamic_Library_Linking#Exporting_Symbols_from_the_Main_Executable_Program>`__

.. _Process_Management_and_Interprocess_Communication:

`Process Management and Interprocess Communication </En/NSPR_API_Reference/Process_Management_and_Interprocess_Communication>`__
--------------------------------------------------------------------------------------------------------------------------------

-  `Process Management Types and
   Constants </En/NSPR_API_Reference/Process_Management_and_Interprocess_Communication#Process_Management_Types_and_Constants>`__

   -  ``PRProcess``
   -  ``PRProcessAttr``

-  `Process Management
   Functions </En/NSPR_API_Reference/Process_Management_and_Interprocess_Communication#Process_Management_Functions>`__

   -  `Setting the Attributes of a New
      Process </En/NSPR_API_Reference/Process_Management_and_Interprocess_Communication#Setting_the_Attributes_of_a_New_Process>`__
   -  `Creating and Managing
      Processes </En/NSPR_API_Reference/Process_Management_and_Interprocess_Communication#Creating_and_Managing_Processes>`__

.. _Multiwait_Receive:

Multiwait Receive
-----------------

.. _System_Information_and_Environment_Variables:

System Information and Environment Variables
--------------------------------------------

.. _Logging:

`Logging </NSPR_API_Reference/Logging>`__
-----------------------------------------

-  `Conditional Compilation and
   Execution </NSPR_API_Reference/Logging#Conditional_Compilation_and_Execution>`__
-  `Log Types and
   Variables </NSPR_API_Reference/Logging#Log_Types_and_Variables>`__

   -  ``PRLogModuleInfo``
   -  ``PRLogModuleLevel``
   -  ``NSPR_LOG_MODULES``
   -  ``NSPR_LOG_FILE``

-  `Logging Functions and
   Macros </NSPR_API_Reference/Logging#Logging_Functions_and_Macros>`__

   -  ``PR_NewLogModule``
   -  ``PR_SetLogFile``
   -  ``PR_SetLogBuffering``
   -  ``PR_LogPrint``
   -  ``PR_LogFlush``
   -  ``PR_LOG_TEST``
   -  ``PR_LOG``
   -  ``PR_Assert``
   -  `PR_ASSERT </en-US/docs/Mozilla/Projects/NSPR/Reference/PR_ASSERT>`__
   -  ``PR_NOT_REACHED``

-  `Use Example </NSPR_API_Reference/Logging#Use_Example>`__

.. _Instrumentation_Counters:

Instrumentation Counters
------------------------

.. _Named_Shared_Memory:

`Named Shared Memory </Mozilla/Projects/NSPR/Reference/Named_Shared_Memory>`__
------------------------------------------------------------------------------

-  `Shared Memory
   Protocol </Mozilla/Projects/NSPR/Reference/Named_Shared_Memory#Shared_Memory_Protocol>`__
-  `Named Shared Memory
   Functions </Mozilla/Projects/NSPR/Reference/Named_Shared_Memory#Named_Shared_Memory_Functions>`__

.. _Anonymous_Shared_Memory:

`Anonymous Shared Memory </Mozilla/Projects/NSPR/Reference/Anonymous_Shared_Memory>`__
--------------------------------------------------------------------------------------

-  `Anonymous Memory
   Protocol </Mozilla/Projects/NSPR/Reference/Anonymous_Shared_Memory#Anonymous_Memory_Protocol>`__
-  `Anonymous Shared Memory
   Functions </Mozilla/Projects/NSPR/Reference/Anonymous_Shared_Memory#Anonymous_Shared_Memory_Functions>`__

.. _IPC_Semaphores:

`IPC Semaphores </Mozilla/Projects/NSPR/Reference/IPC_Semaphores>`__
--------------------------------------------------------------------

-  `IPC Semaphore
   Functions </Mozilla/Projects/NSPR/Reference/IPC_Semaphores#IPC_Semaphore_Functions>`__

.. _Thread_Pools:

`Thread Pools </Mozilla/Projects/NSPR/Reference/Thread_Pools>`__
----------------------------------------------------------------

-  `Thread Pool
   Types </Mozilla/Projects/NSPR/Reference/Thread_Pools#Thread_Pool_Types>`__
-  `Thread Pool
   Functions </Mozilla/Projects/NSPR/Reference/Thread_Pools#Thread_Pool_Functions>`__

.. _Random_Number_Generator:

`Random Number Generator </Mozilla/Projects/NSPR/Reference/Random_Number_Generator>`__
--------------------------------------------------------------------------------------

-  `Random Number Generator
   Function </Mozilla/Projects/NSPR/Reference/Random_Number_Generator#Random_Number_Generator_Function>`__

.. _Hash_Tables:

`Hash Tables </Mozilla/Projects/NSPR/Reference/Hash_Tables>`__
--------------------------------------------------------------

-  `Hash Tables and Type
   Constants </Mozilla/Projects/NSPR/Reference/Hash_Tables#Hash_Tables_and_Type_Constants>`__
-  `Hash Table
   Functions </Mozilla/Projects/NSPR/Reference/Hash_Tables#Hash_Table_Functions>`__

.. _NSPR_Error_Handling:

`NSPR Error Handling </Mozilla/Projects/NSPR/Reference/NSPR_Error_Handling>`__
------------------------------------------------------------------------------

-  `Error
   Type </Mozilla/Projects/NSPR/Reference/NSPR_Error_Handling#Error_Type>`__
-  `Error
   Functions </Mozilla/Projects/NSPR/Reference/NSPR_Error_Handling#Error_Functions>`__
-  `Error
   Codes </Mozilla/Projects/NSPR/Reference/NSPR_Error_Handling#Error_Codes>`__
