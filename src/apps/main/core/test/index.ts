import {
  type Accessor,
  createEffect,
  createSignal,
  type Setter,
} from "solid-js";
import { trpc } from "./client";

async function screenshot(): Promise<string> {
  const bmp = await window.browsingContext.currentWindowGlobal.drawSnapshot(
    document.body.getBoundingClientRect(),
    window.devicePixelRatio,
    "rgb(255,255,255)",
    true,
  );
  const canvas = document.createElement("canvas");
  // resize it to the size of our ImageBitmap
  canvas.width = bmp.width;
  canvas.height = bmp.height;
  // get a bitmaprenderer context
  const ctx2 = canvas.getContext("bitmaprenderer");
  ctx2.transferFromImageBitmap(bmp);
  // get it back as a Blob
  const blob2 = await new Promise((res) => canvas.toBlob(res));
  //console.log(blob2); // Blob
  const reader = new FileReader();
  reader.readAsDataURL(blob2);
  return new Promise((resolve, reject) => {
    reader.onloadend = () => {
      //console.log(reader.result);
      resolve(reader.result);
    };
  });
}

export namespace TestUtils {
  let progressListener: nsIWebProgressListener2;
  let pLoadEnd: Promise<string>;
  let isPLoadEndResolvedOrRejected = false;
  let aLoadEnd: Accessor<string>;
  let sLoadEnd: Setter<string>;
  /**
   * @description This promise is only for load another URI than current.
   * @returns {Promise<string>} Promise of loandend
   */
  export async function onLoadEnd() {
    if (isPLoadEndResolvedOrRejected) {
      resetPLoadEnd();
    }
    return pLoadEnd;
  }
  export function init() {
    [aLoadEnd, sLoadEnd] = createSignal("");
    progressListener = {
      async onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
        if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
          sLoadEnd(
            aWebProgress.browsingContext.window
              ? aWebProgress.browsingContext.window.location.href
              : "about:newtab",
          );
        }
      },
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener2",
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };
    window.gBrowser.addProgressListener(progressListener);
    resetPLoadEnd();
  }
  function resetPLoadEnd() {
    isPLoadEndResolvedOrRejected = false;
    pLoadEnd = new Promise((resolve, reject) => {
      const _aLoadEnd = aLoadEnd();
      createEffect(() => {
        if (_aLoadEnd !== aLoadEnd()) {
          if (aLoadEnd().startsWith("error")) {
            reject(aLoadEnd());
          } else {
            resolve(aLoadEnd());
          }
        }
      });
    });
    pLoadEnd.then(
      () => {
        isPLoadEndResolvedOrRejected = true;
      },
      () => {
        isPLoadEndResolvedOrRejected = true;
      },
    );
  }
}

const testMap = new Map<
  string,
  () => Promise<{ status: "completed" | "failure"; data: string }>
>();

if (!window.floorp) {
  window.floorp = {};
}
window.floorp.testMap = testMap;

export default async function init() {
  TestUtils.init();
  trpc.post.onTestCommand.subscribe(undefined, {
    async onData(value) {
      const testFunc = testMap.get(value.testName);
      if (testFunc) {
        trpc.post.testStatus.mutate({
          testName: value.testName,
          status: "pending",
          data: "",
        });
        const ret = await testFunc();
        trpc.post.testStatus.mutate({
          testName: value.testName,
          status: ret.status,
          data: ret.data,
        });
      } else {
        trpc.post.testStatus.mutate({
          testName: value.testName,
          status: "failure",
          data: "Function Not Found",
        });
      }
    },
  });
}

testMap.set("screenshot:about__newtab", async () => {
  const p = TestUtils.onLoadEnd();
  //https://searchfox.org/mozilla-central/rev/03258de701dbcde998cfb07f75dce2b7d8fdbe20/browser/components/sessionstore/content/aboutSessionRestore.js#201
  window.gBrowser.loadURI(Services.io.newURI("about:newtab"), {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  console.log(await p);
  trpc.post.save.mutate({
    filename: "about__newtab",
    image: await screenshot(),
  });
  return { status: "completed", data: "" };
});

trpc.post.initCompleted.mutate();
