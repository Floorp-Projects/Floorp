### The syntax in this file has been discontinued. It is no longer part of the
### Fluent specification and should not be implemented nor used. We're keeping
### these fixtures around to protect against accidental syntax reuse.


## Variant lists.

message-variant-list =
    {
       *[key] Value
    }

-term-variant-list =
    {
       *[key] Value
    }


## Variant expressions.

message-variant-expression-placeable = {msg[case]}
message-variant-expression-selector = {msg[case] ->
   *[key] Value
}

term-variant-expression-placeable = {-term[case]}
term-variant-expression-selector = {-term[case] ->
   *[key] Value
}
