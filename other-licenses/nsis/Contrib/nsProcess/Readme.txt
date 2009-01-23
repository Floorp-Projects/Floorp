*****************************************************************
***                nsProcess NSIS plugin v1.5                 ***
*****************************************************************

2006 Shengalts Aleksander aka Instructor (Shengalts@mail.ru)

Source function FIND_PROC_BY_NAME based
   upon the Ravi Kochhar (kochhar@physiology.wisc.edu) code
Thanks iceman_k (FindProcDLL plugin) and
   DITMan (KillProcDLL plugin) for direct me


Features:
- Find a process by name
- Kill a process by name
- Kill all processes with specified name (not only one)
- The process name is case-insensitive
- Win95/98/ME/NT/2000/XP support
- Small plugin size (4 Kb)


**** Find process ****
${nsProcess::FindProcess} "[file.exe]" $var

"[file.exe]"  - Process name (e.g. "notepad.exe")

$var     0    Success
         603  Process was not currently running
         604  Unable to identify system type
         605  Unsupported OS
         606  Unable to load NTDLL.DLL
         607  Unable to get procedure address from NTDLL.DLL
         608  NtQuerySystemInformation failed
         609  Unable to load KERNEL32.DLL
         610  Unable to get procedure address from KERNEL32.DLL
         611  CreateToolhelp32Snapshot failed


**** Kill process ****
${nsProcess::KillProcess} "[file.exe]" $var

"[file.exe]"  - Process name (e.g. "notepad.exe")

$var     0    Success
         601  No permission to terminate process
         602  Not all processes terminated successfully
         603  Process was not currently running
         604  Unable to identify system type
         605  Unsupported OS
         606  Unable to load NTDLL.DLL
         607  Unable to get procedure address from NTDLL.DLL
         608  NtQuerySystemInformation failed
         609  Unable to load KERNEL32.DLL
         610  Unable to get procedure address from KERNEL32.DLL
         611  CreateToolhelp32Snapshot failed


**** Unload plugin ****
${nsProcess::Unload}
