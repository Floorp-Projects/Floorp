# This file must be used with "source bin/activate.csh" *from csh*.
# You cannot run it directly.
# Created by Davide Di Blasi <davidedb@gmail.com>.

alias deactivate 'test $?_OLD_VIRTUAL_PATH != 0 && setenv PATH "$_OLD_VIRTUAL_PATH" && unset _OLD_VIRTUAL_PATH; rehash; test $?_OLD_VIRTUAL_PROMPT != 0 && set prompt="$_OLD_VIRTUAL_PROMPT" && unset _OLD_VIRTUAL_PROMPT; unsetenv VIRTUAL_ENV; test "\!:*" != "nondestructive" && unalias deactivate'

# Unset irrelavent variables.
deactivate nondestructive

setenv VIRTUAL_ENV "__VIRTUAL_ENV__"

set _OLD_VIRTUAL_PATH="$PATH"
setenv PATH "$VIRTUAL_ENV/__BIN_NAME__:$PATH"

set _OLD_VIRTUAL_PROMPT="$prompt"

if ("__VIRTUAL_PROMPT__" != "") then
    set env_name = "__VIRTUAL_PROMPT__"
else
    if (`basename "$VIRTUAL_ENV"` == "__") then
        # special case for Aspen magic directories
        # see http://www.zetadev.com/software/aspen/
        set env_name = `basename \`dirname "$VIRTUAL_ENV"\``
    else
        set env_name = `basename "$VIRTUAL_ENV"`
    endif
endif
set prompt = "[$env_name] $prompt"
unset env_name

rehash

