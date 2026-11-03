// SPDX-License-Identifier: MPL-2.0
import type {
  MessageJSONEvidence,
  MessageRecord,
} from "./native-message-json-test-types.ts";
export interface ResponseRaceEvidence
  extends Omit<MessageJSONEvidence, "stage"> {
  stage: "authored-actual-native-response-race";
}
export interface ResponseRaceReference {
  completed: boolean;
  extensionId: string;
  rows: MessageRecord[];
}
