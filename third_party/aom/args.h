/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef ARGS_H_
#define ARGS_H_
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct arg {
  char **argv;
  const char *name;
  const char *val;
  unsigned int argv_step;
  const struct arg_def *def;
};

struct arg_enum_list {
  const char *name;
  int val;
};
#define ARG_ENUM_LIST_END \
  { 0 }

typedef struct arg_def {
  const char *short_name;
  const char *long_name;
  int has_val;
  const char *desc;
  const struct arg_enum_list *enums;
} arg_def_t;
#define ARG_DEF(s, l, v, d) \
  { s, l, v, d, NULL }
#define ARG_DEF_ENUM(s, l, v, d, e) \
  { s, l, v, d, e }
#define ARG_DEF_LIST_END \
  { 0 }

struct arg arg_init(char **argv);
int arg_match(struct arg *arg_, const struct arg_def *def, char **argv);
const char *arg_next(struct arg *arg);
void arg_show_usage(FILE *fp, const struct arg_def *const *defs);
char **argv_dup(int argc, const char **argv);

unsigned int arg_parse_uint(const struct arg *arg);
int arg_parse_int(const struct arg *arg);
struct aom_rational arg_parse_rational(const struct arg *arg);
int arg_parse_enum(const struct arg *arg);
int arg_parse_enum_or_int(const struct arg *arg);
int arg_parse_list(const struct arg *arg, int *list, int n);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARGS_H_
