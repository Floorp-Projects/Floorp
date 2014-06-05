/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

onmessage = function(e) {
    // send the OS.Constants as a stringified JSON
    postMessage(JSON.stringify({'OS_Constants': OS.Constants, 'OS_Constants_Win': OS.Constants.Win,
        'OS_Constants_Path': OS.Constants.Path, 'OS_Constants_Sys': OS.Constants.Sys,
        'OS_Constants_libc': OS.Constants.libc, 'OS_Constants_Sys_DEBUG': OS.Constants.Sys.DEBUG,
        'OS_Constants_Sys_Name': OS.Constants.Sys.Name}));
};