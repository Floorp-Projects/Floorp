-term = { $arg ->
   *[key] Value
}

key01 = { -term }
key02 = { -term () }
key03 = { -term(arg: 1) }
key04 = { -term("positional", narg1: 1, narg2: 2) }
