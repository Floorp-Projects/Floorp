xpidl -w -I ../../../../js/src/xpconnect/idl/ -m header nsITestProxyFoo.idl
xpidl -w -I ../../../../js/src/xpconnect/idl/ -m typelib nsITestProxyFoo.idl

cp nsITestProxyFoo.xpt ../../../../dist/win32_d.obj/bin/components

