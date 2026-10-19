/// <reference path="../../libs/@types/gecko/index.d.ts" />
// SPDX-License-Identifier: MPL-2.0

/** @type {{setTimeout:(callback:()=>void,delay:number)=>number,clearTimeout:(id:number)=>void}} */
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

/**
 * Bound private actor queries so a lost document cannot stall suite cleanup.
 * @template T
 * @param {Promise<T>|undefined} task
 * @param {string} name
 * @returns {Promise<T>}
 */
export async function boundedAutomationQuery(task, name) {
  if (!task) throw Error("Private automation actor unavailable for " + name);
  /** @type {number|undefined} */
  let timer;
  try {
    return await Promise.race([
      task,
      new Promise((_, reject) => {
        timer = setTimeout(() => {
          reject(
            Error("Private automation query timed out after 5000ms: " + name),
          );
        }, 5000);
      }),
    ]);
  } finally {
    if (timer !== undefined) clearTimeout(timer);
  }
}
