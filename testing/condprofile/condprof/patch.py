# flake8: noqa
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# patch for https://bugzilla.mozilla.org/show_bug.cgi?id=1655869
# see https://github.com/HDE/arsenic/issues/85
from arsenic.connection import *


@ensure_task
async def request(self, *, url: str, method: str, data=None) -> Tuple[int, Any]:
    if data is None:
        data = {}
    if method not in {"POST", "PUT"}:
        data = None
        headers = {}
    else:
        headers = {"Content-Type": "application/json"}
    body = json.dumps(data) if data is not None else None
    full_url = self.prefix + url
    log.info(
        "request", url=strip_auth(full_url), method=method, body=body, headers=headers
    )

    async with self.session.request(
        url=full_url, method=method, data=body, headers=headers
    ) as response:
        response_body = await response.read()
        try:
            data = json.loads(response_body)
        except JSONDecodeError as exc:
            log.error("json-decode", body=response_body)
            data = {"error": "!internal", "message": str(exc), "stacktrace": ""}
        wrap_screen(data)
        log.info(
            "response",
            url=strip_auth(full_url),
            method=method,
            body=body,
            response=response,
            data=data,
        )
        return response.status, data


Connection.request = request
