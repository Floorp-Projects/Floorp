function _mach()
{
  local cur targets
  COMPREPLY=()

  # Calling `mach-completion` with -h/--help would result in the
  # help text being used as the completion targets.
  if [[ $COMP_LINE == *"-h"* || $COMP_LINE == *"--help"* ]]; then
    return 0
  fi

  # Load the list of targets
  targets=`"${COMP_WORDS[0]}" mach-completion ${COMP_LINE}`
  cur="${COMP_WORDS[COMP_CWORD]}"
  COMPREPLY=( $(compgen -W "$targets" -- ${cur}) )
  return 0
}
complete -o default -F _mach mach
