import { useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { useTranslation } from "react-i18next";

interface InstallCompleteProps {
    success: boolean;
    message: string;
    onReset: () => void;
    showLegacyDownload?: boolean;
}

export default function InstallComplete(
    { success, message, onReset, showLegacyDownload = false }:
        InstallCompleteProps,
) {
    const { t } = useTranslation();
    const [launchApp, setLaunchApp] = useState(true);
    const [isClosing, setIsClosing] = useState(false);

    const handleClose = async () => {
        if (isClosing) return;

        try {
            setIsClosing(true);

            if (success && launchApp) {
                try {
                    await invoke("launch_floorp_browser");
                } catch (e) {
                    const errorMessage = e as string;
                    console.error(
                        "Failed to launch Floorp browser:",
                        errorMessage,
                    );

                    // エラーメッセージがi18n形式の場合は処理
                    if (errorMessage.includes("|")) {
                        const [key, param] = errorMessage.split("|");
                        console.error(t(key, { 0: param }));
                    } else {
                        console.error(t(errorMessage));
                    }
                }
            }

            await invoke("exit_application");
        } catch (err) {
            console.error("Failed to exit application:", err);
            setIsClosing(false);
        }
    };

    const toggleLaunchApp = () => {
        setLaunchApp(!launchApp);
    };

    const handleLegacyDownload = () => {
        // Open Floorp.app legacy installer page
        window.open("https://floorp.app/download/", "_blank");
    };

    return (
        <div className="flex flex-col items-center justify-center h-full text-center m-auto">
            <div className="bg-base-200 p-8 rounded-xl shadow-lg max-w-md w-full">
                {success
                    ? (
                        <>
                            <div className="mb-6 text-success">
                                <svg
                                    xmlns="http://www.w3.org/2000/svg"
                                    className="h-24 w-24 mx-auto"
                                    fill="none"
                                    viewBox="0 0 24 24"
                                    stroke="currentColor"
                                >
                                    <path
                                        strokeLinecap="round"
                                        strokeLinejoin="round"
                                        strokeWidth={2}
                                        d="M5 13l4 4L19 7"
                                    />
                                </svg>
                            </div>
                            <h2 className="text-2xl font-bold mb-4">
                                {t("app.complete.success")}
                            </h2>
                            <p className="mb-6">{message}</p>

                            <div className="form-control w-full max-w-xs mx-auto mb-4">
                                <label className="label cursor-pointer justify-center">
                                    <span className="label-text mr-4">
                                        {t("app.complete.launchCheckbox")}
                                    </span>
                                    <input
                                        type="checkbox"
                                        className="checkbox checkbox-primary"
                                        checked={launchApp}
                                        onChange={toggleLaunchApp}
                                    />
                                </label>
                            </div>

                            <div className="flex justify-center space-x-4">
                                <button
                                    className={`btn btn-primary ${
                                        isClosing ? "btn-disabled" : ""
                                    }`}
                                    onClick={handleClose}
                                    disabled={isClosing}
                                >
                                    {isClosing
                                        ? t("app.complete.countdownMessage", {
                                            count: 3,
                                        })
                                        : t("app.complete.closeButton")}
                                </button>
                            </div>
                        </>
                    )
                    : (
                        <>
                            <div className="mb-6 text-error">
                                <svg
                                    xmlns="http://www.w3.org/2000/svg"
                                    className="h-24 w-24 mx-auto"
                                    fill="none"
                                    viewBox="0 0 24 24"
                                    stroke="currentColor"
                                >
                                    <path
                                        strokeLinecap="round"
                                        strokeLinejoin="round"
                                        strokeWidth={2}
                                        d="M6 18L18 6M6 6l12 12"
                                    />
                                </svg>
                            </div>
                            <h2 className="text-2xl font-bold mb-4">
                                {t("app.complete.error")}
                            </h2>
                            <p className="mb-6 text-error">{message}</p>

                            {showLegacyDownload && (
                                <div className="mb-4 p-3 border-info border-opacity-25">
                                    <p className="text-xs mb-2 text-info">
                                        {t("app.complete.legacyDownloadMessage")}
                                    </p>
                                    <button
                                        className="btn btn-info btn-xs"
                                        onClick={handleLegacyDownload}
                                        disabled={isClosing}
                                    >
                                        <svg
                                            xmlns="http://www.w3.org/2000/svg"
                                            className="h-3 w-3 mr-1"
                                            fill="none"
                                            viewBox="0 0 24 24"
                                            stroke="currentColor"
                                        >
                                            <path
                                                strokeLinecap="round"
                                                strokeLinejoin="round"
                                                strokeWidth={2}
                                                d="M12 10v6m0 0l-3-3m3 3l3-3m2 8H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z"
                                            />
                                        </svg>
                                        {t("app.complete.legacyDownloadButton")}
                                    </button>
                                </div>
                            )}

                            <div className="flex justify-center space-x-4">
                                <button
                                    className="btn btn-outline"
                                    onClick={onReset}
                                    disabled={isClosing}
                                >
                                    {t("app.installer.retryButton")}
                                </button>
                                <button
                                    className={`btn btn-error ${
                                        isClosing ? "btn-disabled" : ""
                                    }`}
                                    onClick={handleClose}
                                    disabled={isClosing}
                                >
                                    {isClosing
                                        ? t("app.complete.countdownMessage", {
                                            count: 3,
                                        })
                                        : t("app.complete.closeButton")}
                                </button>
                            </div>
                        </>
                    )}
            </div>
        </div>
    );
}
