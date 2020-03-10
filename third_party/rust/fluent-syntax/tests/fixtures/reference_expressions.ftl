## Reference expressions in placeables.

message-reference-placeable = {msg}
term-reference-placeable = {-term}
variable-reference-placeable = {$var}

# Function references are invalid outside of call expressions.
# This parses as a valid MessageReference.
function-reference-placeable = {FUN}


## Reference expressions in selectors.

variable-reference-selector = {$var ->
   *[key] Value
}

# ERROR Message values may not be used as selectors.
message-reference-selector = {msg ->
   *[key] Value
}
# ERROR Term values may not be used as selectors.
term-reference-selector = {-term ->
   *[key] Value
}
# ERROR Function references are invalid outside of call expressions, and this
# parses as a MessageReference which isn't a valid selector.
function-expression-selector = {FUN ->
   *[key] Value
}
