/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/eqrion/cbindgen` and use a tagged release
 *   2. Run `make cbindgen`
 */



#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * A recoverable error.
 */
#define UPLOAD_RESULT_RECOVERABLE 1

/**
 * An unrecoverable error.
 */
#define UPLOAD_RESULT_UNRECOVERABLE 2

/**
 * A HTTP response code.
 *
 * The actual response code is encoded in the lower bits.
 */
#define UPLOAD_RESULT_HTTP_STATUS 32768

/**
 * The supported metrics' lifetimes.
 *
 * A metric's lifetime determines when its stored data gets reset.
 */
enum Lifetime {
  /**
   * The metric is reset with each sent ping
   */
  Lifetime_Ping,
  /**
   * The metric is reset on application restart
   */
  Lifetime_Application,
  /**
   * The metric is reset with each user profile
   */
  Lifetime_User,
};
typedef int32_t Lifetime;

/**
 * Different resolutions supported by the memory related metric types (e.g.
 * MemoryDistributionMetric).
 */
enum MemoryUnit {
  /**
   * 1 byte
   */
  MemoryUnit_Byte,
  /**
   * 2^10 bytes
   */
  MemoryUnit_Kilobyte,
  /**
   * 2^20 bytes
   */
  MemoryUnit_Megabyte,
  /**
   * 2^30 bytes
   */
  MemoryUnit_Gigabyte,
};
typedef int32_t MemoryUnit;

/**
 * Different resolutions supported by the time related
 * metric types (e.g. DatetimeMetric).
 */
enum TimeUnit {
  /**
   * Truncate to nanosecond precision.
   */
  TimeUnit_Nanosecond,
  /**
   * Truncate to microsecond precision.
   */
  TimeUnit_Microsecond,
  /**
   * Truncate to millisecond precision.
   */
  TimeUnit_Millisecond,
  /**
   * Truncate to second precision.
   */
  TimeUnit_Second,
  /**
   * Truncate to minute precision.
   */
  TimeUnit_Minute,
  /**
   * Truncate to hour precision.
   */
  TimeUnit_Hour,
  /**
   * Truncate to day precision.
   */
  TimeUnit_Day,
};
typedef int32_t TimeUnit;

/**
 * `FfiStr<'a>` is a safe (`#[repr(transparent)]`) wrapper around a
 * nul-terminated `*const c_char` (e.g. a C string). Conceptually, it is
 * similar to [`std::ffi::CStr`], except that it may be used in the signatures
 * of extern "C" functions.
 *
 * Functions accepting strings should use this instead of accepting a C string
 * directly. This allows us to write those functions using safe code without
 * allowing safe Rust to cause memory unsafety.
 *
 * A single function for constructing these from Rust ([`FfiStr::from_raw`])
 * has been provided. Most of the time, this should not be necessary, and users
 * should accept `FfiStr` in the parameter list directly.
 *
 * ## Caveats
 *
 * An effort has been made to make this struct hard to misuse, however it is
 * still possible, if the `'static` lifetime is manually specified in the
 * struct. E.g.
 *
 * ```rust,no_run
 * # use ffi_support::FfiStr;
 * // NEVER DO THIS
 * #[no_mangle]
 * extern "C" fn never_do_this(s: FfiStr<'static>) {
 *     // save `s` somewhere, and access it after this
 *     // function returns.
 * }
 * ```
 *
 * Instead, one of the following patterns should be used:
 *
 * ```
 * # use ffi_support::FfiStr;
 * #[no_mangle]
 * extern "C" fn valid_use_1(s: FfiStr<'_>) {
 *     // Use of `s` after this function returns is impossible
 * }
 * // Alternative:
 * #[no_mangle]
 * extern "C" fn valid_use_2(s: FfiStr) {
 *     // Use of `s` after this function returns is impossible
 * }
 * ```
 */
typedef const char *FfiStr;

/**
 * Configuration over FFI.
 *
 * **CAUTION**: This must match _exactly_ the definition on the Kotlin side.
 * If this side is changed, the Kotlin side need to be changed, too.
 */
typedef struct FfiConfiguration {
  FfiStr data_dir;
  FfiStr package_name;
  FfiStr language_binding_name;
  uint8_t upload_enabled;
  const int32_t *max_events;
  uint8_t delay_ping_lifetime_io;
} FfiConfiguration;

typedef const char *const *RawStringArray;

/**
 * ByteBuffer is a struct that represents an array of bytes to be sent over the FFI boundaries.
 * There are several cases when you might want to use this, but the primary one for us
 * is for returning protobuf-encoded data to Swift and Java. The type is currently rather
 * limited (implementing almost no functionality), however in the future it may be
 * more expanded.
 *
 * ## Caveats
 *
 * Note that the order of the fields is `len` (an i32) then `data` (a `*mut u8`), getting
 * this wrong on the other side of the FFI will cause memory corruption and crashes.
 * `i32` is used for the length instead of `u64` and `usize` because JNA has interop
 * issues with both these types.
 *
 * ByteBuffer does not implement Drop. This is intentional. Memory passed into it will
 * be leaked if it is not explicitly destroyed by calling [`ByteBuffer::destroy`]. This
 * is because in the future, we may allow it's use for passing data into Rust code.
 * ByteBuffer assuming ownership of the data would make this a problem.
 *
 * ## Layout/fields
 *
 * This struct's field are not `pub` (mostly so that we can soundly implement `Send`, but also so
 * that we can verify Rust users are constructing them appropriately), the fields, their types, and
 * their order are *very much* a part of the public API of this type. Consumers on the other side
 * of the FFI will need to know its layout.
 *
 * If this were a C struct, it would look like
 *
 * ```c,no_run
 * struct ByteBuffer {
 *     int64_t len;
 *     uint8_t *data; // note: nullable
 * };
 * ```
 *
 * In Rust, there are two fields, in this order: `len: i32`, and `data: *mut u8`.
 *
 * ### Description of fields
 *
 * `data` is a pointer to an array of `len` bytes. Not that data can be a null pointer and therefore
 * should be checked.
 *
 * The bytes array is allocated on the heap and must be freed on it as well. Critically, if there
 * are multiple rust packages using being used in the same application, it *must be freed on the
 * same heap that allocated it*, or you will corrupt both heaps.
 */
typedef struct ByteBuffer {
  int32_t len;
  uint8_t *data;
} ByteBuffer;

/**
 * A FFI-compatible representation for the PingUploadTask.
 *
 * This is exposed as a C-compatible tagged union, like this:
 *
 * ```c
 * enum FfiPingUploadTask_Tag {
 *   FfiPingUploadTask_Upload,
 *   FfiPingUploadTask_Wait,
 *   FfiPingUploadTask_Done,
 * };
 * typedef uint8_t FfiPingUploadTask_Tag;
 *
 * typedef struct {
 *   FfiPingUploadTask_Tag tag;
 *   char *document_id;
 *   char *path;
 *   char *body;
 *   char *headers;
 * } FfiPingUploadTask_Upload_Body;
 *
 * typedef union {
 *   FfiPingUploadTask_Tag tag;
 *   FfiPingUploadTask_Upload_Body upload;
 * } FfiPingUploadTask;
 *
 * ```
 *
 * It is therefore always valid to read the `tag` field of the returned union (always the first
 * field in memory).
 *
 * Language bindings should turn this into proper language types (e.g. enums/structs) and
 * copy out data.
 *
 * String fields are encoded into null-terminated UTF-8 C strings.
 *
 * * The language binding should copy out the data and turn these into their equivalent string type.
 * * The language binding should _not_ free these fields individually.
 *   Instead `glean_process_ping_upload_response` will receive the whole enum, taking care of
 *   freeing the memory.
 *
 *
 * The order of variants should be the same as in `glean-core/src/upload/mod.rs`
 * and `glean-core/android/src/main/java/mozilla/telemetry/glean/net/Upload.kt`.
 *
 */
enum FfiPingUploadTask_Tag {
  FfiPingUploadTask_Upload,
  FfiPingUploadTask_Wait,
  FfiPingUploadTask_Done,
};
typedef uint8_t FfiPingUploadTask_Tag;

typedef struct FfiPingUploadTask_Upload_Body {
  FfiPingUploadTask_Tag tag;
  char *document_id;
  char *path;
  struct ByteBuffer body;
  char *headers;
} FfiPingUploadTask_Upload_Body;

typedef union FfiPingUploadTask {
  FfiPingUploadTask_Tag tag;
  FfiPingUploadTask_Upload_Body upload;
  struct {
    FfiPingUploadTask_Tag wait_tag;
    uint64_t wait;
  };
} FfiPingUploadTask;

typedef const int64_t *RawInt64Array;

typedef const int32_t *RawIntArray;

/**
 * Identifier for a running timer.
 */
typedef uint64_t TimerId;

/**
 * Initialize the logging system based on the target platform. This ensures
 * that logging is shown when executing the Glean SDK unit tests.
 */
void glean_enable_logging(void);

/**
 * Initialize the logging system to send JSON messages to a file descriptor
 * (Unix) or file handle (Windows).
 *
 * Not available on Android and iOS.
 *
 * `fd` is a writable file descriptor (on Unix) or file handle (on Windows).
 *
 * # Safety
 * Unsafe because the fd u64 passed in will be interpreted as either a file
 * descriptor (Unix) or file handle (Windows) without any checking.
 */
void glean_enable_logging_to_fd(uint64_t fd);

/**
 * # Safety
 *
 * A valid and non-null configuration object is required for this function.
 */
uint8_t glean_initialize(const struct FfiConfiguration *cfg);

uint8_t glean_on_ready_to_submit_pings(void);

uint8_t glean_is_upload_enabled(void);

void glean_set_upload_enabled(uint8_t flag);

uint8_t glean_submit_ping_by_name(FfiStr ping_name, FfiStr reason);

char *glean_ping_collect(uint64_t ping_type_handle, FfiStr reason);

void glean_set_experiment_active(FfiStr experiment_id,
                                 FfiStr branch,
                                 RawStringArray extra_keys,
                                 RawStringArray extra_values,
                                 int32_t extra_len);

void glean_set_experiment_inactive(FfiStr experiment_id);

uint8_t glean_experiment_test_is_active(FfiStr experiment_id);

char *glean_experiment_test_get_data(FfiStr experiment_id);

void glean_clear_application_lifetime_metrics(void);

/**
 * Try to unblock the RLB dispatcher to start processing queued tasks.
 *
 * **Note**: glean-core does not have its own dispatcher at the moment.
 * This tries to detect the RLB and, if loaded, instructs the RLB dispatcher to flush.
 * This allows the usage of both the RLB and other language bindings (e.g. Kotlin/Swift)
 * within the same application.
 */
void glean_flush_rlb_dispatcher(void);

void glean_set_dirty_flag(uint8_t flag);

uint8_t glean_is_dirty_flag_set(void);

void glean_handle_client_active(void);

void glean_handle_client_inactive(void);

void glean_test_clear_all_stores(void);

void glean_destroy_glean(void);

uint8_t glean_is_first_run(void);

void glean_get_upload_task(union FfiPingUploadTask *result);

/**
 * Process and free a `FfiPingUploadTask`.
 *
 * We need to pass the whole task instead of only the document id,
 * so that we can free the strings properly on Drop.
 *
 * After return the `task` should not be used further by the caller.
 *
 * # Safety
 *
 * A valid and non-null upload task object is required for this function.
 */
void glean_process_ping_upload_response(union FfiPingUploadTask *task, uint32_t status);

/**
 * # Safety
 *
 * A valid and non-null configuration object is required for this function.
 */
uint8_t glean_initialize_for_subprocess(const struct FfiConfiguration *cfg);

uint8_t glean_set_debug_view_tag(FfiStr tag);

void glean_set_log_pings(uint8_t value);

uint8_t glean_set_source_tags(RawStringArray raw_tags, int32_t tags_count);

uint64_t glean_get_timestamp_ms(void);

/**
 * Public destructor for strings managed by the other side of the FFI.
 *
 * # Safety
 *
 * This will free the string pointer it gets passed in as an argument,
 * and thus can be wildly unsafe if misused.
 *
 * See the documentation of `ffi_support::destroy_c_string` and
 * `ffi_support::define_string_destructor!` for further info.
 */
void glean_str_free(char *s);

void glean_destroy_boolean_metric(uint64_t v);

uint64_t glean_new_boolean_metric(FfiStr category,
                                  FfiStr name,
                                  RawStringArray send_in_pings,
                                  int32_t send_in_pings_len,
                                  Lifetime lifetime,
                                  uint8_t disabled);

void glean_boolean_set(uint64_t metric_id, uint8_t value);

uint8_t glean_boolean_test_has_value(uint64_t metric_id, FfiStr storage_name);

uint8_t glean_boolean_test_get_value(uint64_t metric_id, FfiStr storage_name);

void glean_destroy_counter_metric(uint64_t v);

uint64_t glean_new_counter_metric(FfiStr category,
                                  FfiStr name,
                                  RawStringArray send_in_pings,
                                  int32_t send_in_pings_len,
                                  Lifetime lifetime,
                                  uint8_t disabled);

int32_t glean_counter_test_get_num_recorded_errors(uint64_t metric_id,
                                                   int32_t error_type,
                                                   FfiStr storage_name);

void glean_counter_add(uint64_t metric_id, int32_t amount);

uint8_t glean_counter_test_has_value(uint64_t metric_id, FfiStr storage_name);

int32_t glean_counter_test_get_value(uint64_t metric_id, FfiStr storage_name);

void glean_destroy_custom_distribution_metric(uint64_t v);

uint64_t glean_new_custom_distribution_metric(FfiStr category,
                                              FfiStr name,
                                              RawStringArray send_in_pings,
                                              int32_t send_in_pings_len,
                                              Lifetime lifetime,
                                              uint8_t disabled,
                                              uint64_t range_min,
                                              uint64_t range_max,
                                              uint64_t bucket_count,
                                              int32_t histogram_type);

int32_t glean_custom_distribution_test_get_num_recorded_errors(uint64_t metric_id,
                                                               int32_t error_type,
                                                               FfiStr storage_name);

void glean_custom_distribution_accumulate_samples(uint64_t metric_id,
                                                  RawInt64Array raw_samples,
                                                  int32_t num_samples);

uint8_t glean_custom_distribution_test_has_value(uint64_t metric_id, FfiStr storage_name);

char *glean_custom_distribution_test_get_value_as_json_string(uint64_t metric_id,
                                                              FfiStr storage_name);

void glean_destroy_datetime_metric(uint64_t v);

uint64_t glean_new_datetime_metric(FfiStr category,
                                   FfiStr name,
                                   RawStringArray send_in_pings,
                                   int32_t send_in_pings_len,
                                   Lifetime lifetime,
                                   uint8_t disabled,
                                   TimeUnit time_unit);

int32_t glean_datetime_test_get_num_recorded_errors(uint64_t metric_id,
                                                    int32_t error_type,
                                                    FfiStr storage_name);

void glean_datetime_set(uint64_t metric_id,
                        int32_t year,
                        uint32_t month,
                        uint32_t day,
                        uint32_t hour,
                        uint32_t minute,
                        uint32_t second,
                        int64_t nano,
                        int32_t offset_seconds);

uint8_t glean_datetime_test_has_value(uint64_t metric_id, FfiStr storage_name);

char *glean_datetime_test_get_value_as_string(uint64_t metric_id, FfiStr storage_name);

void glean_destroy_event_metric(uint64_t v);

int32_t glean_event_test_get_num_recorded_errors(uint64_t metric_id,
                                                 int32_t error_type,
                                                 FfiStr storage_name);

uint64_t glean_new_event_metric(FfiStr category,
                                FfiStr name,
                                RawStringArray send_in_pings,
                                int32_t send_in_pings_len,
                                int32_t lifetime,
                                uint8_t disabled,
                                RawStringArray extra_keys,
                                int32_t extra_keys_len);

void glean_event_record(uint64_t metric_id,
                        uint64_t timestamp,
                        RawIntArray extra_keys,
                        RawStringArray extra_values,
                        int32_t extra_len);

uint8_t glean_event_test_has_value(uint64_t metric_id, FfiStr storage_name);

char *glean_event_test_get_value_as_json_string(uint64_t metric_id, FfiStr storage_name);

void glean_destroy_jwe_metric(uint64_t v);

uint64_t glean_new_jwe_metric(FfiStr category,
                              FfiStr name,
                              RawStringArray send_in_pings,
                              int32_t send_in_pings_len,
                              Lifetime lifetime,
                              uint8_t disabled);

int32_t glean_jwe_test_get_num_recorded_errors(uint64_t metric_id,
                                               int32_t error_type,
                                               FfiStr storage_name);

void glean_jwe_set_with_compact_representation(uint64_t metric_id, FfiStr value);

void glean_jwe_set(uint64_t metric_id,
                   FfiStr header,
                   FfiStr key,
                   FfiStr init_vector,
                   FfiStr cipher_text,
                   FfiStr auth_tag);

uint8_t glean_jwe_test_has_value(uint64_t metric_id, FfiStr storage_name);

char *glean_jwe_test_get_value(uint64_t metric_id, FfiStr storage_name);

char *glean_jwe_test_get_value_as_json_string(uint64_t metric_id, FfiStr storage_name);

void glean_destroy_labeled_counter_metric(uint64_t v);

/**
 * Create a new labeled metric.
 */
uint64_t glean_new_labeled_counter_metric(FfiStr category,
                                          FfiStr name,
                                          RawStringArray send_in_pings,
                                          int32_t send_in_pings_len,
                                          int32_t lifetime,
                                          uint8_t disabled,
                                          RawStringArray labels,
                                          int32_t label_count);

/**
 * Create a new instance of the sub-metric of this labeled metric.
 */
uint64_t glean_labeled_counter_metric_get(uint64_t handle, FfiStr label);

int32_t glean_labeled_counter_test_get_num_recorded_errors(uint64_t metric_id,
                                                           int32_t error_type,
                                                           FfiStr storage_name);

void glean_destroy_labeled_boolean_metric(uint64_t v);

/**
 * Create a new labeled metric.
 */
uint64_t glean_new_labeled_boolean_metric(FfiStr category,
                                          FfiStr name,
                                          RawStringArray send_in_pings,
                                          int32_t send_in_pings_len,
                                          int32_t lifetime,
                                          uint8_t disabled,
                                          RawStringArray labels,
                                          int32_t label_count);

/**
 * Create a new instance of the sub-metric of this labeled metric.
 */
uint64_t glean_labeled_boolean_metric_get(uint64_t handle, FfiStr label);

int32_t glean_labeled_boolean_test_get_num_recorded_errors(uint64_t metric_id,
                                                           int32_t error_type,
                                                           FfiStr storage_name);

void glean_destroy_labeled_string_metric(uint64_t v);

/**
 * Create a new labeled metric.
 */
uint64_t glean_new_labeled_string_metric(FfiStr category,
                                         FfiStr name,
                                         RawStringArray send_in_pings,
                                         int32_t send_in_pings_len,
                                         int32_t lifetime,
                                         uint8_t disabled,
                                         RawStringArray labels,
                                         int32_t label_count);

/**
 * Create a new instance of the sub-metric of this labeled metric.
 */
uint64_t glean_labeled_string_metric_get(uint64_t handle, FfiStr label);

int32_t glean_labeled_string_test_get_num_recorded_errors(uint64_t metric_id,
                                                          int32_t error_type,
                                                          FfiStr storage_name);

void glean_destroy_memory_distribution_metric(uint64_t v);

uint64_t glean_new_memory_distribution_metric(FfiStr category,
                                              FfiStr name,
                                              RawStringArray send_in_pings,
                                              int32_t send_in_pings_len,
                                              Lifetime lifetime,
                                              uint8_t disabled,
                                              MemoryUnit memory_unit);

int32_t glean_memory_distribution_test_get_num_recorded_errors(uint64_t metric_id,
                                                               int32_t error_type,
                                                               FfiStr storage_name);

void glean_memory_distribution_accumulate(uint64_t metric_id, uint64_t sample);

void glean_memory_distribution_accumulate_samples(uint64_t metric_id,
                                                  RawInt64Array raw_samples,
                                                  int32_t num_samples);

uint8_t glean_memory_distribution_test_has_value(uint64_t metric_id, FfiStr storage_name);

char *glean_memory_distribution_test_get_value_as_json_string(uint64_t metric_id,
                                                              FfiStr storage_name);

void glean_destroy_ping_type(uint64_t v);

uint64_t glean_new_ping_type(FfiStr ping_name,
                             uint8_t include_client_id,
                             uint8_t send_if_empty,
                             RawStringArray reason_codes,
                             int32_t reason_codes_len);

uint8_t glean_test_has_ping_type(FfiStr ping_name);

void glean_register_ping_type(uint64_t ping_type_handle);

void glean_destroy_quantity_metric(uint64_t v);

uint64_t glean_new_quantity_metric(FfiStr category,
                                   FfiStr name,
                                   RawStringArray send_in_pings,
                                   int32_t send_in_pings_len,
                                   Lifetime lifetime,
                                   uint8_t disabled);

int32_t glean_quantity_test_get_num_recorded_errors(uint64_t metric_id,
                                                    int32_t error_type,
                                                    FfiStr storage_name);

void glean_quantity_set(uint64_t metric_id, int64_t value);

uint8_t glean_quantity_test_has_value(uint64_t metric_id, FfiStr storage_name);

int64_t glean_quantity_test_get_value(uint64_t metric_id, FfiStr storage_name);

void glean_destroy_string_metric(uint64_t v);

uint64_t glean_new_string_metric(FfiStr category,
                                 FfiStr name,
                                 RawStringArray send_in_pings,
                                 int32_t send_in_pings_len,
                                 Lifetime lifetime,
                                 uint8_t disabled);

int32_t glean_string_test_get_num_recorded_errors(uint64_t metric_id,
                                                  int32_t error_type,
                                                  FfiStr storage_name);

void glean_string_set(uint64_t metric_id, FfiStr value);

uint8_t glean_string_test_has_value(uint64_t metric_id, FfiStr storage_name);

char *glean_string_test_get_value(uint64_t metric_id, FfiStr storage_name);

void glean_destroy_string_list_metric(uint64_t v);

uint64_t glean_new_string_list_metric(FfiStr category,
                                      FfiStr name,
                                      RawStringArray send_in_pings,
                                      int32_t send_in_pings_len,
                                      Lifetime lifetime,
                                      uint8_t disabled);

int32_t glean_string_list_test_get_num_recorded_errors(uint64_t metric_id,
                                                       int32_t error_type,
                                                       FfiStr storage_name);

void glean_string_list_add(uint64_t metric_id, FfiStr value);

void glean_string_list_set(uint64_t metric_id, RawStringArray values, int32_t values_len);

uint8_t glean_string_list_test_has_value(uint64_t metric_id, FfiStr storage_name);

char *glean_string_list_test_get_value_as_json_string(uint64_t metric_id, FfiStr storage_name);

void glean_destroy_timespan_metric(uint64_t v);

uint64_t glean_new_timespan_metric(FfiStr category,
                                   FfiStr name,
                                   RawStringArray send_in_pings,
                                   int32_t send_in_pings_len,
                                   Lifetime lifetime,
                                   uint8_t disabled,
                                   int32_t time_unit);

int32_t glean_timespan_test_get_num_recorded_errors(uint64_t metric_id,
                                                    int32_t error_type,
                                                    FfiStr storage_name);

void glean_timespan_set_start(uint64_t metric_id, uint64_t start_time);

void glean_timespan_set_stop(uint64_t metric_id, uint64_t stop_time);

void glean_timespan_cancel(uint64_t metric_id);

void glean_timespan_set_raw_nanos(uint64_t metric_id, uint64_t elapsed_nanos);

uint8_t glean_timespan_test_has_value(uint64_t metric_id, FfiStr storage_name);

uint64_t glean_timespan_test_get_value(uint64_t metric_id, FfiStr storage_name);

void glean_destroy_timing_distribution_metric(uint64_t v);

uint64_t glean_new_timing_distribution_metric(FfiStr category,
                                              FfiStr name,
                                              RawStringArray send_in_pings,
                                              int32_t send_in_pings_len,
                                              Lifetime lifetime,
                                              uint8_t disabled,
                                              TimeUnit time_unit);

int32_t glean_timing_distribution_test_get_num_recorded_errors(uint64_t metric_id,
                                                               int32_t error_type,
                                                               FfiStr storage_name);

TimerId glean_timing_distribution_set_start(uint64_t metric_id, uint64_t start_time);

void glean_timing_distribution_set_stop_and_accumulate(uint64_t metric_id,
                                                       TimerId timer_id,
                                                       uint64_t stop_time);

void glean_timing_distribution_cancel(uint64_t metric_id, TimerId timer_id);

void glean_timing_distribution_accumulate_samples(uint64_t metric_id,
                                                  RawInt64Array raw_samples,
                                                  int32_t num_samples);

uint8_t glean_timing_distribution_test_has_value(uint64_t metric_id, FfiStr storage_name);

char *glean_timing_distribution_test_get_value_as_json_string(uint64_t metric_id,
                                                              FfiStr storage_name);

void glean_destroy_uuid_metric(uint64_t v);

uint64_t glean_new_uuid_metric(FfiStr category,
                               FfiStr name,
                               RawStringArray send_in_pings,
                               int32_t send_in_pings_len,
                               Lifetime lifetime,
                               uint8_t disabled);

int32_t glean_uuid_test_get_num_recorded_errors(uint64_t metric_id,
                                                int32_t error_type,
                                                FfiStr storage_name);

void glean_uuid_set(uint64_t metric_id, FfiStr value);

uint8_t glean_uuid_test_has_value(uint64_t metric_id, FfiStr storage_name);

char *glean_uuid_test_get_value(uint64_t metric_id, FfiStr storage_name);
