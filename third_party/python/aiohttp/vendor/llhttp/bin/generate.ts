#!/usr/bin/env -S npx ts-node
import * as fs from 'fs';
import { LLParse } from 'llparse';
import * as path from 'path';
import * as semver from 'semver';

import * as llhttp from '../src/llhttp';

const pkgFile = path.join(__dirname, '..', 'package.json');
const pkg = JSON.parse(fs.readFileSync(pkgFile).toString());

const BUILD_DIR = path.join(__dirname, '..', 'build');
const C_DIR = path.join(BUILD_DIR, 'c');
const SRC_DIR = path.join(__dirname, '..', 'src');

const C_FILE = path.join(C_DIR, 'llhttp.c');
const HEADER_FILE = path.join(BUILD_DIR, 'llhttp.h');

for (const dir of [ BUILD_DIR, C_DIR ]) {
  try {
    fs.mkdirSync(dir);
  } catch (e) {
    // no-op
  }
}

function build(mode: 'strict' | 'loose') {
  const llparse = new LLParse('llhttp__internal');
  const instance = new llhttp.HTTP(llparse, mode);

  return llparse.build(instance.build().entry, {
    c: {
      header: 'llhttp',
    },
    debug: process.env.LLPARSE_DEBUG ? 'llhttp__debug' : undefined,
    headerGuard: 'INCLUDE_LLHTTP_ITSELF_H_',
  });
}

function guard(strict: string, loose: string): string {
  let out = '';

  if (strict === loose) {
    return strict;
  }

  out += '#if LLHTTP_STRICT_MODE\n';
  out += '\n';
  out += strict + '\n';
  out += '\n';
  out += '#else  /* !LLHTTP_STRICT_MODE */\n';
  out += '\n';
  out += loose + '\n';
  out += '\n';
  out += '#endif  /* LLHTTP_STRICT_MODE */\n';

  return out;
}

const artifacts = {
  loose: build('loose'),
  strict: build('strict'),
};

let headers = '';

headers += '#ifndef INCLUDE_LLHTTP_H_\n';
headers += '#define INCLUDE_LLHTTP_H_\n';

headers += '\n';

const version = semver.parse(pkg.version)!;

headers += `#define LLHTTP_VERSION_MAJOR ${version.major}\n`;
headers += `#define LLHTTP_VERSION_MINOR ${version.minor}\n`;
headers += `#define LLHTTP_VERSION_PATCH ${version.patch}\n`;
headers += '\n';

headers += '#ifndef LLHTTP_STRICT_MODE\n';
headers += '# define LLHTTP_STRICT_MODE 0\n';
headers += '#endif\n';
headers += '\n';

const cHeaders = new llhttp.CHeaders();

headers += guard(artifacts.strict.header, artifacts.loose.header);

headers += '\n';

headers += cHeaders.build();

headers += '\n';

headers += fs.readFileSync(path.join(SRC_DIR, 'native', 'api.h'));

headers += '\n';
headers += '#endif  /* INCLUDE_LLHTTP_H_ */\n';

fs.writeFileSync(C_FILE,
  guard(artifacts.strict.c || '', artifacts.loose.c || ''));
fs.writeFileSync(HEADER_FILE, headers);
