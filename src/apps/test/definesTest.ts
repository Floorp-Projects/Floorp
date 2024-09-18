import { ee } from "./defines";

export async function runTest(testName: string) {
  let resolve;
  let reject;
  const listener = (data: {
    testName: string;
    status: "pending" | "complete" | "failure";
    data: string;
  }) => {
    if (status === "pending") {
      console.log(`test ${data.testName} status : ${data.status}`);
    } else if (status === "complete") {
      console.log(
        `test ${data.testName} status : ${data.status} data: ${data.data}`,
      );
      ee.off("testStatus", listener);
      resolve(data.data);
    } else if (status === "failure") {
      console.warn(
        `test ${data.testName} status : ${data.status} data: ${data.data}`,
      );
      ee.off("testStatus", listener);
      reject(data.data);
    }
  };
  const ret = new Promise<string>((_resolve, _reject) => {
    resolve = _resolve;
    reject = _reject;
    ee.on("testStatus", listener);
  });
  ee.emit("testCommand", { testName: testName });
  return ret;
}
