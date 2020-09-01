missing-arg-error = Błąd: Proszę wprowadzić liczbę jako argument.
input-parse-error = Błąd: Nie udało się sparsować `{ $input }`. Powód: { $reason } 
response-msg =
    { $value ->
        [one] "{ $input }" ma jeden krok Collatza.
        [few] "{ $input }" ma { $value } kroki Collatza.
       *[many] "{ $input }" ma { $value } kroków Collatza.
    }
