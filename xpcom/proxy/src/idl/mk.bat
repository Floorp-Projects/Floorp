xpidl -w -I ../../../../js/src/xpconnect/idl/ -m header nsIProxyCreateInstance.idl
xpidl -w -I ../../../../js/src/xpconnect/idl/ -m typelib nsIProxyCreateInstance.idl

cp nsIProxyCreateInstance.xpt ../../../../dist/win32_d.obj/bin/components
cp nsIProxyCreateInstance.h ../

