#include "llhttp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int handle_on_message_complete(llhttp_t *arg) { return 0; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  llhttp_t parser;
  llhttp_settings_t settings;
  llhttp_type_t http_type;

  /* We need four bytes to determine variable parameters */
  if (size < 4) {
    return 0;
  }

  int headers = (data[0] & 0x01) == 1;
  int chunked_length = (data[1] & 0x01) == 1;
  int keep_alive = (data[2] & 0x01) == 1;
  if (data[0] % 3 == 0) {
    http_type = HTTP_BOTH;
  } else if (data[0] % 3 == 1) {
    http_type = HTTP_REQUEST;
  } else {
    http_type = HTTP_RESPONSE;
  }
  data += 4;
  size -= 4;

  /* Initialize user callbacks and settings */
  llhttp_settings_init(&settings);

  /* Set user callback */
  settings.on_message_complete = handle_on_message_complete;

  llhttp_init(&parser, http_type, &settings);
  llhttp_set_lenient_headers(&parser, headers);
  llhttp_set_lenient_chunked_length(&parser, chunked_length);
  llhttp_set_lenient_keep_alive(&parser, keep_alive);

  llhttp_execute(&parser, data, size);

  return 0;
}
