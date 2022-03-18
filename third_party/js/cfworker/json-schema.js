/*
 * Copyright (c) 2020 Jeremy Danyow
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

'use strict';

function deepCompareStrict(a, b) {
    const typeofa = typeof a;
    if (typeofa !== typeof b) {
        return false;
    }
    if (Array.isArray(a)) {
        if (!Array.isArray(b)) {
            return false;
        }
        const length = a.length;
        if (length !== b.length) {
            return false;
        }
        for (let i = 0; i < length; i++) {
            if (!deepCompareStrict(a[i], b[i])) {
                return false;
            }
        }
        return true;
    }
    if (typeofa === 'object') {
        if (!a || !b) {
            return a === b;
        }
        const aKeys = Object.keys(a);
        const bKeys = Object.keys(b);
        const length = aKeys.length;
        if (length !== bKeys.length) {
            return false;
        }
        for (const k of aKeys) {
            if (!deepCompareStrict(a[k], b[k])) {
                return false;
            }
        }
        return true;
    }
    return a === b;
}

function encodePointer(p) {
    return encodeURI(escapePointer(p));
}
function escapePointer(p) {
    return p.replace(/~/g, '~0').replace(/\//g, '~1');
}

const schemaKeyword = {
    additionalItems: true,
    unevaluatedItems: true,
    items: true,
    contains: true,
    additionalProperties: true,
    unevaluatedProperties: true,
    propertyNames: true,
    not: true,
    if: true,
    then: true,
    else: true
};
const schemaArrayKeyword = {
    prefixItems: true,
    items: true,
    allOf: true,
    anyOf: true,
    oneOf: true
};
const schemaMapKeyword = {
    $defs: true,
    definitions: true,
    properties: true,
    patternProperties: true,
    dependentSchemas: true
};
const ignoredKeyword = {
    id: true,
    $id: true,
    $ref: true,
    $schema: true,
    $anchor: true,
    $vocabulary: true,
    $comment: true,
    default: true,
    enum: true,
    const: true,
    required: true,
    type: true,
    maximum: true,
    minimum: true,
    exclusiveMaximum: true,
    exclusiveMinimum: true,
    multipleOf: true,
    maxLength: true,
    minLength: true,
    pattern: true,
    format: true,
    maxItems: true,
    minItems: true,
    uniqueItems: true,
    maxProperties: true,
    minProperties: true
};
let initialBaseURI = typeof self !== 'undefined' && self.location
    ?
        new URL(self.location.origin + self.location.pathname + location.search)
    : new URL('https://github.com/cfworker');
function dereference(schema, lookup = Object.create(null), baseURI = initialBaseURI, basePointer = '') {
    if (schema && typeof schema === 'object' && !Array.isArray(schema)) {
        const id = schema.$id || schema.id;
        if (id) {
            const url = new URL(id, baseURI.href);
            if (url.hash.length > 1) {
                lookup[url.href] = schema;
            }
            else {
                url.hash = '';
                if (basePointer === '') {
                    baseURI = url;
                }
                else {
                    dereference(schema, lookup, baseURI);
                }
            }
        }
    }
    else if (schema !== true && schema !== false) {
        return lookup;
    }
    const schemaURI = baseURI.href + (basePointer ? '#' + basePointer : '');
    if (lookup[schemaURI] !== undefined) {
        throw new Error(`Duplicate schema URI "${schemaURI}".`);
    }
    lookup[schemaURI] = schema;
    if (schema === true || schema === false) {
        return lookup;
    }
    if (schema.__absolute_uri__ === undefined) {
        Object.defineProperty(schema, '__absolute_uri__', {
            enumerable: false,
            value: schemaURI
        });
    }
    if (schema.$ref && schema.__absolute_ref__ === undefined) {
        const url = new URL(schema.$ref, baseURI.href);
        url.hash = url.hash;
        Object.defineProperty(schema, '__absolute_ref__', {
            enumerable: false,
            value: url.href
        });
    }
    if (schema.$recursiveRef && schema.__absolute_recursive_ref__ === undefined) {
        const url = new URL(schema.$recursiveRef, baseURI.href);
        url.hash = url.hash;
        Object.defineProperty(schema, '__absolute_recursive_ref__', {
            enumerable: false,
            value: url.href
        });
    }
    if (schema.$anchor) {
        const url = new URL('#' + schema.$anchor, baseURI.href);
        lookup[url.href] = schema;
    }
    for (let key in schema) {
        if (ignoredKeyword[key]) {
            continue;
        }
        const keyBase = `${basePointer}/${encodePointer(key)}`;
        const subSchema = schema[key];
        if (Array.isArray(subSchema)) {
            if (schemaArrayKeyword[key]) {
                const length = subSchema.length;
                for (let i = 0; i < length; i++) {
                    dereference(subSchema[i], lookup, baseURI, `${keyBase}/${i}`);
                }
            }
        }
        else if (schemaMapKeyword[key]) {
            for (let subKey in subSchema) {
                dereference(subSchema[subKey], lookup, baseURI, `${keyBase}/${encodePointer(subKey)}`);
            }
        }
        else {
            dereference(subSchema, lookup, baseURI, keyBase);
        }
    }
    return lookup;
}

const DATE = /^(\d\d\d\d)-(\d\d)-(\d\d)$/;
const DAYS = [0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
const TIME = /^(\d\d):(\d\d):(\d\d)(\.\d+)?(z|[+-]\d\d(?::?\d\d)?)?$/i;
const HOSTNAME = /^(?=.{1,253}\.?$)[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?(?:\.[a-z0-9](?:[-0-9a-z]{0,61}[0-9a-z])?)*\.?$/i;
const URIREF = /^(?:[a-z][a-z0-9+\-.]*:)?(?:\/?\/(?:(?:[a-z0-9\-._~!$&'()*+,;=:]|%[0-9a-f]{2})*@)?(?:\[(?:(?:(?:(?:[0-9a-f]{1,4}:){6}|::(?:[0-9a-f]{1,4}:){5}|(?:[0-9a-f]{1,4})?::(?:[0-9a-f]{1,4}:){4}|(?:(?:[0-9a-f]{1,4}:){0,1}[0-9a-f]{1,4})?::(?:[0-9a-f]{1,4}:){3}|(?:(?:[0-9a-f]{1,4}:){0,2}[0-9a-f]{1,4})?::(?:[0-9a-f]{1,4}:){2}|(?:(?:[0-9a-f]{1,4}:){0,3}[0-9a-f]{1,4})?::[0-9a-f]{1,4}:|(?:(?:[0-9a-f]{1,4}:){0,4}[0-9a-f]{1,4})?::)(?:[0-9a-f]{1,4}:[0-9a-f]{1,4}|(?:(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(?:25[0-5]|2[0-4]\d|[01]?\d\d?))|(?:(?:[0-9a-f]{1,4}:){0,5}[0-9a-f]{1,4})?::[0-9a-f]{1,4}|(?:(?:[0-9a-f]{1,4}:){0,6}[0-9a-f]{1,4})?::)|[Vv][0-9a-f]+\.[a-z0-9\-._~!$&'()*+,;=:]+)\]|(?:(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(?:25[0-5]|2[0-4]\d|[01]?\d\d?)|(?:[a-z0-9\-._~!$&'"()*+,;=]|%[0-9a-f]{2})*)(?::\d*)?(?:\/(?:[a-z0-9\-._~!$&'"()*+,;=:@]|%[0-9a-f]{2})*)*|\/(?:(?:[a-z0-9\-._~!$&'"()*+,;=:@]|%[0-9a-f]{2})+(?:\/(?:[a-z0-9\-._~!$&'"()*+,;=:@]|%[0-9a-f]{2})*)*)?|(?:[a-z0-9\-._~!$&'"()*+,;=:@]|%[0-9a-f]{2})+(?:\/(?:[a-z0-9\-._~!$&'"()*+,;=:@]|%[0-9a-f]{2})*)*)?(?:\?(?:[a-z0-9\-._~!$&'"()*+,;=:@/?]|%[0-9a-f]{2})*)?(?:#(?:[a-z0-9\-._~!$&'"()*+,;=:@/?]|%[0-9a-f]{2})*)?$/i;
const URITEMPLATE = /^(?:(?:[^\x00-\x20"'<>%\\^`{|}]|%[0-9a-f]{2})|\{[+#./;?&=,!@|]?(?:[a-z0-9_]|%[0-9a-f]{2})+(?::[1-9][0-9]{0,3}|\*)?(?:,(?:[a-z0-9_]|%[0-9a-f]{2})+(?::[1-9][0-9]{0,3}|\*)?)*\})*$/i;
const URL_ = /^(?:(?:https?|ftp):\/\/)(?:\S+(?::\S*)?@)?(?:(?!10(?:\.\d{1,3}){3})(?!127(?:\.\d{1,3}){3})(?!169\.254(?:\.\d{1,3}){2})(?!192\.168(?:\.\d{1,3}){2})(?!172\.(?:1[6-9]|2\d|3[0-1])(?:\.\d{1,3}){2})(?:[1-9]\d?|1\d\d|2[01]\d|22[0-3])(?:\.(?:1?\d{1,2}|2[0-4]\d|25[0-5])){2}(?:\.(?:[1-9]\d?|1\d\d|2[0-4]\d|25[0-4]))|(?:(?:[a-z\u{00a1}-\u{ffff}0-9]+-?)*[a-z\u{00a1}-\u{ffff}0-9]+)(?:\.(?:[a-z\u{00a1}-\u{ffff}0-9]+-?)*[a-z\u{00a1}-\u{ffff}0-9]+)*(?:\.(?:[a-z\u{00a1}-\u{ffff}]{2,})))(?::\d{2,5})?(?:\/[^\s]*)?$/iu;
const UUID = /^(?:urn:uuid:)?[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}$/i;
const JSON_POINTER = /^(?:\/(?:[^~/]|~0|~1)*)*$/;
const JSON_POINTER_URI_FRAGMENT = /^#(?:\/(?:[a-z0-9_\-.!$&'()*+,;:=@]|%[0-9a-f]{2}|~0|~1)*)*$/i;
const RELATIVE_JSON_POINTER = /^(?:0|[1-9][0-9]*)(?:#|(?:\/(?:[^~/]|~0|~1)*)*)$/;
const FASTDATE = /^\d\d\d\d-[0-1]\d-[0-3]\d$/;
const FASTTIME = /^(?:[0-2]\d:[0-5]\d:[0-5]\d|23:59:60)(?:\.\d+)?(?:z|[+-]\d\d(?::?\d\d)?)?$/i;
const FASTDATETIME = /^\d\d\d\d-[0-1]\d-[0-3]\d[t\s](?:[0-2]\d:[0-5]\d:[0-5]\d|23:59:60)(?:\.\d+)?(?:z|[+-]\d\d(?::?\d\d)?)$/i;
const FASTURIREFERENCE = /^(?:(?:[a-z][a-z0-9+-.]*:)?\/?\/)?(?:[^\\\s#][^\s#]*)?(?:#[^\\\s]*)?$/i;
const EMAIL = (input) => {
    if (input[0] === '"')
        return false;
    const [name, host, ...rest] = input.split('@');
    if (!name ||
        !host ||
        rest.length !== 0 ||
        name.length > 64 ||
        host.length > 253)
        return false;
    if (name[0] === '.' || name.endsWith('.') || name.includes('..'))
        return false;
    if (!/^[a-z0-9.-]+$/i.test(host) ||
        !/^[a-z0-9.!#$%&'*+/=?^_`{|}~-]+$/i.test(name))
        return false;
    return host
        .split('.')
        .every(part => /^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$/i.test(part));
};
const IPV4 = /^(?:(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(?:25[0-5]|2[0-4]\d|[01]?\d\d?)$/;
const IPV6 = /^((([0-9a-f]{1,4}:){7}([0-9a-f]{1,4}|:))|(([0-9a-f]{1,4}:){6}(:[0-9a-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9a-f]{1,4}:){5}(((:[0-9a-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9a-f]{1,4}:){4}(((:[0-9a-f]{1,4}){1,3})|((:[0-9a-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9a-f]{1,4}:){3}(((:[0-9a-f]{1,4}){1,4})|((:[0-9a-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9a-f]{1,4}:){2}(((:[0-9a-f]{1,4}){1,5})|((:[0-9a-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9a-f]{1,4}:){1}(((:[0-9a-f]{1,4}){1,6})|((:[0-9a-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9a-f]{1,4}){1,7})|((:[0-9a-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))$/i;
const DURATION = (input) => input.length > 1 &&
    input.length < 80 &&
    (/^P\d+([.,]\d+)?W$/.test(input) ||
        (/^P[\dYMDTHS]*(\d[.,]\d+)?[YMDHS]$/.test(input) &&
            /^P([.,\d]+Y)?([.,\d]+M)?([.,\d]+D)?(T([.,\d]+H)?([.,\d]+M)?([.,\d]+S)?)?$/.test(input)));
function bind(r) {
    return r.test.bind(r);
}
const fullFormat = {
    date,
    time: time.bind(undefined, false),
    'date-time': date_time,
    duration: DURATION,
    uri,
    'uri-reference': bind(URIREF),
    'uri-template': bind(URITEMPLATE),
    url: bind(URL_),
    email: EMAIL,
    hostname: bind(HOSTNAME),
    ipv4: bind(IPV4),
    ipv6: bind(IPV6),
    regex: regex,
    uuid: bind(UUID),
    'json-pointer': bind(JSON_POINTER),
    'json-pointer-uri-fragment': bind(JSON_POINTER_URI_FRAGMENT),
    'relative-json-pointer': bind(RELATIVE_JSON_POINTER)
};
const fastFormat = {
    ...fullFormat,
    date: bind(FASTDATE),
    time: bind(FASTTIME),
    'date-time': bind(FASTDATETIME),
    'uri-reference': bind(FASTURIREFERENCE)
};
function isLeapYear(year) {
    return year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
}
function date(str) {
    const matches = str.match(DATE);
    if (!matches)
        return false;
    const year = +matches[1];
    const month = +matches[2];
    const day = +matches[3];
    return (month >= 1 &&
        month <= 12 &&
        day >= 1 &&
        day <= (month == 2 && isLeapYear(year) ? 29 : DAYS[month]));
}
function time(full, str) {
    const matches = str.match(TIME);
    if (!matches)
        return false;
    const hour = +matches[1];
    const minute = +matches[2];
    const second = +matches[3];
    const timeZone = !!matches[5];
    return (((hour <= 23 && minute <= 59 && second <= 59) ||
        (hour == 23 && minute == 59 && second == 60)) &&
        (!full || timeZone));
}
const DATE_TIME_SEPARATOR = /t|\s/i;
function date_time(str) {
    const dateTime = str.split(DATE_TIME_SEPARATOR);
    return dateTime.length == 2 && date(dateTime[0]) && time(true, dateTime[1]);
}
const NOT_URI_FRAGMENT = /\/|:/;
const URI_PATTERN = /^(?:[a-z][a-z0-9+\-.]*:)(?:\/?\/(?:(?:[a-z0-9\-._~!$&'()*+,;=:]|%[0-9a-f]{2})*@)?(?:\[(?:(?:(?:(?:[0-9a-f]{1,4}:){6}|::(?:[0-9a-f]{1,4}:){5}|(?:[0-9a-f]{1,4})?::(?:[0-9a-f]{1,4}:){4}|(?:(?:[0-9a-f]{1,4}:){0,1}[0-9a-f]{1,4})?::(?:[0-9a-f]{1,4}:){3}|(?:(?:[0-9a-f]{1,4}:){0,2}[0-9a-f]{1,4})?::(?:[0-9a-f]{1,4}:){2}|(?:(?:[0-9a-f]{1,4}:){0,3}[0-9a-f]{1,4})?::[0-9a-f]{1,4}:|(?:(?:[0-9a-f]{1,4}:){0,4}[0-9a-f]{1,4})?::)(?:[0-9a-f]{1,4}:[0-9a-f]{1,4}|(?:(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(?:25[0-5]|2[0-4]\d|[01]?\d\d?))|(?:(?:[0-9a-f]{1,4}:){0,5}[0-9a-f]{1,4})?::[0-9a-f]{1,4}|(?:(?:[0-9a-f]{1,4}:){0,6}[0-9a-f]{1,4})?::)|[Vv][0-9a-f]+\.[a-z0-9\-._~!$&'()*+,;=:]+)\]|(?:(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(?:25[0-5]|2[0-4]\d|[01]?\d\d?)|(?:[a-z0-9\-._~!$&'()*+,;=]|%[0-9a-f]{2})*)(?::\d*)?(?:\/(?:[a-z0-9\-._~!$&'()*+,;=:@]|%[0-9a-f]{2})*)*|\/(?:(?:[a-z0-9\-._~!$&'()*+,;=:@]|%[0-9a-f]{2})+(?:\/(?:[a-z0-9\-._~!$&'()*+,;=:@]|%[0-9a-f]{2})*)*)?|(?:[a-z0-9\-._~!$&'()*+,;=:@]|%[0-9a-f]{2})+(?:\/(?:[a-z0-9\-._~!$&'()*+,;=:@]|%[0-9a-f]{2})*)*)(?:\?(?:[a-z0-9\-._~!$&'()*+,;=:@/?]|%[0-9a-f]{2})*)?(?:#(?:[a-z0-9\-._~!$&'()*+,;=:@/?]|%[0-9a-f]{2})*)?$/i;
function uri(str) {
    return NOT_URI_FRAGMENT.test(str) && URI_PATTERN.test(str);
}
const Z_ANCHOR = /[^\\]\\Z/;
function regex(str) {
    if (Z_ANCHOR.test(str))
        return false;
    try {
        new RegExp(str);
        return true;
    }
    catch (e) {
        return false;
    }
}

function ucs2length(s) {
    let result = 0;
    let length = s.length;
    let index = 0;
    let charCode;
    while (index < length) {
        result++;
        charCode = s.charCodeAt(index++);
        if (charCode >= 0xd800 && charCode <= 0xdbff && index < length) {
            charCode = s.charCodeAt(index);
            if ((charCode & 0xfc00) == 0xdc00) {
                index++;
            }
        }
    }
    return result;
}

function validate(instance, schema, draft = '2019-09', lookup = dereference(schema), shortCircuit = true, recursiveAnchor = null, instanceLocation = '#', schemaLocation = '#', evaluated = Object.create(null)) {
    if (schema === true) {
        return { valid: true, errors: [] };
    }
    if (schema === false) {
        return {
            valid: false,
            errors: [
                {
                    instanceLocation,
                    keyword: 'false',
                    keywordLocation: instanceLocation,
                    error: 'False boolean schema.'
                }
            ]
        };
    }
    const rawInstanceType = typeof instance;
    let instanceType;
    switch (rawInstanceType) {
        case 'boolean':
        case 'number':
        case 'string':
            instanceType = rawInstanceType;
            break;
        case 'object':
            if (instance === null) {
                instanceType = 'null';
            }
            else if (Array.isArray(instance)) {
                instanceType = 'array';
            }
            else {
                instanceType = 'object';
            }
            break;
        default:
            throw new Error(`Instances of "${rawInstanceType}" type are not supported.`);
    }
    const { $ref, $recursiveRef, $recursiveAnchor, type: $type, const: $const, enum: $enum, required: $required, not: $not, anyOf: $anyOf, allOf: $allOf, oneOf: $oneOf, if: $if, then: $then, else: $else, format: $format, properties: $properties, patternProperties: $patternProperties, additionalProperties: $additionalProperties, unevaluatedProperties: $unevaluatedProperties, minProperties: $minProperties, maxProperties: $maxProperties, propertyNames: $propertyNames, dependentRequired: $dependentRequired, dependentSchemas: $dependentSchemas, dependencies: $dependencies, prefixItems: $prefixItems, items: $items, additionalItems: $additionalItems, unevaluatedItems: $unevaluatedItems, contains: $contains, minContains: $minContains, maxContains: $maxContains, minItems: $minItems, maxItems: $maxItems, uniqueItems: $uniqueItems, minimum: $minimum, maximum: $maximum, exclusiveMinimum: $exclusiveMinimum, exclusiveMaximum: $exclusiveMaximum, multipleOf: $multipleOf, minLength: $minLength, maxLength: $maxLength, pattern: $pattern, __absolute_ref__, __absolute_recursive_ref__ } = schema;
    const errors = [];
    if ($recursiveAnchor === true && recursiveAnchor === null) {
        recursiveAnchor = schema;
    }
    if ($recursiveRef === '#') {
        const refSchema = recursiveAnchor === null
            ? lookup[__absolute_recursive_ref__]
            : recursiveAnchor;
        const keywordLocation = `${schemaLocation}/$recursiveRef`;
        const result = validate(instance, recursiveAnchor === null ? schema : recursiveAnchor, draft, lookup, shortCircuit, refSchema, instanceLocation, keywordLocation, evaluated);
        if (!result.valid) {
            errors.push({
                instanceLocation,
                keyword: '$recursiveRef',
                keywordLocation,
                error: 'A subschema had errors.'
            }, ...result.errors);
        }
    }
    if ($ref !== undefined) {
        const uri = __absolute_ref__ || $ref;
        const refSchema = lookup[uri];
        if (refSchema === undefined) {
            let message = `Unresolved $ref "${$ref}".`;
            if (__absolute_ref__ && __absolute_ref__ !== $ref) {
                message += `  Absolute URI "${__absolute_ref__}".`;
            }
            message += `\nKnown schemas:\n- ${Object.keys(lookup).join('\n- ')}`;
            throw new Error(message);
        }
        const keywordLocation = `${schemaLocation}/$ref`;
        const result = validate(instance, refSchema, draft, lookup, shortCircuit, recursiveAnchor, instanceLocation, keywordLocation, evaluated);
        if (!result.valid) {
            errors.push({
                instanceLocation,
                keyword: '$ref',
                keywordLocation,
                error: 'A subschema had errors.'
            }, ...result.errors);
        }
        if (draft === '4' || draft === '7') {
            return { valid: errors.length === 0, errors };
        }
    }
    if (Array.isArray($type)) {
        let length = $type.length;
        let valid = false;
        for (let i = 0; i < length; i++) {
            if (instanceType === $type[i] ||
                ($type[i] === 'integer' &&
                    instanceType === 'number' &&
                    instance % 1 === 0 &&
                    instance === instance)) {
                valid = true;
                break;
            }
        }
        if (!valid) {
            errors.push({
                instanceLocation,
                keyword: 'type',
                keywordLocation: `${schemaLocation}/type`,
                error: `Instance type "${instanceType}" is invalid. Expected "${$type.join('", "')}".`
            });
        }
    }
    else if ($type === 'integer') {
        if (instanceType !== 'number' || instance % 1 || instance !== instance) {
            errors.push({
                instanceLocation,
                keyword: 'type',
                keywordLocation: `${schemaLocation}/type`,
                error: `Instance type "${instanceType}" is invalid. Expected "${$type}".`
            });
        }
    }
    else if ($type !== undefined && instanceType !== $type) {
        errors.push({
            instanceLocation,
            keyword: 'type',
            keywordLocation: `${schemaLocation}/type`,
            error: `Instance type "${instanceType}" is invalid. Expected "${$type}".`
        });
    }
    if ($const !== undefined) {
        if (instanceType === 'object' || instanceType === 'array') {
            if (!deepCompareStrict(instance, $const)) {
                errors.push({
                    instanceLocation,
                    keyword: 'const',
                    keywordLocation: `${schemaLocation}/const`,
                    error: `Instance does not match ${JSON.stringify($const)}.`
                });
            }
        }
        else if (instance !== $const) {
            errors.push({
                instanceLocation,
                keyword: 'const',
                keywordLocation: `${schemaLocation}/const`,
                error: `Instance does not match ${JSON.stringify($const)}.`
            });
        }
    }
    if ($enum !== undefined) {
        if (instanceType === 'object' || instanceType === 'array') {
            if (!$enum.some(value => deepCompareStrict(instance, value))) {
                errors.push({
                    instanceLocation,
                    keyword: 'enum',
                    keywordLocation: `${schemaLocation}/enum`,
                    error: `Instance does not match any of ${JSON.stringify($enum)}.`
                });
            }
        }
        else if (!$enum.some(value => instance === value)) {
            errors.push({
                instanceLocation,
                keyword: 'enum',
                keywordLocation: `${schemaLocation}/enum`,
                error: `Instance does not match any of ${JSON.stringify($enum)}.`
            });
        }
    }
    if ($not !== undefined) {
        const keywordLocation = `${schemaLocation}/not`;
        const result = validate(instance, $not, draft, lookup, shortCircuit, recursiveAnchor, instanceLocation, keywordLocation);
        if (result.valid) {
            errors.push({
                instanceLocation,
                keyword: 'not',
                keywordLocation,
                error: 'Instance matched "not" schema.'
            });
        }
    }
    let subEvaluateds = [];
    if ($anyOf !== undefined) {
        const keywordLocation = `${schemaLocation}/anyOf`;
        const errorsLength = errors.length;
        let anyValid = false;
        for (let i = 0; i < $anyOf.length; i++) {
            const subSchema = $anyOf[i];
            const subEvaluated = Object.create(evaluated);
            const result = validate(instance, subSchema, draft, lookup, shortCircuit, $recursiveAnchor === true ? recursiveAnchor : null, instanceLocation, `${keywordLocation}/${i}`, subEvaluated);
            errors.push(...result.errors);
            anyValid = anyValid || result.valid;
            if (result.valid) {
                subEvaluateds.push(subEvaluated);
            }
        }
        if (anyValid) {
            errors.length = errorsLength;
        }
        else {
            errors.splice(errorsLength, 0, {
                instanceLocation,
                keyword: 'anyOf',
                keywordLocation,
                error: 'Instance does not match any subschemas.'
            });
        }
    }
    if ($allOf !== undefined) {
        const keywordLocation = `${schemaLocation}/allOf`;
        const errorsLength = errors.length;
        let allValid = true;
        for (let i = 0; i < $allOf.length; i++) {
            const subSchema = $allOf[i];
            const subEvaluated = Object.create(evaluated);
            const result = validate(instance, subSchema, draft, lookup, shortCircuit, $recursiveAnchor === true ? recursiveAnchor : null, instanceLocation, `${keywordLocation}/${i}`, subEvaluated);
            errors.push(...result.errors);
            allValid = allValid && result.valid;
            if (result.valid) {
                subEvaluateds.push(subEvaluated);
            }
        }
        if (allValid) {
            errors.length = errorsLength;
        }
        else {
            errors.splice(errorsLength, 0, {
                instanceLocation,
                keyword: 'allOf',
                keywordLocation,
                error: `Instance does not match every subschema.`
            });
        }
    }
    if ($oneOf !== undefined) {
        const keywordLocation = `${schemaLocation}/oneOf`;
        const errorsLength = errors.length;
        const matches = $oneOf.filter((subSchema, i) => {
            const subEvaluated = Object.create(evaluated);
            const result = validate(instance, subSchema, draft, lookup, shortCircuit, $recursiveAnchor === true ? recursiveAnchor : null, instanceLocation, `${keywordLocation}/${i}`, subEvaluated);
            errors.push(...result.errors);
            if (result.valid) {
                subEvaluateds.push(subEvaluated);
            }
            return result.valid;
        }).length;
        if (matches === 1) {
            errors.length = errorsLength;
        }
        else {
            errors.splice(errorsLength, 0, {
                instanceLocation,
                keyword: 'oneOf',
                keywordLocation,
                error: `Instance does not match exactly one subschema (${matches} matches).`
            });
        }
    }
    if (instanceType === 'object' || instanceType === 'array') {
        Object.assign(evaluated, ...subEvaluateds);
    }
    if ($if !== undefined) {
        const keywordLocation = `${schemaLocation}/if`;
        const conditionResult = validate(instance, $if, draft, lookup, shortCircuit, recursiveAnchor, instanceLocation, keywordLocation, evaluated).valid;
        if (conditionResult) {
            if ($then !== undefined) {
                const thenResult = validate(instance, $then, draft, lookup, shortCircuit, recursiveAnchor, instanceLocation, `${schemaLocation}/then`, evaluated);
                if (!thenResult.valid) {
                    errors.push({
                        instanceLocation,
                        keyword: 'if',
                        keywordLocation,
                        error: `Instance does not match "then" schema.`
                    }, ...thenResult.errors);
                }
            }
        }
        else if ($else !== undefined) {
            const elseResult = validate(instance, $else, draft, lookup, shortCircuit, recursiveAnchor, instanceLocation, `${schemaLocation}/else`, evaluated);
            if (!elseResult.valid) {
                errors.push({
                    instanceLocation,
                    keyword: 'if',
                    keywordLocation,
                    error: `Instance does not match "else" schema.`
                }, ...elseResult.errors);
            }
        }
    }
    if (instanceType === 'object') {
        if ($required !== undefined) {
            for (const key of $required) {
                if (!(key in instance)) {
                    errors.push({
                        instanceLocation,
                        keyword: 'required',
                        keywordLocation: `${schemaLocation}/required`,
                        error: `Instance does not have required property "${key}".`
                    });
                }
            }
        }
        const keys = Object.keys(instance);
        if ($minProperties !== undefined && keys.length < $minProperties) {
            errors.push({
                instanceLocation,
                keyword: 'minProperties',
                keywordLocation: `${schemaLocation}/minProperties`,
                error: `Instance does not have at least ${$minProperties} properties.`
            });
        }
        if ($maxProperties !== undefined && keys.length > $maxProperties) {
            errors.push({
                instanceLocation,
                keyword: 'maxProperties',
                keywordLocation: `${schemaLocation}/maxProperties`,
                error: `Instance does not have at least ${$maxProperties} properties.`
            });
        }
        if ($propertyNames !== undefined) {
            const keywordLocation = `${schemaLocation}/propertyNames`;
            for (const key in instance) {
                const subInstancePointer = `${instanceLocation}/${encodePointer(key)}`;
                const result = validate(key, $propertyNames, draft, lookup, shortCircuit, recursiveAnchor, subInstancePointer, keywordLocation);
                if (!result.valid) {
                    errors.push({
                        instanceLocation,
                        keyword: 'propertyNames',
                        keywordLocation,
                        error: `Property name "${key}" does not match schema.`
                    }, ...result.errors);
                }
            }
        }
        if ($dependentRequired !== undefined) {
            const keywordLocation = `${schemaLocation}/dependantRequired`;
            for (const key in $dependentRequired) {
                if (key in instance) {
                    const required = $dependentRequired[key];
                    for (const dependantKey of required) {
                        if (!(dependantKey in instance)) {
                            errors.push({
                                instanceLocation,
                                keyword: 'dependentRequired',
                                keywordLocation,
                                error: `Instance has "${key}" but does not have "${dependantKey}".`
                            });
                        }
                    }
                }
            }
        }
        if ($dependentSchemas !== undefined) {
            for (const key in $dependentSchemas) {
                const keywordLocation = `${schemaLocation}/dependentSchemas`;
                if (key in instance) {
                    const result = validate(instance, $dependentSchemas[key], draft, lookup, shortCircuit, recursiveAnchor, instanceLocation, `${keywordLocation}/${encodePointer(key)}`, evaluated);
                    if (!result.valid) {
                        errors.push({
                            instanceLocation,
                            keyword: 'dependentSchemas',
                            keywordLocation,
                            error: `Instance has "${key}" but does not match dependant schema.`
                        }, ...result.errors);
                    }
                }
            }
        }
        if ($dependencies !== undefined) {
            const keywordLocation = `${schemaLocation}/dependencies`;
            for (const key in $dependencies) {
                if (key in instance) {
                    const propsOrSchema = $dependencies[key];
                    if (Array.isArray(propsOrSchema)) {
                        for (const dependantKey of propsOrSchema) {
                            if (!(dependantKey in instance)) {
                                errors.push({
                                    instanceLocation,
                                    keyword: 'dependencies',
                                    keywordLocation,
                                    error: `Instance has "${key}" but does not have "${dependantKey}".`
                                });
                            }
                        }
                    }
                    else {
                        const result = validate(instance, propsOrSchema, draft, lookup, shortCircuit, recursiveAnchor, instanceLocation, `${keywordLocation}/${encodePointer(key)}`);
                        if (!result.valid) {
                            errors.push({
                                instanceLocation,
                                keyword: 'dependencies',
                                keywordLocation,
                                error: `Instance has "${key}" but does not match dependant schema.`
                            }, ...result.errors);
                        }
                    }
                }
            }
        }
        const thisEvaluated = Object.create(null);
        let stop = false;
        if ($properties !== undefined) {
            const keywordLocation = `${schemaLocation}/properties`;
            for (const key in $properties) {
                if (!(key in instance)) {
                    continue;
                }
                const subInstancePointer = `${instanceLocation}/${encodePointer(key)}`;
                const result = validate(instance[key], $properties[key], draft, lookup, shortCircuit, recursiveAnchor, subInstancePointer, `${keywordLocation}/${encodePointer(key)}`);
                if (result.valid) {
                    evaluated[key] = thisEvaluated[key] = true;
                }
                else {
                    stop = shortCircuit;
                    errors.push({
                        instanceLocation,
                        keyword: 'properties',
                        keywordLocation,
                        error: `Property "${key}" does not match schema.`
                    }, ...result.errors);
                    if (stop)
                        break;
                }
            }
        }
        if (!stop && $patternProperties !== undefined) {
            const keywordLocation = `${schemaLocation}/patternProperties`;
            for (const pattern in $patternProperties) {
                const regex = new RegExp(pattern);
                const subSchema = $patternProperties[pattern];
                for (const key in instance) {
                    if (!regex.test(key)) {
                        continue;
                    }
                    const subInstancePointer = `${instanceLocation}/${encodePointer(key)}`;
                    const result = validate(instance[key], subSchema, draft, lookup, shortCircuit, recursiveAnchor, subInstancePointer, `${keywordLocation}/${encodePointer(pattern)}`);
                    if (result.valid) {
                        evaluated[key] = thisEvaluated[key] = true;
                    }
                    else {
                        stop = shortCircuit;
                        errors.push({
                            instanceLocation,
                            keyword: 'patternProperties',
                            keywordLocation,
                            error: `Property "${key}" matches pattern "${pattern}" but does not match associated schema.`
                        }, ...result.errors);
                    }
                }
            }
        }
        if (!stop && $additionalProperties !== undefined) {
            const keywordLocation = `${schemaLocation}/additionalProperties`;
            for (const key in instance) {
                if (thisEvaluated[key]) {
                    continue;
                }
                const subInstancePointer = `${instanceLocation}/${encodePointer(key)}`;
                const result = validate(instance[key], $additionalProperties, draft, lookup, shortCircuit, recursiveAnchor, subInstancePointer, keywordLocation);
                if (result.valid) {
                    evaluated[key] = true;
                }
                else {
                    stop = shortCircuit;
                    errors.push({
                        instanceLocation,
                        keyword: 'additionalProperties',
                        keywordLocation,
                        error: `Property "${key}" does not match additional properties schema.`
                    }, ...result.errors);
                }
            }
        }
        else if (!stop && $unevaluatedProperties !== undefined) {
            const keywordLocation = `${schemaLocation}/unevaluatedProperties`;
            for (const key in instance) {
                if (!evaluated[key]) {
                    const subInstancePointer = `${instanceLocation}/${encodePointer(key)}`;
                    const result = validate(instance[key], $unevaluatedProperties, draft, lookup, shortCircuit, recursiveAnchor, subInstancePointer, keywordLocation);
                    if (result.valid) {
                        evaluated[key] = true;
                    }
                    else {
                        errors.push({
                            instanceLocation,
                            keyword: 'unevaluatedProperties',
                            keywordLocation,
                            error: `Property "${key}" does not match unevaluated properties schema.`
                        }, ...result.errors);
                    }
                }
            }
        }
    }
    else if (instanceType === 'array') {
        if ($maxItems !== undefined && instance.length > $maxItems) {
            errors.push({
                instanceLocation,
                keyword: 'maxItems',
                keywordLocation: `${schemaLocation}/maxItems`,
                error: `Array has too many items (${instance.length} > ${$maxItems}).`
            });
        }
        if ($minItems !== undefined && instance.length < $minItems) {
            errors.push({
                instanceLocation,
                keyword: 'minItems',
                keywordLocation: `${schemaLocation}/minItems`,
                error: `Array has too few items (${instance.length} < ${$minItems}).`
            });
        }
        const length = instance.length;
        let i = 0;
        let stop = false;
        if ($prefixItems !== undefined) {
            const keywordLocation = `${schemaLocation}/prefixItems`;
            const length2 = Math.min($prefixItems.length, length);
            for (; i < length2; i++) {
                const result = validate(instance[i], $prefixItems[i], draft, lookup, shortCircuit, recursiveAnchor, `${instanceLocation}/${i}`, `${keywordLocation}/${i}`);
                evaluated[i] = true;
                if (!result.valid) {
                    stop = shortCircuit;
                    errors.push({
                        instanceLocation,
                        keyword: 'prefixItems',
                        keywordLocation,
                        error: `Items did not match schema.`
                    }, ...result.errors);
                    if (stop)
                        break;
                }
            }
        }
        if ($items !== undefined) {
            const keywordLocation = `${schemaLocation}/items`;
            if (Array.isArray($items)) {
                const length2 = Math.min($items.length, length);
                for (; i < length2; i++) {
                    const result = validate(instance[i], $items[i], draft, lookup, shortCircuit, recursiveAnchor, `${instanceLocation}/${i}`, `${keywordLocation}/${i}`);
                    evaluated[i] = true;
                    if (!result.valid) {
                        stop = shortCircuit;
                        errors.push({
                            instanceLocation,
                            keyword: 'items',
                            keywordLocation,
                            error: `Items did not match schema.`
                        }, ...result.errors);
                        if (stop)
                            break;
                    }
                }
            }
            else {
                for (; i < length; i++) {
                    const result = validate(instance[i], $items, draft, lookup, shortCircuit, recursiveAnchor, `${instanceLocation}/${i}`, keywordLocation);
                    evaluated[i] = true;
                    if (!result.valid) {
                        stop = shortCircuit;
                        errors.push({
                            instanceLocation,
                            keyword: 'items',
                            keywordLocation,
                            error: `Items did not match schema.`
                        }, ...result.errors);
                        if (stop)
                            break;
                    }
                }
            }
            if (!stop && $additionalItems !== undefined) {
                const keywordLocation = `${schemaLocation}/additionalItems`;
                for (; i < length; i++) {
                    const result = validate(instance[i], $additionalItems, draft, lookup, shortCircuit, recursiveAnchor, `${instanceLocation}/${i}`, keywordLocation);
                    evaluated[i] = true;
                    if (!result.valid) {
                        stop = shortCircuit;
                        errors.push({
                            instanceLocation,
                            keyword: 'additionalItems',
                            keywordLocation,
                            error: `Items did not match additional items schema.`
                        }, ...result.errors);
                    }
                }
            }
        }
        if ($contains !== undefined) {
            if (length === 0 && $minContains === undefined) {
                errors.push({
                    instanceLocation,
                    keyword: 'contains',
                    keywordLocation: `${schemaLocation}/contains`,
                    error: `Array is empty. It must contain at least one item matching the schema.`
                });
            }
            else if ($minContains !== undefined && length < $minContains) {
                errors.push({
                    instanceLocation,
                    keyword: 'minContains',
                    keywordLocation: `${schemaLocation}/minContains`,
                    error: `Array has less items (${length}) than minContains (${$minContains}).`
                });
            }
            else {
                const keywordLocation = `${schemaLocation}/contains`;
                const errorsLength = errors.length;
                let contained = 0;
                for (let j = 0; j < length; j++) {
                    const result = validate(instance[j], $contains, draft, lookup, shortCircuit, recursiveAnchor, `${instanceLocation}/${j}`, keywordLocation);
                    if (result.valid) {
                        evaluated[j] = true;
                        contained++;
                    }
                    else {
                        errors.push(...result.errors);
                    }
                }
                if (contained >= ($minContains || 0)) {
                    errors.length = errorsLength;
                }
                if ($minContains === undefined &&
                    $maxContains === undefined &&
                    contained === 0) {
                    errors.splice(errorsLength, 0, {
                        instanceLocation,
                        keyword: 'contains',
                        keywordLocation,
                        error: `Array does not contain item matching schema.`
                    });
                }
                else if ($minContains !== undefined && contained < $minContains) {
                    errors.push({
                        instanceLocation,
                        keyword: 'minContains',
                        keywordLocation: `${schemaLocation}/minContains`,
                        error: `Array must contain at least ${$minContains} items matching schema. Only ${contained} items were found.`
                    });
                }
                else if ($maxContains !== undefined && contained > $maxContains) {
                    errors.push({
                        instanceLocation,
                        keyword: 'maxContains',
                        keywordLocation: `${schemaLocation}/maxContains`,
                        error: `Array may contain at most ${$maxContains} items matching schema. ${contained} items were found.`
                    });
                }
            }
        }
        if (!stop && $unevaluatedItems !== undefined) {
            const keywordLocation = `${schemaLocation}/unevaluatedItems`;
            for (i; i < length; i++) {
                if (evaluated[i]) {
                    continue;
                }
                const result = validate(instance[i], $unevaluatedItems, draft, lookup, shortCircuit, recursiveAnchor, `${instanceLocation}/${i}`, keywordLocation);
                evaluated[i] = true;
                if (!result.valid) {
                    errors.push({
                        instanceLocation,
                        keyword: 'unevaluatedItems',
                        keywordLocation,
                        error: `Items did not match unevaluated items schema.`
                    }, ...result.errors);
                }
            }
        }
        if ($uniqueItems) {
            for (let j = 0; j < length; j++) {
                const a = instance[j];
                const ao = typeof a === 'object' && a !== null;
                for (let k = 0; k < length; k++) {
                    if (j === k) {
                        continue;
                    }
                    const b = instance[k];
                    const bo = typeof b === 'object' && b !== null;
                    if (a === b || (ao && bo && deepCompareStrict(a, b))) {
                        errors.push({
                            instanceLocation,
                            keyword: 'uniqueItems',
                            keywordLocation: `${schemaLocation}/uniqueItems`,
                            error: `Duplicate items at indexes ${j} and ${k}.`
                        });
                        j = Number.MAX_SAFE_INTEGER;
                        k = Number.MAX_SAFE_INTEGER;
                    }
                }
            }
        }
    }
    else if (instanceType === 'number') {
        if (draft === '4') {
            if ($minimum !== undefined &&
                (($exclusiveMinimum === true && instance <= $minimum) ||
                    instance < $minimum)) {
                errors.push({
                    instanceLocation,
                    keyword: 'minimum',
                    keywordLocation: `${schemaLocation}/minimum`,
                    error: `${instance} is less than ${$exclusiveMinimum ? 'or equal to ' : ''} ${$minimum}.`
                });
            }
            if ($maximum !== undefined &&
                (($exclusiveMaximum === true && instance >= $maximum) ||
                    instance > $maximum)) {
                errors.push({
                    instanceLocation,
                    keyword: 'maximum',
                    keywordLocation: `${schemaLocation}/maximum`,
                    error: `${instance} is greater than ${$exclusiveMaximum ? 'or equal to ' : ''} ${$maximum}.`
                });
            }
        }
        else {
            if ($minimum !== undefined && instance < $minimum) {
                errors.push({
                    instanceLocation,
                    keyword: 'minimum',
                    keywordLocation: `${schemaLocation}/minimum`,
                    error: `${instance} is less than ${$minimum}.`
                });
            }
            if ($maximum !== undefined && instance > $maximum) {
                errors.push({
                    instanceLocation,
                    keyword: 'maximum',
                    keywordLocation: `${schemaLocation}/maximum`,
                    error: `${instance} is greater than ${$maximum}.`
                });
            }
            if ($exclusiveMinimum !== undefined && instance <= $exclusiveMinimum) {
                errors.push({
                    instanceLocation,
                    keyword: 'exclusiveMinimum',
                    keywordLocation: `${schemaLocation}/exclusiveMinimum`,
                    error: `${instance} is less than ${$exclusiveMinimum}.`
                });
            }
            if ($exclusiveMaximum !== undefined && instance >= $exclusiveMaximum) {
                errors.push({
                    instanceLocation,
                    keyword: 'exclusiveMaximum',
                    keywordLocation: `${schemaLocation}/exclusiveMaximum`,
                    error: `${instance} is greater than or equal to ${$exclusiveMaximum}.`
                });
            }
        }
        if ($multipleOf !== undefined) {
            const remainder = instance % $multipleOf;
            if (Math.abs(0 - remainder) >= 1.1920929e-7 &&
                Math.abs($multipleOf - remainder) >= 1.1920929e-7) {
                errors.push({
                    instanceLocation,
                    keyword: 'multipleOf',
                    keywordLocation: `${schemaLocation}/multipleOf`,
                    error: `${instance} is not a multiple of ${$multipleOf}.`
                });
            }
        }
    }
    else if (instanceType === 'string') {
        const length = $minLength === undefined && $maxLength === undefined
            ? 0
            : ucs2length(instance);
        if ($minLength !== undefined && length < $minLength) {
            errors.push({
                instanceLocation,
                keyword: 'minLength',
                keywordLocation: `${schemaLocation}/minLength`,
                error: `String is too short (${length} < ${$minLength}).`
            });
        }
        if ($maxLength !== undefined && length > $maxLength) {
            errors.push({
                instanceLocation,
                keyword: 'maxLength',
                keywordLocation: `${schemaLocation}/maxLength`,
                error: `String is too long (${length} > ${$maxLength}).`
            });
        }
        if ($pattern !== undefined && !new RegExp($pattern).test(instance)) {
            errors.push({
                instanceLocation,
                keyword: 'pattern',
                keywordLocation: `${schemaLocation}/pattern`,
                error: `String does not match pattern.`
            });
        }
        if ($format !== undefined &&
            fastFormat[$format] &&
            !fastFormat[$format](instance)) {
            errors.push({
                instanceLocation,
                keyword: 'format',
                keywordLocation: `${schemaLocation}/format`,
                error: `String does not match format "${$format}".`
            });
        }
    }
    return { valid: errors.length === 0, errors };
}

class Validator {
    constructor(schema, draft = '2019-09', shortCircuit = true) {
        this.schema = schema;
        this.draft = draft;
        this.shortCircuit = shortCircuit;
        this.lookup = dereference(schema);
    }
    validate(instance) {
        return validate(instance, this.schema, this.draft, this.lookup, this.shortCircuit);
    }
    addSchema(schema, id) {
        if (id) {
            schema = { ...schema, $id: id };
        }
        dereference(schema, this.lookup);
    }
}

this.Validator = Validator;
this.deepCompareStrict = deepCompareStrict;
this.dereference = dereference;
this.encodePointer = encodePointer;
this.escapePointer = escapePointer;
this.fastFormat = fastFormat;
this.fullFormat = fullFormat;
this.ignoredKeyword = ignoredKeyword;
this.initialBaseURI = initialBaseURI;
this.schemaArrayKeyword = schemaArrayKeyword;
this.schemaKeyword = schemaKeyword;
this.schemaMapKeyword = schemaMapKeyword;
this.ucs2length = ucs2length;
this.validate = validate;
