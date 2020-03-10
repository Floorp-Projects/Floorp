simple-identifier =
    { $sel ->
       *[key] value
    }

identifier-surrounded-by-whitespace =
    { $sel ->
       *[     key     ] value
    }

int-number =
    { $sel ->
       *[1] value
    }

float-number =
    { $sel ->
       *[3.14] value
    }

# ERROR
invalid-identifier =
    { $sel ->
       *[two words] value
    }

# ERROR
invalid-int =
    { $sel ->
       *[1 apple] value
    }

# ERROR
invalid-int =
    { $sel ->
       *[3.14 apples] value
    }
