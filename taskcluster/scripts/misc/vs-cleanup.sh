case "$(uname -s)" in
MINGW*)
  # For some reason, by the time the task finishes, and when run-task
  # starts its cleanup, there is still a vctip.exe (MSVC telemetry-related
  # process) running and using a dll that run-task can't then delete.
  # "For some reason", because the same doesn't happen with other tasks.
  # In fact, this used to happen with older versions of MSVC for other
  # tasks, and stopped when upgrading to 15.8.4...
  taskkill -f -im vctip.exe || true
  # Same with the mspdbsrv process.
  taskkill -f -im mspdbsrv.exe || true
  ;;
esac
