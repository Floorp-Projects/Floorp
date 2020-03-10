key01 = {$var}
key02 = {   $var   }
key03 = {
    $var
}
key04 = {
$var}


## Errors

# ERROR Missing variable identifier
err01 = {$}
# ERROR Double $$
err02 = {$$var}
# ERROR Invalid first char of the identifier
err03 = {$-var}
