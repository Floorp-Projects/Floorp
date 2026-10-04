// SPDX-License-Identifier: MPL-2.0

#include <errno.h>
#include <inttypes.h>
#include <libproc.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

static int print_probe_session(void) {
  const pid_t pid = getpid();
  rusage_info_current usage;
  memset(&usage, 0, sizeof(usage));
  if (proc_pid_rusage(pid, RUSAGE_INFO_CURRENT, (rusage_info_t *)&usage) != 0 ||
      usage.ri_proc_start_abstime == 0) {
    (void)fprintf(stderr, "proc_pid_rusage failed: %s\n", strerror(errno));
    return 1;
  }

  const pid_t pgid = getpgid(pid);
  const pid_t sid = getsid(pid);
  if (pgid == -1 || sid == -1) {
    (void)fprintf(stderr, "session lookup failed: %s\n", strerror(errno));
    return 1;
  }

  (void)printf(
      "{\"pid\":%d,\"pgid\":%d,\"sid\":%d,\"start_abstime\":\"%" PRIu64
      "\",\"process_generation\":\"pid-%d-generation-%" PRIu64
      "\",\"descendant_ownership\":\"not-established\",\"event_stream\":\"incomplete\"}\n",
      pid, pgid, sid, usage.ri_proc_start_abstime, pid, usage.ri_proc_start_abstime);
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc == 2 && strcmp(argv[1], "probe-session") == 0) {
    return print_probe_session();
  }
  {
    (void)fprintf(stderr, "usage: %s probe-session\n", argv[0]);
    return 64;
  }
}
