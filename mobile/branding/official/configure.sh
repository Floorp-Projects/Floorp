MOZ_APP_DISPLAYNAME=Firefox
MOZ_APP_UA_EXTRA=Firefox

case "$target" in
*-wince*)
    MOZ_APP_VERSION=1.0a4pre
    ;;
*)
    MOZ_APP_VERSION=1.1a1
    ;;
esac
