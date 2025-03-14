/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type ModalOptions = {
  jsx: string;
};

export type ModalResponse = {
  success: boolean;
  data?: Record<string, unknown>;
  error?: string;
};

export interface ModalChildFunctions {
  showModal: (
    formJsx: string,
    options?: ModalOptions,
  ) => Promise<ModalResponse>;
  hideModal: () => Promise<ModalResponse>;
  submitForm: (formData: Record<string, unknown>) => Promise<ModalResponse>;
}

export interface ModalParentFunctions {
  onFormSubmitted: (result: ModalResponse) => void;
}
