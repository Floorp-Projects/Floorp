import React, { useState, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import installer_png from '../assets/installer.png';
import { open } from '@tauri-apps/plugin-dialog';

interface InstallerWelcomeProps {
    onInstall: (useAdmin: boolean, customInstallPath: string | null) => void;
}

const InstallerWelcome: React.FC<InstallerWelcomeProps> = ({ onInstall }) => {
    const { t } = useTranslation();
    const [useAdmin, setUseAdmin] = useState(true);
    const [customPath, setCustomPath] = useState("");
    const [useCustomPath, setUseCustomPath] = useState(false);
    const modalRef = useRef<HTMLDialogElement>(null);

    const defaultInstallPath = useAdmin
        ? "C:\\Program Files\\Ablaze Floorp"
        : "C:\\Users\\<USERNAME>\\AppData\\Local\\Ablaze Floorp";

    const handleInstall = () => {
        if (useCustomPath && customPath) {
            onInstall(useAdmin, customPath);
        } else {
            onInstall(useAdmin, null);
        }
    };

    const openPathModal = () => {
        if (modalRef.current) {
            modalRef.current.showModal();
        }
    };

    const closePathModal = () => {
        if (modalRef.current) {
            modalRef.current.close();
        }
    };

    const handleSavePathSettings = () => {
        setUseCustomPath(true);
        setUseAdmin(true);
        closePathModal();
    };

    const handleResetPath = () => {
        setCustomPath("");
        setUseCustomPath(false);
        closePathModal();
    };

    const handleBrowseFolder = async () => {
        try {
            const selected = await open({
                directory: true,
                multiple: false,
                title: t('app.modal.selectLocation'),
            });

            if (selected !== null) {
                setCustomPath(selected as string);
            }
        } catch (error) {
            console.error('failed to select folder:', error);
        }
    };

    return (
        <div className="m-auto">
            <div className="card">
                <div className="card-body flex flex-col items-center justify-center text-center">
                    <div className="avatar mb-4">
                        <div className="w-24 ring-primary ring-offset-base-100 ring-offset-2">
                            <img src={installer_png} alt="Floorp Installer" />
                        </div>
                    </div>
                    <h1 className="text-3xl font-bold mb-4">{t('app.installer.title')}</h1>
                    <p className="mb-4">
                        {t('app.installer.description')}
                    </p>

                    <div className="flex flex-row gap-12 w-2xl">
                        <div className="form-control w-full max-w-xs mb-4 flex flex-col gap-4">
                            <label className={`label cursor-pointer flex justify-between ${useCustomPath ? 'opacity-75' : ''}`}>
                                <span className="label-text font-bold">{t('app.installer.adminRights')}</span>
                                <input
                                    type="checkbox"
                                    className="toggle toggle-primary"
                                    checked={useCustomPath ? true : useAdmin}
                                    onChange={(e) => setUseAdmin(e.target.checked)}
                                    disabled={useCustomPath}
                                />
                            </label>
                            <p className="text-sm mt-1 opacity-75">
                                {useCustomPath
                                    ? t('app.installer.customPathRequired')
                                    : useAdmin
                                        ? t('app.installer.adminRightsEnabled')
                                        : t('app.installer.adminRightsDisabled')}
                            </p>
                        </div>

                        <div className="form-control w-full max-w-xs mb-6 flex flex-col gap-2">
                            <div className="flex items-center justify-between">
                                <label className="label">
                                    <span className="label-text font-bold">{t('app.installer.installLocation')}</span>
                                </label>
                                <button
                                    className="btn btn-sm btn-outline"
                                    onClick={openPathModal}
                                >
                                    {t('app.installer.change')}
                                </button>
                            </div>
                            <div className="p-2 bg-base-200 rounded-md text-sm text-left break-all h-[60px]">
                                {useCustomPath ? customPath : defaultInstallPath}
                            </div>
                        </div>
                    </div>

                    <button
                        className="btn btn-primary btn-lg shadow-xl hover:shadow-primary/20 transform hover:-translate-y-1 transition-all duration-300 px-8 text-white font-bold mt-2"
                        onClick={handleInstall}
                    >
                        <svg className="mr-2" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                            <line x1="12" y1="5" x2="12" y2="19"></line>
                            <polyline points="19 12 12 19 5 12"></polyline>
                        </svg>
                        {t('app.installer.installButton')}
                    </button>
                </div>
            </div>

            <dialog ref={modalRef} className="modal modal-bottom sm:modal-middle">
                <div className="modal-box w-11/12 max-w-md">
                    <h3 className="font-bold text-lg mb-4">{t('app.modal.selectLocation')}</h3>

                    <div className="form-control w-full">
                        <label className="label">
                            <span className="label-text">{t('app.modal.installPath')}</span>
                        </label>
                        <div className="join w-full">
                            <input
                                type="text"
                                className="input input-bordered join-item w-full"
                                placeholder={defaultInstallPath}
                                value={customPath}
                                onChange={(e) => setCustomPath(e.target.value)}
                                readOnly
                            />
                            <button
                                className="btn join-item btn-primary"
                                onClick={handleBrowseFolder}
                            >
                                {t('app.modal.browse')}
                            </button>
                        </div>
                    </div>

                    <div className="modal-action">
                        <button className="btn btn-ghost" onClick={handleResetPath}>
                            {t('app.modal.resetDefault')}
                        </button>
                        <button className="btn" onClick={closePathModal}>
                            {t('app.modal.cancel')}
                        </button>
                        <button className="btn btn-primary" onClick={handleSavePathSettings}>
                            {t('app.modal.save')}
                        </button>
                    </div>
                </div>
                <form method="dialog" className="modal-backdrop">
                    <button onClick={closePathModal}>{t('app.modal.close')}</button>
                </form>
            </dialog>
        </div>
    );
};

export default InstallerWelcome;