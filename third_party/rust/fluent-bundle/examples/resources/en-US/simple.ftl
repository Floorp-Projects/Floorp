missing-arg-error = Error: Please provide a number as argument.
input-parse-error = Error: Could not parse input `{ $input }`. Reason: { $reason }
response-msg =
    { $value ->
        [one] "{ $input }" has one Collatz step.
       *[other] "{ $input }" has { $value } Collatz steps.
    }
