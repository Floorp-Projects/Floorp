/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Archive code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include "mar.h"

#ifdef XP_WIN
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

static void print_usage() {
    printf("usage: mar [-C dir] {-c|-x|-t} archive.mar [files...]\n");
}

static int mar_test_callback(MarFile *mar, const MarItem *item, void *unused) {
  printf("%u\t0%o\t%s\n", item->length, item->flags, item->name);
  return 0;
}

static int mar_test(const char *path) {
  MarFile *mar;

  mar = mar_open(path);
  if (!mar)
    return -1;

  printf("SIZE\tMODE\tNAME\n");
  mar_enum_items(mar, mar_test_callback, NULL);

  mar_close(mar);
  return 0;
}

int main(int argc, char **argv) {
  int command;

  if (argc < 3) {
    print_usage();
    return -1;
  }

  if (argv[1][1] == 'C') {
    chdir(argv[2]);
    argv += 2;
    argc -= 2;
  }

  switch (argv[1][1]) {
  case 'c':
    return mar_create(argv[2], argc - 3, argv + 3);
  case 't':
    return mar_test(argv[2]);
  case 'x':
    return mar_extract(argv[2]);
  default:
    print_usage();
    return -1;
  }

  return 0;
}
