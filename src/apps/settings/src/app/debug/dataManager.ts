// OneDriveのファイル情報を取得
export async function getOneDriveFileNameList(): Promise<string[]> {
  console.log("dataManager: getOneDriveFileNameList called");

  return await new Promise((resolve) => {
    const timeoutId = setTimeout(() => {
      console.error("dataManager: Timeout waiting for OneDrive file list");
      resolve([]);
    }, 30000);

    if (window.NRGetOneDriveFileNameList) {
      console.log(
        "dataManager: NRGetOneDriveFileNameList is available, calling it",
      );

      window.NRGetOneDriveFileNameList((data: string) => {
        clearTimeout(timeoutId);

        console.log("dataManager: Received data from OneDrive callback");
        try {
          const oneDriveFileNameList = JSON.parse(data) as string[];
          console.log(
            `dataManager: Parsed ${oneDriveFileNameList.length} files`,
          );
          console.log(
            "dataManager: First few files:",
            oneDriveFileNameList.slice(0, 5),
          );
          resolve(oneDriveFileNameList);
        } catch (error) {
          console.error("dataManager: Error parsing OneDrive data:", error);
          console.error("dataManager: Data received:", data);
          resolve([]);
        }
      });
    } else {
      clearTimeout(timeoutId);

      console.error("dataManager: NRGetOneDriveFileNameList is not available");
      const dummyData = [
        "ダミーファイル1.docx",
        "ダミーファイル2.xlsx",
        "ダミーファイル3.pdf",
      ];
      resolve(dummyData);
    }
  });
}

declare global {
  interface Window {
    NRGetOneDriveFileNameList?: (callback: (data: string) => void) => void;
  }
}
