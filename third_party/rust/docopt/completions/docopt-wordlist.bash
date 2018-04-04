# This is your basic tab completion that will work well with commands that
# have only one usage (i.e., no distinct sub-commands).
#
# Completion works by simply taking the command name and running `$cmd --help`
# to get the usage (which is then parsed for possible completions).
function _docopt_wordlist {
  if [ -z "$DOCOPT_WORDLIST_BIN" ]; then
    DOCOPT_WORDLIST_BIN=/usr/local/bin/docopt-wordlist
  fi

  cword=$(_get_cword)
  cmd="${COMP_WORDS[0]}"
  wordlist=$("$cmd" --help 2>&1 | "$DOCOPT_WORDLIST_BIN")
  gen "$cword" "$wordlist"
}

# This is a fancier version of the above that supports commands that have
# multiple sub-commands (i.e., distinct usages like Cargo).
#
# This supports sub-command completion only if `$cmd --list` shows a list of
# available sub-commands.
#
# Otherwise, the usage for the command `a b c d` is taken from the first
# command that exits successfully:
#
#   a b c d --help
#   a b c --help
#   a b --help
#   a --help
#
# So for example, if you've typed `cargo test --jo`, then the following
# happens:
#
#   cargo test --jo --help  # error
#   cargo test --help       # gives 'test' sub-command usage!
#
# As a special case, if only the initial command has been typed, then the
# sub-commands (taken from `$cmd --list`) are added to the wordlist.
function _docopt_wordlist_commands {
  if [ -z "$DOCOPT_WORDLIST_BIN" ]; then
    DOCOPT_WORDLIST_BIN=/usr/local/bin/docopt-wordlist
  fi

  cword=$(_get_cword)
  if [ "$COMP_CWORD" = 1 ]; then
    cmd="${COMP_WORDS[0]}"
    wordlist=$("$cmd" --help 2>&1 | "$DOCOPT_WORDLIST_BIN")
    wordlist+=" $("$cmd" --list | egrep '^ +\w' | awk '{print $1}')"
    gen "$cword" "$wordlist"
  else
    for ((i="$COMP_CWORD"; i >= 1; i++)); do
      cmd="${COMP_WORDS[@]::$i}"
      wordlist=$($cmd --help 2>&1 | "$DOCOPT_WORDLIST_BIN")
      if [ $? = 0 ]; then
        gen "$cword" "$wordlist"
        break
      fi
    done
  fi
}

# A helper function for running `compgen`, which is responsible for taking
# a prefix and presenting possible completions.
#
# If the current prefix starts with a `.` or a `/`, then file/directory
# completion is done. Otherwise, Docopt completion is done. If Docopt
# completion is empty, then it falls back to file/directory completion.
function gen {
  cword="$1"
  wordlist="$2"
  if [[ "$cword" = .* || "$cword" = /* ]]; then
    COMPREPLY=($(compgen -A file -- "$cword"))
  else
    COMPREPLY=($(compgen -W "$wordlist" -- "$cword"))
    if [ -z "$COMPREPLY" ]; then
      COMPREPLY=($(compgen -A file -- "$cword"))
    fi
  fi
}
