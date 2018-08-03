/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

(function(){

const testNames = [
    "3d-cube",
    "3d-morph",
    "3d-raytrace",
    "access-binary-trees",
    "access-fannkuch",
    "access-nbody",
    "access-nsieve",
    "bitops-3bit-bits-in-byte",
    "bitops-bits-in-byte",
    "bitops-bitwise-and",
    "bitops-nsieve-bits",
    "controlflow-recursive",
    "crypto-aes",
    "crypto-md5",
    "crypto-sha1",
    "date-format-tofte",
    "date-format-xparb",
    "math-cordic",
    "math-partial-sums",
    "math-spectral-norm",
    "regexp-dna",
    "string-base64",
    "string-fasta",
    "string-tagcloud",
    "string-unpack-code",
    "string-validate-input"
];

for (let testName of testNames) {
    let testScript = testName + ".js";
    let startTime = new Date;
    if (testScript.indexOf('parse-only') >= 0)
        checkSyntax(testScript);
    else
        load(testScript);
    let duration = new Date() - startTime;
    gc();

    print(testName+":", duration);
}

})();

