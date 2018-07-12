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

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "args.h"

#include "aom/aom_integer.h"
#include "aom_ports/msvc.h"

#if defined(__GNUC__) && __GNUC__
extern void die(const char *fmt, ...) __attribute__((noreturn));
#else
extern void die(const char *fmt, ...);
#endif

struct arg arg_init(char **argv) {
  struct arg a;

  a.argv = argv;
  a.argv_step = 1;
  a.name = NULL;
  a.val = NULL;
  a.def = NULL;
  return a;
}

int arg_match(struct arg *arg_, const struct arg_def *def, char **argv) {
  struct arg arg;

  if (!argv[0] || argv[0][0] != '-') return 0;

  arg = arg_init(argv);

  if (def->short_name && strlen(arg.argv[0]) == strlen(def->short_name) + 1 &&
      !strcmp(arg.argv[0] + 1, def->short_name)) {
    arg.name = arg.argv[0] + 1;
    arg.val = def->has_val ? arg.argv[1] : NULL;
    arg.argv_step = def->has_val ? 2 : 1;
  } else if (def->long_name) {
    const size_t name_len = strlen(def->long_name);

    if (strlen(arg.argv[0]) >= name_len + 2 && arg.argv[0][1] == '-' &&
        !strncmp(arg.argv[0] + 2, def->long_name, name_len) &&
        (arg.argv[0][name_len + 2] == '=' ||
         arg.argv[0][name_len + 2] == '\0')) {
      arg.name = arg.argv[0] + 2;
      arg.val = arg.name[name_len] == '=' ? arg.name + name_len + 1 : NULL;
      arg.argv_step = 1;
    }
  }

  if (arg.name && !arg.val && def->has_val)
    die("Error: option %s requires argument.\n", arg.name);

  if (arg.name && arg.val && !def->has_val)
    die("Error: option %s requires no argument.\n", arg.name);

  if (arg.name && (arg.val || !def->has_val)) {
    arg.def = def;
    *arg_ = arg;
    return 1;
  }

  return 0;
}

const char *arg_next(struct arg *arg) {
  if (arg->argv[0]) arg->argv += arg->argv_step;

  return *arg->argv;
}

char **argv_dup(int argc, const char **argv) {
  char **new_argv = malloc((argc + 1) * sizeof(*argv));

  memcpy(new_argv, argv, argc * sizeof(*argv));
  new_argv[argc] = NULL;
  return new_argv;
}

void arg_show_usage(FILE *fp, const struct arg_def *const *defs) {
  char option_text[40] = { 0 };

  for (; *defs; defs++) {
    const struct arg_def *def = *defs;
    char *short_val = def->has_val ? " <arg>" : "";
    char *long_val = def->has_val ? "=<arg>" : "";

    if (def->short_name && def->long_name) {
      char *comma = def->has_val ? "," : ",      ";

      snprintf(option_text, 37, "-%s%s%s --%s%6s", def->short_name, short_val,
               comma, def->long_name, long_val);
    } else if (def->short_name)
      snprintf(option_text, 37, "-%s%s", def->short_name, short_val);
    else if (def->long_name)
      snprintf(option_text, 37, "          --%s%s", def->long_name, long_val);

    fprintf(fp, "  %-37s\t%s\n", option_text, def->desc);

    if (def->enums) {
      const struct arg_enum_list *listptr;

      fprintf(fp, "  %-37s\t  ", "");

      for (listptr = def->enums; listptr->name; listptr++)
        fprintf(fp, "%s%s", listptr->name, listptr[1].name ? ", " : "\n");
    }
  }
}

unsigned int arg_parse_uint(const struct arg *arg) {
  char *endptr;
  const unsigned long rawval = strtoul(arg->val, &endptr, 10);  // NOLINT

  if (arg->val[0] != '\0' && endptr[0] == '\0') {
    if (rawval <= UINT_MAX) return (unsigned int)rawval;

    die("Option %s: Value %lu out of range for unsigned int\n", arg->name,
        rawval);
  }

  die("Option %s: Invalid character '%c'\n", arg->name, *endptr);
  return 0;
}

int arg_parse_int(const struct arg *arg) {
  char *endptr;
  const long rawval = strtol(arg->val, &endptr, 10);  // NOLINT

  if (arg->val[0] != '\0' && endptr[0] == '\0') {
    if (rawval >= INT_MIN && rawval <= INT_MAX) return (int)rawval;

    die("Option %s: Value %ld out of range for signed int\n", arg->name,
        rawval);
  }

  die("Option %s: Invalid character '%c'\n", arg->name, *endptr);
  return 0;
}

struct aom_rational {
  int num; /**< fraction numerator */
  int den; /**< fraction denominator */
};
struct aom_rational arg_parse_rational(const struct arg *arg) {
  long int rawval;
  char *endptr;
  struct aom_rational rat;

  /* parse numerator */
  rawval = strtol(arg->val, &endptr, 10);

  if (arg->val[0] != '\0' && endptr[0] == '/') {
    if (rawval >= INT_MIN && rawval <= INT_MAX)
      rat.num = (int)rawval;
    else
      die("Option %s: Value %ld out of range for signed int\n", arg->name,
          rawval);
  } else
    die("Option %s: Expected / at '%c'\n", arg->name, *endptr);

  /* parse denominator */
  rawval = strtol(endptr + 1, &endptr, 10);

  if (arg->val[0] != '\0' && endptr[0] == '\0') {
    if (rawval >= INT_MIN && rawval <= INT_MAX)
      rat.den = (int)rawval;
    else
      die("Option %s: Value %ld out of range for signed int\n", arg->name,
          rawval);
  } else
    die("Option %s: Invalid character '%c'\n", arg->name, *endptr);

  return rat;
}

int arg_parse_enum(const struct arg *arg) {
  const struct arg_enum_list *listptr;
  long int rawval;
  char *endptr;

  /* First see if the value can be parsed as a raw value */
  rawval = strtol(arg->val, &endptr, 10);
  if (arg->val[0] != '\0' && endptr[0] == '\0') {
    /* Got a raw value, make sure it's valid */
    for (listptr = arg->def->enums; listptr->name; listptr++)
      if (listptr->val == rawval) return (int)rawval;
  }

  /* Next see if it can be parsed as a string */
  for (listptr = arg->def->enums; listptr->name; listptr++)
    if (!strcmp(arg->val, listptr->name)) return listptr->val;

  die("Option %s: Invalid value '%s'\n", arg->name, arg->val);
  return 0;
}

int arg_parse_enum_or_int(const struct arg *arg) {
  if (arg->def->enums) return arg_parse_enum(arg);
  return arg_parse_int(arg);
}

// parse a comma separated list of at most n integers
// return the number of elements in the list
int arg_parse_list(const struct arg *arg, int *list, int n) {
  const char *ptr = arg->val;
  char *endptr;
  int i = 0;

  while (ptr[0] != '\0') {
    int32_t rawval = (int32_t)strtol(ptr, &endptr, 10);
    if (rawval < INT_MIN || rawval > INT_MAX) {
      die("Option %s: Value %ld out of range for signed int\n", arg->name,
          rawval);
    } else if (i >= n) {
      die("Option %s: List has more than %d entries\n", arg->name, n);
    } else if (*endptr == ',') {
      endptr++;
    } else if (*endptr != '\0') {
      die("Option %s: Bad list separator '%c'\n", arg->name, *endptr);
    }
    list[i++] = (int)rawval;
    ptr = endptr;
  }
  return i;
}
