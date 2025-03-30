import { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { useTranslation } from 'react-i18next';

interface InstallCompleteProps {
    success: boolean;
    message: string;
    onReset: () => void;
}

export default function InstallComplete({ success, message, onReset }: InstallCompleteProps) {
    const { t } = useTranslation();
    const [launchApp, setLaunchApp] = useState(true);
    const [isClosing, setIsClosing] = useState(false);


    const handleClose = async () => {
        if (isClosing) return;

        try {
            setIsClosing(true);

            if (success && launchApp) {
                try {
                    await invoke('launch_floorp_browser');
                } catch (e) {
                    const errorMessage = e as string;
                    console.error('Failed to launch Floorp browser:', errorMessage);

                    // エラーメッセージがi18n形式の場合は処理
                    if (errorMessage.includes('|')) {
                        const [key, param] = errorMessage.split('|');
                        console.error(t(key, { 0: param }));
                    } else {
                        console.error(t(errorMessage));
                    }
                }
            }

            await invoke('exit_application');
        } catch (err) {
            console.error('Failed to exit application:', err);
            setIsClosing(false);
        }
    };

    const toggleLaunchApp = () => {
        setLaunchApp(!launchApp);
    };

    return (
        <div className="flex flex-col items-center justify-center h-full text-center m-auto">
            <div className="bg-base-200 p-8 rounded-xl shadow-lg max-w-md w-full">
                {success ? (
                    <>
                        <div className="mb-6 text-success">
                            <svg xmlns="http://www.w3.org/2000/svg" className="h-24 w-24 mx-auto" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                            </svg>
                        </div>
                        <h2 className="text-2xl font-bold mb-4">{t('app.complete.success')}</h2>
                        <p className="mb-6">{message}</p>

                        <div className="form-control w-full max-w-xs mx-auto mb-4">
                            <label className="label cursor-pointer justify-center">
                                <span className="label-text mr-4">{t('app.complete.launchCheckbox')}</span>
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
                                className={`btn btn-primary ${isClosing ? 'btn-disabled' : ''}`}
                                onClick={handleClose}
                                disabled={isClosing}
                            >
                                {isClosing ? t('app.complete.countdownMessage', { count: 3 }) : t('app.complete.closeButton')}
                            </button>
                        </div>
                    </>
                ) : (
                    <>
                        <div className="mb-6 text-error">
                            <svg xmlns="http://www.w3.org/2000/svg" className="h-24 w-24 mx-auto" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
                            </svg>
                        </div>
                        <h2 className="text-2xl font-bold mb-4">{t('app.complete.error')}</h2>
                        <p className="mb-6 text-error">{message}</p>
                        <div className="flex justify-center space-x-4">
                            <button
                                className="btn btn-outline"
                                onClick={onReset}
                                disabled={isClosing}
                            >
                                {t('app.installer.retryButton')}
                            </button>
                            <button
                                className={`btn btn-error ${isClosing ? 'btn-disabled' : ''}`}
                                onClick={handleClose}
                                disabled={isClosing}
                            >
                                {isClosing ? t('app.complete.countdownMessage', { count: 3 }) : t('app.complete.closeButton')}
                            </button>
                        </div>
                    </>
                )}
            </div>
        </div>
    );
}