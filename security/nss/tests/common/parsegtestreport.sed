/\<testcase/{
  s/^.* name="\([^"]*\)" value_param="\([^"]*\)" status="\([^"]*\)" time="[^"]*" classname="\([^"]*\).*$/\3 '\4: \1 \2'/
  t end
  s/^.* name="\([^"]*\)" status="\([^"]*\)" time="[^"]*" classname="\([^"]*\)".*$/\2 '\3: \1'/
  t end
  s/^.* name="\([^"]*\)" value_param="\([^"]*\)" status="\([^"]*\)" result="[^"]*" time="[^"]*" timestamp="[^"]*" classname="\([^"]*\)".*$/\3 '\4: \1 \2'/
  t end
  s/^.* name="\([^"]*\)" status="\([^"]*\)" result="[^"]*" time="[^"]*" timestamp="[^"]*" classname="\([^"]*\)".*$/\2 '\3: \1'/
  t end
}
d
: end
s/&quot;/"/g
