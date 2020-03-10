new-messages =
    { BUILTIN() ->
        [0] Zero
       *[other] {""}Other
    }

valid-selector-term-attribute =
    { -term.case ->
       *[key] value
    }

# ERROR Term values are not valid selectors
invalid-selector-term-value =
    { -term ->
       *[key] value
    }

# ERROR CallExpressions on Terms are similar to TermReferences
invalid-selector-term-variant =
    { -term(case: "nominative") ->
       *[key] value
    }

# ERROR Nested expressions are not valid selectors
invalid-selector-nested-expression =
    { { 3 } ->
        *[key] default
    }

# ERROR Select expressions are not valid selectors
invalid-selector-select-expression =
    { { $sel ->
        *[key] value
        } ->
        *[key] default
    }

empty-variant =
    { $sel ->
       *[key] {""}
    }

reduced-whitespace =
    {FOO()->
       *[key] {""}
    }

nested-select =
    { $sel ->
       *[one] { $sel ->
          *[two] Value
       }
    }

# ERROR Missing selector
missing-selector =
    {
       *[key] Value
    }

# ERROR Missing line end after variant list
missing-line-end =
    { $sel ->
        *[key] Value}
