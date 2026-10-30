// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
/// <reference path="../../libs/@types/gecko/index.d.ts" />
import {
  assert,
  assertEquals,
  runTests,
} from "../../browser-features/chrome/test/utils/test_harness.ts";

interface CanvasMessage {
  data: Record<string, unknown>;
}
interface CanvasTab {
  linkedBrowser: {
    messageManager: {
      addMessageListener(
        name: string,
        listener: (message: CanvasMessage) => void,
      ): void;
      removeMessageListener(
        name: string,
        listener: (message: CanvasMessage) => void,
      ): void;
      loadFrameScript(url: string, delayed: boolean): void;
    };
  };
}
interface CanvasWindow {
  gBrowser: {
    addTab(url: string, options: { triggeringPrincipal: unknown }): CanvasTab;
    removeTab(tab: CanvasTab): void;
  };
  wrappedJSObject?: { __floorpRuntimeFixtureURL?: string };
}

export async function runAllTests(): Promise<void> {
  await runTests("Native Worker backing canvas", [
    {
      name:
        "inert DOMParser document encodes real pixels in extension-capable content process",
      fn: async () => {
        const win = Services.wm.getMostRecentWindow(
          "navigator:browser",
        ) as unknown as CanvasWindow | null;
        assert(win, "native browser window is available");
        const fixtureURL = win.wrappedJSObject?.__floorpRuntimeFixtureURL;
        assert(
          typeof fixtureURL === "string",
          "runner provides the local fixture URL",
        );
        const tab = win.gBrowser.addTab(
          fixtureURL + "worker-target.html",
          {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          },
        );
        const topic = "floorp-worker-canvas-" + Services.uuid.generateUUID();
        const manager = tab.linkedBrowser.messageManager;
        try {
          const result = await new Promise<Record<string, unknown>>(
            (resolve) => {
              manager.addMessageListener(
                topic,
                function listener(message: { data: Record<string, unknown> }) {
                  manager.removeMessageListener(topic, listener);
                  resolve(message.data);
                },
              );
              manager.loadFrameScript(
                "data:application/javascript," +
                  encodeURIComponent(`
          try {
            Components.utils.importGlobalProperties(["DOMParser"]);
            const doc = new DOMParser().parseFromString("<!doctype html><html></html>", "text/html");
            const canvas = doc.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
            canvas.width=2;canvas.height=1;
            const context=canvas.getContext("2d");
            const image=context.createImageData(2,1);
            image.data.set([255,0,0,255,0,255,0,255]);
            context.putImageData(image,0,0);
            sendAsyncMessage(${JSON.stringify(topic)}, {
              pixels:Array.from(context.getImageData(0,0,2,1).data),
              encoded:canvas.toDataURL("image/png"),
              noWindow:doc.defaultView===null,
              child:Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).processType!==Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT,
            });
          } catch(error) {sendAsyncMessage(${
                    JSON.stringify(topic)
                  },{error:error.message});}
        `),
                false,
              );
            },
          );
          assert(!result.error, String(result.error));
          assert(
            result.child && result.noWindow,
            "real content process canvas uses an inert document",
          );
          assertEquals(
            JSON.stringify(result.pixels),
            "[255,0,0,255,0,255,0,255]",
            "native canvas retains input pixels",
          );
          assert(
            String(result.encoded).startsWith("data:image/png;base64,iVBOR"),
            "native encoder produces PNG",
          );
        } finally {
          win.gBrowser.removeTab(tab);
        }
      },
    },
  ]);
}
