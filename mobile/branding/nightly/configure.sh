MOZ_APP_DISPLAYNAME=Fennec
MOZ_APP_UA_EXTRA=Namoroka

case "$target" in
*-wince*)
    MOZ_APP_VERSION=1.0a4pre
    ;;
*)
    MOZ_APP_VERSION=1.1a1pre
    ;;
esac
