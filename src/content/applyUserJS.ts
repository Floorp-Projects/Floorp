export async function applyUserJS() {
  const userjs = await (
    await fetch("chrome://noraneko/skin/lepton/user.js")
  ).text();
  const p_userjs = userjs.replaceAll(/\/\/[\/]*.*\n/g, "\n");
  for (const line of p_userjs.split("\n")) {
    if (line.includes("user_pref")) {
      const tmp = line.replaceAll("user_pref(", "").replaceAll(");", "");
      let [prefName, value, ..._] = tmp.split(",");
      prefName = prefName.trim().replaceAll('"', "");
      value = value.trim();

      if (value === "true" || value === "false") {
        //console.log(prefName);
        Services.prefs.setBoolPref(prefName, value as unknown as boolean);
      } else if (value.includes('"')) {
        Services.prefs.setStringPref(prefName, value.replace('"', ""));
      } else if (!Number.isNaN(value)) {
        // integer
        Services.prefs.setIntPref(prefName, value as unknown as number);
      }
    }
  }
}
