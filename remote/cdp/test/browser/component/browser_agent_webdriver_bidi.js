/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_agent_task(async function remoteWebDriverBiDiActivePortFile() {
  const profileDir = await PathUtils.getProfileDir();
  const portFile = PathUtils.join(profileDir, "WebDriverBiDiActivePort");

  const port = getNonAtomicFreePort();

  await RemoteAgent.listen("http://localhost:" + port);

  if (await IOUtils.exists(portFile)) {
    const buffer = await IOUtils.read(portFile);
    const lines = new TextDecoder().decode(buffer).split("\n");
    is(lines.length, 1, "WebDriverBiDiActivePort file contains two lines");
    is(parseInt(lines[0]), port, "WebDriverBiDiActivePort file contains port");
  } else {
    ok(false, "WebDriverBiDiActivePort file written");
  }

  await RemoteAgent.close();
  ok(!(await IOUtils.exists(portFile)), "WebDriverBiDiActivePort file removed");
});
