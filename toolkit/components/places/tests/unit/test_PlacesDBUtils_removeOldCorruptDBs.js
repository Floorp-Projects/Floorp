/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEMP_FILES_TO_CREATE = 5;
const LAST_MODIFICATION_DAY = [5, 10, 15, 20, 25];
const TEST_CURRENT_TIME = Date.now();
const MS_PER_DAY = 86400000;
const RETAIN_DAYS = 14;

async function createfiles() {
  for (let i = 0; i < TEMP_FILES_TO_CREATE; i++) {
    let setTime = TEST_CURRENT_TIME;
    setTime -= LAST_MODIFICATION_DAY[i] * MS_PER_DAY;
    let fileName = "places.sqlite" + (i > 0 ? "-" + i : "") + ".corrupt";
    let filePath = PathUtils.join(PathUtils.profileDir, fileName);
    await IOUtils.writeUTF8(filePath, "test-file-delete-me", {
      tmpPath: filePath + ".tmp",
    });
    Assert.ok(await IOUtils.exists(filePath), "file created: " + filePath);
    await IOUtils.setModificationTime(filePath, setTime);
  }
}

add_task(async function removefiles() {
  await createfiles();
  await PlacesDBUtils.runTasks([PlacesDBUtils.removeOldCorruptDBs]);
  for (let i = 0; i < TEMP_FILES_TO_CREATE; i++) {
    let fileName = "places.sqlite" + (i > 0 ? "-" + i : "") + ".corrupt";
    let filePath = PathUtils.join(PathUtils.profileDir, fileName);
    if (LAST_MODIFICATION_DAY[i] >= RETAIN_DAYS) {
      Assert.ok(
        !(await IOUtils.exists(filePath)),
        "Old corrupt file has been removed" + filePath
      );
    } else {
      Assert.ok(
        await IOUtils.exists(filePath),
        "Files that are not old are not removed" + filePath
      );
    }
  }
});
