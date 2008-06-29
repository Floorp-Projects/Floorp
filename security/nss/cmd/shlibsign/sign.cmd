/* Equivalent to sign.sh for OS/2 */
PARSE ARG dist objdir os_target nspr_lib_dir therest
dist=forwardtoback(dist);
objdir=forwardtoback(objdir);
nspr_lib_dir=forwardtoback(nspr_lib_dir);
'echo 'dist
'echo 'objdir
'echo 'nspr_lib_dir
'set BEGINLIBPATH='dist'\lib;'nspr_lib_dir';'dist'\bin;%BEGINLIBPATH%'
'set LIBPATHSTRICT=T'
objdir'\shlibsign -v -i 'therest
exit

forwardtoback: procedure
  arg pathname
  parse var pathname pathname'/'rest
  do while (rest <> "")
    pathname = pathname'\'rest
    parse var pathname pathname'/'rest
  end
  return pathname
