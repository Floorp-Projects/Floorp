// SPDX-License-Identifier: MPL-2.0

#include <errno.h>
#include <inttypes.h>
#include <libproc.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static int print_probe_session(void) {
  const pid_t pid = getpid();
  const pid_t original_pgid = getpgid(pid);
  const pid_t original_sid = getsid(pid);
  if (original_pgid == -1 || original_sid == -1) {
    (void)fprintf(stderr, "session lookup failed: %s\n", strerror(errno));
    return 1;
  }
  if (original_pgid != pid || original_sid != pid) {
    if (setsid() == -1) {
      (void)fprintf(stderr, "setsid failed: %s\n", strerror(errno));
      return 1;
    }
  }

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

static int launch_in_private_session(char *const command[]) {
  const pid_t child = fork();
  if (child == -1) {
    (void)fprintf(stderr, "fork failed: %s\n", strerror(errno));
    return 1;
  }
  if (child == 0) {
    if (setsid() == -1) {
      (void)fprintf(stderr, "setsid failed: %s\n", strerror(errno));
      _exit(1);
    }
    execvp(command[0], command);
    (void)fprintf(stderr, "execvp failed: %s\n", strerror(errno));
    _exit(127);
  }

  int status = 0;
  if (waitpid(child, &status, 0) == -1) {
    (void)fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
    return 1;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return 1;
}

static int exec_in_private_process_group(char *const command[]) {
  const pid_t pid = getpid();
  if (setpgid(0, 0) == -1) {
    (void)fprintf(stderr, "setpgid failed: %s\n", strerror(errno));
    return 1;
  }
  if (getpgid(pid) != pid) {
    (void)fprintf(stderr, "exclusive process group was not established\n");
    return 1;
  }
  execvp(command[0], command);
  (void)fprintf(stderr, "execvp failed: %s\n", strerror(errno));
  return 127;
}

int main(int argc, char *argv[]) {
  if (argc == 2 && strcmp(argv[1], "probe-session") == 0) {
    return print_probe_session();
  }
  if (argc >= 4 && strcmp(argv[1], "launch") == 0 &&
      strcmp(argv[2], "--") == 0) {
    return launch_in_private_session(&argv[3]);
  }
  if (argc >= 4 && strcmp(argv[1], "exec") == 0 &&
      strcmp(argv[2], "--") == 0) {
    return exec_in_private_process_group(&argv[3]);
  }
  {
    (void)fprintf(stderr, "usage: %s probe-session | launch -- command [args...] | exec -- command [args...]\n", argv[0]);
    return 64;
  }
}
