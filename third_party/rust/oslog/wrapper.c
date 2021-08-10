#include <os/log.h>

os_log_t wrapped_get_default_log() {
    return OS_LOG_DEFAULT;
}

void wrapped_os_log_with_type(os_log_t log, os_log_type_t type, const char* message) {
    os_log_with_type(log, type, "%{public}s", message);
}

void wrapped_os_log_debug(os_log_t log, const char* message) {
    os_log_debug(log, "%{public}s", message);
}

void wrapped_os_log_info(os_log_t log, const char* message) {
    os_log_info(log, "%{public}s", message);
}

void wrapped_os_log_default(os_log_t log, const char* message) {
    os_log(log, "%{public}s", message);
}

void wrapped_os_log_error(os_log_t log, const char* message) {
    os_log_error(log, "%{public}s", message);
}

void wrapped_os_log_fault(os_log_t log, const char* message) {
    os_log_fault(log, "%{public}s", message);
}