/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plgetopt.h>
#include "XRemoteClient.h"

static void print_usage(void);

int main(int argc, char **argv)
{
  nsresult rv;
  XRemoteClient client;
  char *browser = 0;
  char *profile = 0;
  char *username = 0;
  char *command = 0;

  if (argc < 2) {
    print_usage();
    return 4;
  }

  PLOptStatus os;
  PLOptState *opt = PL_CreateOptState(argc, argv, "ha:u:p:");
  while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
    if (PL_OPT_BAD == os) {
      print_usage();
      return 4;
    }

    switch (opt->option) {
    case 'h':
      print_usage();
      return 4;
      break;
    case 'u':
      username = strdup(opt->value);
      break;
    case 'a':
      browser = strdup(opt->value);
      break;
    case 'p':
      profile = strdup(opt->value);
      break;
    case 0:
      command = strdup(opt->value);      
    default:
      break;
    }
  }

  rv = client.Init();
  // failed to connect to the X server
  if (NS_FAILED(rv))
    return 1;

  // send the command - it doesn't get any easier than this
  bool success = false;
  char *error = 0;
  rv = client.SendCommand(browser, username, profile, command, nullptr,
                          &error, &success);

  // failed to send command
  if (NS_FAILED(rv)) {
    fprintf(stderr, "%s: Error: Failed to send command: ", argv[0]);
    if (error) {
      fprintf(stderr, "%s\n", error);
      free(error);
    }
    else {
      fprintf(stderr, "No error string reported..\n");
    }

    return 3;
  }

  // no running window found
  if (!success) {
    fprintf(stderr, "%s: Error: Failed to find a running server.\n", argv[0]);
    return 2;
  }

  // else, everything is fine.
  return 0;
}

/* static */
void print_usage(void) {
  fprintf(stderr, "Usage: mozilla-xremote-client [-a firefox|thunderbird|mozilla|any]\n");
  fprintf(stderr, "                              [-u <username>]\n");
  fprintf(stderr, "                              [-p <profile>] COMMAND\n");
}
