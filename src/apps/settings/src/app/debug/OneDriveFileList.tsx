import React, { useState } from "react";
import { getOneDriveFileNameList } from "./dataManager";

const OneDriveFileList: React.FC = () => {
  const [fileList, setFileList] = useState<string[]>([]);
  const [loading, setLoading] = useState<boolean>(false);
  const [error, setError] = useState<string | null>(null);
  const [dataFetched, setDataFetched] = useState<boolean>(false);

  // OneDriveからファイルリストを取得
  const handleFetchFiles = async () => {
    try {
      setLoading(true);
      setError(null);
      const files = await getOneDriveFileNameList();
      setFileList(files);
      setDataFetched(true);
    } catch (err) {
      setError("OneDriveからのファイルリスト取得に失敗しました");
      console.error("Error fetching OneDrive files:", err);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="p-4">
      <div className="flex justify-between items-center mb-4">
        <h2 className="text-xl font-bold">OneDriveファイルリスト</h2>
        <button
          onClick={handleFetchFiles}
          disabled={loading}
          className={`px-4 py-2 bg-blue-500 text-white rounded ${
            loading ? "opacity-50 cursor-not-allowed" : "hover:bg-blue-600"
          }`}
        >
          {loading ? "取得中..." : "OneDriveからファイル情報を取得"}
        </button>
      </div>

      {error && (
        <div className="mb-4 p-3 bg-red-100 border border-red-400 text-red-700 rounded">
          {error}
        </div>
      )}

      {loading
        ? (
          <div className="flex justify-center items-center h-40">
            <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500">
            </div>
          </div>
        )
        : dataFetched
        ? (
          <div>
            <p className="mb-2">
              {fileList.length}件のファイルが見つかりました
            </p>
            <div className="border rounded overflow-hidden">
              <table className="min-w-full divide-y divide-gray-200">
                <thead className="bg-gray-100">
                  <tr>
                    <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                      #
                    </th>
                    <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                      ファイル名
                    </th>
                  </tr>
                </thead>
                <tbody className="bg-white divide-y divide-gray-200">
                  {fileList.map((file, index) => (
                    <tr
                      key={index}
                      className={index % 2 === 0 ? "bg-white" : "bg-gray-50"}
                    >
                      <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                        {index + 1}
                      </td>
                      <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
                        {file}
                      </td>
                    </tr>
                  ))}
                  {fileList.length === 0 && (
                    <tr>
                      <td
                        colSpan={2}
                        className="px-6 py-4 text-center text-sm text-gray-500"
                      >
                        ファイルが見つかりません
                      </td>
                    </tr>
                  )}
                </tbody>
              </table>
            </div>
          </div>
        )
        : (
          <div className="text-center p-10 text-gray-500 bg-gray-50 rounded border">
            「OneDriveからファイル情報を取得」ボタンを押して、OneDriveのファイル一覧を取得してください
          </div>
        )}
    </div>
  );
};

export default OneDriveFileList;
