# [Android Components](../../../README.md) > Libraries > JEXL

Javascript Expression Language: Powerful context-based expression parser and evaluator.

This implementation is based on [Mozjexl](https://github.com/mozilla/mozjexl), a fork of Jexl (designed and created at TechnologyAdvice) for use at Mozilla, specifically as a part of SHIELD and Normandy.

Features not supported yet:

* JavaScript object properties (e.g. [String.length](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/length))
* Adding custom operators (binary/unary)

Other implementations:

* [JavaScript](https://github.com/mozilla/mozjexl)
* [Python](https://github.com/mozilla/pyjexl)

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:jexl:{latest-version}
```

### Evaluating expressions

```Kotlin
val jexl = Jexl()

val result = jexl.evaluate("75 > 42")

// evaluate() returns an object of type JexlValue. Calling toKotlin() converts this
// into a matching Kotlin type (in this case a Boolean).
println(result.value) // Prints "true"
```

Often expressions should return a `Boolean`value. In this case `evaluateBooleanExpression` is a helper that always returns a Kotlin `Boolean` and never throws an exception (Returns false).

```Kotlin
val jexl = Jexl()

// "result" has type Boolean and value "true"
val result = jexl.evaluateBooleanExpression("42 + 23 > 50", defaultValue = false)
```


### Unary Operators

| Operation | Symbol |
|-----------|:------:|
| Negate    |    !   |

### Binary Operators

| Operation        |      Symbol      |
|------------------|:----------------:|
| Add, Concat      |         +        |
| Subtract         |         -        |
| Multiply         |         *        |
| Divide           |         /        |
| Divide and floor |        //        |
| Modulus          |         %        |
| Power of         |         ^        |
| Logical AND      |        &&        |
| Logical OR       |   &#124;&#124;   |

### Comparison

| Comparison                 | Symbol |
|----------------------------|:------:|
| Equal                      |   ==   |
| Not equal                  |   !=   |
| Greater than               |    >   |
| Greater than or equal      |   >=   |
| Less than                  |    <   |
| Less than or equal         |   <=   |
| Element in array or string |   in   |

### Ternary operator

Conditional expressions check to see if the first segment evaluates to a truthy
value. If so, the consequent segment is evaluated.  Otherwise, the alternate
is. If the consequent section is missing, the test result itself will be used
instead.

| Expression                          | Result |
|-------------------------------------|--------|
| `"" ? "Full" : "Empty"`             | Empty  |
| `"foo" in "foobar" ? "Yes" : "No"`  | Yes    |
| `{agent: "Archer"}.agent ?: "Kane"` | Archer |

### Native Types

| Type      |            Examples            |
|-----------|:------------------------------:|
| Booleans  |         `true`, `false`        |
| Strings   | "Hello \"user\"", 'Hey there!' |
| Integers  |          6, -7, 5, -3          |
| Doubles   |         -7.2, -3.14159         |
| Objects   |        {hello: "world!"}       |
| Arrays    |       ['hello', 'world!']      |
| Undefined |          `undefined`           |

The JavaScript implementation of Jexl uses a `Numeric` type. This implementation dynamically casts between `Integer` and `Double` as needed.

### Groups

Parentheses work just how you'd expect them to:

| Expression                            | Result |
|---------------------------------------|:-------|
| `(83 + 1) / 2`                        | 42     |
| `1 < 3 && (4 > 2 &#124;&#124; 2 > 4)` | true   |

### Identifiers

Access variables in the context object by just typing their name. Objects can
be traversed with dot notation, or by using brackets to traverse to a dynamic
property name.

Example context:

```javascript
{
    name: {
        first: "Malory",
        last: "Archer"
    },
    exes: [
        "Nikolai Jakov",
        "Len Trexler",
        "Burt Reynolds"
    ],
    lastEx: 2
}
```

| Expression          | Result        |
|---------------------|---------------|
| `name.first`        | Malory        |
| `name['la' + 'st']` | Archer        |
| `exes[2]`           | Burt Reynolds |
| `exes[lastEx - 1]`  | Len Trexler   |

### Collections

Collections, or arrays of objects, can be filtered by including a filter
expression in brackets. Properties of each collection can be referenced by
prefixing them with a leading dot. The result will be an array of the objects
for which the filter expression resulted in a truthy value.

Example context:

```javascript
{
    employees: [
        {first: 'Sterling', last: 'Archer', age: 36},
        {first: 'Malory', last: 'Archer', age: 75},
        {first: 'Lana', last: 'Kane', age: 33},
        {first: 'Cyril', last: 'Figgis', age: 45},
        {first: 'Cheryl', last: 'Tunt', age: 28}
    ],
    retireAge: 62
}
```

| Expression                                      | Result                                                                                |
|-------------------------------------------------|---------------------------------------------------------------------------------------|
| `employees[.first == 'Sterling']`               | [{first: 'Sterling', last: 'Archer', age: 36}]                                        |
| `employees[.last == 'Tu' + 'nt'].first`         | Cheryl                                                                                |
| `employees[.age >= 30 && .age < 40]`            | [{first: 'Sterling', last: 'Archer', age: 36},{first: 'Lana', last: 'Kane', age: 33}] |
| `employees[.age >= 30 && .age < 40][.age < 35]` | [{first: 'Lana', last: 'Kane', age: 33}]                                              |
| `employees[.age >= retireAge].first`            | Malory                                                                                |

### Transforms

The power of Jexl is in transforming data. Transform functions take one or more arguments: The value to be transformed, followed by anything else passed to it in the expression.

```Kotlin
val jexl = Jexl()

jexl.addTransform("split") { value, arguments ->
    value.toString().split(arguments.first().toString()).toJexlArray()
}

jexl.addTransform("lower") { value, _ ->
    value.toString().toLowerCase().toJexl()
}

jexl.addTransform("last") { value, _ ->
    (value as JexlArray).values.last()
}
```

| Expression                                      | Result                |
|-------------------------------------------------|-----------------------|
| `"Pam Poovey"&#124;lower&#124;split(' ')|first` | poovey                |
| `"password==guest"&#124;split('=' + '=')`       | ['password', 'guest'] |

### Context

Variable contexts are straightforward Objects that can be accessed
in the expression.

```Kotlin
val context = Context(
    "employees" to JexlArray(
        JexlObject(
            "first" to "Sterling".toJexl(),
            "last" to "Archer".toJexl(),
            "age" to 36.toJexl()),
        JexlObject(
            "first" to "Malory".toJexl(),
            "last" to "Archer".toJexl(),
            "age" to 75.toJexl()),
        JexlObject(
            "first" to "Malory".toJexl(),
            "last" to "Archer".toJexl(),
            "age" to 33.toJexl())
    )
)
```

| Expression                                      | Result                                                                       |
|-------------------------------------------------|------------------------------------------------------------------------------|
| `employees[.age >= 30 && .age < 40]`            | [{first=Sterling, last=Archer, age=36}, {first=Malory, last=Archer, age=33}] |
| `employees[.age >= 30 && .age < 90][.age < 37]` | [{first=Malory, last=Archer, age=33}]                                        |


## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
