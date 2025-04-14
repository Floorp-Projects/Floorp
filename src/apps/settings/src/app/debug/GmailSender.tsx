"use client";

import React, { useCallback, useState } from "react";
import { sendGmailViaActor } from "./dataManager";

export default function GmailSender() {
  const [to, setTo] = useState("");
  const [subject, setSubject] = useState("");
  const [body, setBody] = useState("");
  const [status, setStatus] = useState("");
  const [isSending, setIsSending] = useState(false);

  const handleSend = useCallback(async () => {
    if (!to || !subject || !body) {
      setStatus("宛先、件名、本文を入力してください。");
      return;
    }
    setIsSending(true);
    setStatus("Gmail送信処理を開始します...");
    try {
      await sendGmailViaActor(to, subject, body);
      setStatus(
        "Gmailタブを開き、内容を入力して送信を試みます。（結果はコンソールを確認）",
      );
      // 必要であれば送信後にフィールドをクリア
      // setTo("");
      // setSubject("");
      // setBody("");
    } catch (error) {
      console.error("GmailSender: Error triggering Gmail send:", error);
      setStatus(
        `エラーが発生しました: ${
          error instanceof Error ? error.message : String(error)
        }`,
      );
    } finally {
      // UI上はすぐに完了したように見せる（実際の送信は別タブで行われるため）
      // 必要であれば isSending の解除を遅らせるか、別ステータスを用意
      setTimeout(() => setIsSending(false), 500); // 少し待ってから解除
    }
  }, [to, subject, body]);

  return (
    <div className="p-4 border rounded space-y-3 dark:border-gray-700">
      <h2 className="text-lg font-semibold">Gmail 送信テスト</h2>
      <div className="space-y-2">
        <div>
          <label
            htmlFor="gmail-to"
            className="block text-sm font-medium text-gray-700 dark:text-gray-300"
          >
            宛先:
          </label>
          <input
            type="email"
            id="gmail-to"
            value={to}
            onChange={(e) => setTo(e.target.value)}
            placeholder="recipient@example.com"
            className="mt-1 block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm dark:bg-gray-700 dark:border-gray-600 dark:placeholder-gray-400 dark:text-white"
            disabled={isSending}
          />
        </div>
        <div>
          <label
            htmlFor="gmail-subject"
            className="block text-sm font-medium text-gray-700 dark:text-gray-300"
          >
            件名:
          </label>
          <input
            type="text"
            id="gmail-subject"
            value={subject}
            onChange={(e) => setSubject(e.target.value)}
            placeholder="メールの件名"
            className="mt-1 block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm dark:bg-gray-700 dark:border-gray-600 dark:placeholder-gray-400 dark:text-white"
            disabled={isSending}
          />
        </div>
        <div>
          <label
            htmlFor="gmail-body"
            className="block text-sm font-medium text-gray-700 dark:text-gray-300"
          >
            本文:
          </label>
          <textarea
            id="gmail-body"
            rows={4}
            value={body}
            onChange={(e) => setBody(e.target.value)}
            placeholder="メールの本文を入力..."
            className="mt-1 block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm dark:bg-gray-700 dark:border-gray-600 dark:placeholder-gray-400 dark:text-white"
            disabled={isSending}
          />
        </div>
      </div>
      <button
        onClick={handleSend}
        disabled={isSending || !to || !subject || !body}
        className="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed"
      >
        {isSending ? "処理中..." : "Gmail で送信"}
      </button>
      {status && (
        <p className="text-sm mt-2 text-gray-600 dark:text-gray-400">
          {status}
        </p>
      )}
    </div>
  );
}
