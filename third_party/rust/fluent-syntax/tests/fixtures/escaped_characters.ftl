## Literal text
text-backslash-one = Value with \ a backslash
text-backslash-two = Value with \\ two backslashes
text-backslash-brace = Value with \{placeable}
text-backslash-u = \u0041
text-backslash-backslash-u = \\u0041

## String literals
quote-in-string = {"\""}
backslash-in-string = {"\\"}
# ERROR Mismatched quote
mismatched-quote = {"\\""}
# ERROR Unknown escape
unknown-escape = {"\x"}
# ERROR Multiline literal
invalid-multiline-literal = {"
 "}

## Unicode escapes
string-unicode-4digits = {"\u0041"}
escape-unicode-4digits = {"\\u0041"}
string-unicode-6digits = {"\U01F602"}
escape-unicode-6digits = {"\\U01F602"}

# OK The trailing "00" is part of the literal value.
string-too-many-4digits = {"\u004100"}
# OK The trailing "00" is part of the literal value.
string-too-many-6digits = {"\U01F60200"}

# ERROR Too few hex digits after \u.
string-too-few-4digits = {"\u41"}
# ERROR Too few hex digits after \U.
string-too-few-6digits = {"\U1F602"}

## Literal braces
brace-open = An opening {"{"} brace.
brace-close = A closing {"}"} brace.
