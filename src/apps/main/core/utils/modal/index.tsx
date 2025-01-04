/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { JSX } from "solid-js";
import { Portal } from "solid-js/web";
import modalStyle from "./styles.css?inline";
import { createRootHMR, render } from "@nora/solid-xul";

const targetParent = document?.getElementById("appcontent") as HTMLElement;

createRootHMR(()=>{
  render(() => <style>{modalStyle}</style>, document?.head, {
    // biome-ignore lint/suspicious/noExplicitAny: <explanation>
    marker: document?.head?.lastChild as Element,
  });
},import.meta.hot)

export function ShareModal(props: {
  onClose: () => void;
  onSave: (formControls: { id: string; value: string }[]) => void;
  ContentElement: () => JSX.Element;
  StyleElement?: () => JSX.Element;
  name?: string;
}) {
  return (
    <Portal mount={targetParent}>
      <div class="modal-overlay" id="modal-overlay">
        <div class="modal">
          <div class="modal-header">{props.name}</div>
          <div class="modal-content">{props.ContentElement()}</div>
          <div class="modal-actions">
            <button
              class="modal-button"
              type="button"
              id="close-modal"
              onClick={props.onClose}
            >
              キャンセル
            </button>
            <button
              class="modal-button primary"
              type="button"
              id="save-modal"
              onClick={() => {
                const forms =
                  document?.getElementsByClassName("form-control") || [];
                const result = Array.from(forms).map((e) => {
                  const element = e as HTMLInputElement;
                  if (!element.id || !element.value) {
                    throw new Error(
                      `Invalid Modal Form Control: "Id" and "Value" are required for all form elements! Occured element: ${element.id}, ${element.value}`,
                    );
                  }
                  return {
                    id: element.id as string,
                    value: element.value as string,
                  };
                });
                props.onSave(result);
              }}
            >
              保存
            </button>
          </div>
        </div>
      </div>
    </Portal>
  );
}
