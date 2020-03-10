missing-arg-error = Erreur : veuillez saisir un nombre en paramètre.
input-parse-error = Erreur : impossible d'interpréter le paramètre `{ $input }`. Raison : { $reason }
response-msg =
    { $value ->
        [one] La suite de Syracuse du nombre "{ $input }" comporte une valeur.
       *[other] La suite de Syracuse du nombre "{ $input }" comporte { $value } valeurs.
    }
