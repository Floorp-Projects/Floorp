// SPDX-License-Identifier: MPL-2.0
// Normal owned page code observing one authored content-script public event.
document.addEventListener("floorp-owned-world-detail", (event) => {
  let observation;
  try {
    const detail = event.detail;
    observation = {
      detailType: typeof detail,
      isNull: detail === null,
      nestedValue: detail?.prefs?.marker ?? null,
    };
  } catch (error) {
    observation = {
      error: {
        name: String(error?.name).slice(0, 300),
        message: String(error?.message).slice(0, 300),
      },
    };
  }
  document.documentElement.setAttribute(
    "data-public-custom-event-observation",
    JSON.stringify(observation),
  );
});
