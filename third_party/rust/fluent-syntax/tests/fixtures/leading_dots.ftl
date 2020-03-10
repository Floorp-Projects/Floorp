key01 = .Value
key02 = …Value
key03 = {"."}Value
key04 =
    {"."}Value

key05 = Value
    {"."}Continued

key06 = .Value
    {"."}Continued

# MESSAGE (value = "Value", attributes = [])
# JUNK (attr .Continued" must have a value)
key07 = Value
    .Continued

# JUNK (attr .Value must have a value)
key08 =
    .Value

# JUNK (attr .Value must have a value)
key09 =
    .Value
    Continued

key10 =
    .Value = which is an attribute
    Continued

key11 =
    {"."}Value = which looks like an attribute
    Continued

key12 =
    .accesskey =
    A

key13 =
    .attribute = .Value

key14 =
    .attribute =
         {"."}Value

key15 =
    { 1 ->
        [one] .Value
       *[other]
            {"."}Value
    }

# JUNK (variant must have a value)
key16 =
    { 1 ->
       *[one]
           .Value
    }

# JUNK (unclosed placeable)
key17 =
    { 1 ->
       *[one] Value
           .Continued
    }

# JUNK (attr .Value must have a value)
key18 =
.Value

key19 =
.attribute = Value
    Continued

key20 =
{"."}Value
