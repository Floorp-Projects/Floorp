// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[derive(Debug)]
pub struct StaticTableEntry {
    index: u64,
    name: &'static [u8],
    value: &'static [u8],
}

impl StaticTableEntry {
    pub fn name(&self) -> &[u8] {
        self.name
    }

    pub fn value(&self) -> &[u8] {
        self.value
    }

    pub fn index(&self) -> u64 {
        self.index
    }
}

macro_rules! static_table_entries {
    [$($i:expr, $n:expr, $v:expr);+ $(;)?] => {
        &[ $(StaticTableEntry { index: $i, name: $n, value: $v }),+ ]
    };
}

pub const HEADER_STATIC_TABLE: &[StaticTableEntry] = static_table_entries![
    0, b":authority", b"";
    1, b":path", b"/";
    2, b"age", b"0";
    3, b"content-disposition", b"";
    4, b"content-length", b"0";
    5, b"cookie", b"";
    6, b"date", b"";
    7, b"etag", b"";
    8, b"if-modified-since", b"";
    9, b"if-none-match", b"";
    10, b"last-modified", b"";
    11, b"link", b"";
    12, b"location", b"";
    13, b"referer", b"";
    14, b"set-cookie", b"";
    15, b":method", b"CONNECT";
    16, b":method", b"DELETE";
    17, b":method", b"GET";
    18, b":method", b"HEAD";
    19, b":method", b"OPTIONS";
    20, b":method", b"POST";
    21, b":method", b"PUT";
    22, b":scheme", b"http";
    23, b":scheme", b"https";
    24, b":status", b"103";
    25, b":status", b"200";
    26, b":status", b"304";
    27, b":status", b"404";
    28, b":status", b"503";
    29, b"accept", b"*/*";
    30, b"accept", b"application/dns-message";
    31, b"accept-encoding", b"gzip, deflate, br";
    32, b"accept-ranges", b"bytes";
    33, b"access-control-allow-headers", b"cache-control";
    34, b"access-control-allow-headers", b"content-type";
    35, b"access-control-allow-origin", b"*";
    36, b"cache-control", b"max-age=0";
    37, b"cache-control", b"max-age=2592000";
    38, b"cache-control", b"max-age=604800";
    39, b"cache-control", b"no-cache";
    40, b"cache-control", b"no-store";
    41, b"cache-control", b"public, max-age=31536000";
    42, b"content-encoding", b"br";
    43, b"content-encoding", b"gzip";
    44, b"content-type", b"application/dns-message";
    45, b"content-type", b"application/javascript";
    46, b"content-type", b"application/json";
    47, b"content-type", b"application/x-www-form-urlencoded";
    48, b"content-type", b"image/gif";
    49, b"content-type", b"image/jpeg";
    50, b"content-type", b"image/png";
    51, b"content-type", b"text/css";
    52, b"content-type", b"text/html; charset=utf-8";
    53, b"content-type", b"text/plain";
    54, b"content-type", b"text/plain;charset=utf-8";
    55, b"range", b"bytes=0-";
    56, b"strict-transport-security", b"max-age=31536000";
    57, b"strict-transport-security", b"max-age=31536000; includesubdomains";
    58, b"strict-transport-security", b"max-age=31536000; includesubdomains; preload";
    59, b"vary", b"accept-encoding";
    60, b"vary", b"origin";
    61, b"x-content-type-options", b"nosniff";
    62, b"x-xss-protection", b"1; mode=block";
    63, b":status", b"100";
    64, b":status", b"204";
    65, b":status", b"206";
    66, b":status", b"302";
    67, b":status", b"400";
    68, b":status", b"403";
    69, b":status", b"421";
    70, b":status", b"425";
    71, b":status", b"500";
    72, b"accept-language", b"";
    73, b"access-control-allow-credentials", b"FALSE";
    74, b"access-control-allow-credentials", b"TRUE";
    75, b"access-control-allow-headers", b"*";
    76, b"access-control-allow-methods", b"get";
    77, b"access-control-allow-methods", b"get, post, options";
    78, b"access-control-allow-methods", b"options";
    79, b"access-control-expose-headers", b"content-length";
    80, b"access-control-request-headers", b"content-type";
    81, b"access-control-request-method", b"get";
    82, b"access-control-request-method", b"post";
    83, b"alt-svc", b"clear";
    84, b"authorization", b"";
    85, b"content-security-policy", b"script-src 'none'; object-src 'none'; base-uri 'none'";
    86, b"early-data", b"1";
    87, b"expect-ct", b"";
    88, b"forwarded", b"";
    89, b"if-range", b"";
    90, b"origin", b"";
    91, b"purpose", b"prefetch";
    92, b"server", b"";
    93, b"timing-allow-origin", b"*";
    94, b"upgrade-insecure-requests", b"1";
    95, b"user-agent", b"";
    96, b"x-forwarded-for", b"";
    97, b"x-frame-options", b"deny";
    98, b"x-frame-options", b"sameorigin";
];
