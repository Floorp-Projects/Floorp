import React, { useRef } from 'react';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { useTranslation } from 'react-i18next';

const NavBar: React.FC = () => {
    const { t } = useTranslation();
    const appWindow = getCurrentWindow();
    const modalRef = useRef<HTMLDialogElement>(null);

    const handleCloseClick = () => {
        if (modalRef.current) {
            modalRef.current.showModal();
        }
    };

    const handleCancelClose = () => {
        if (modalRef.current) {
            modalRef.current.close();
        }
    };

    const handleConfirmClose = () => {
        appWindow.close();
    };

    return (
        <nav className="navbar h-8 relative z-10" data-tauri-drag-region>
            <div className="flex-1">
            </div>
            <div className="flex-none">
                <button
                    className="btn btn-ghost btn-sm"
                    onClick={handleCloseClick}
                >
                    <svg width="12" height="12" viewBox="0 0 12 12" className="fill-current">
                        <path d="M3,3 L9,9 M9,3 L3,9" stroke="currentColor" strokeWidth="1.1" fill="none" />
                    </svg>
                </button>
            </div>

            <dialog ref={modalRef} className="modal modal-bottom sm:modal-middle">
                <div className="modal-box">
                    <h3 className="font-bold text-lg">{t('app.exitConfirmation.title')}</h3>
                    <p className="py-4">{t('app.exitConfirmation.message')}</p>
                    <div className="modal-action">
                        <button className="btn" onClick={handleCancelClose}>{t('app.exitConfirmation.cancelButton')}</button>
                        <button className="btn btn-error" onClick={handleConfirmClose}>{t('app.exitConfirmation.confirmButton')}</button>
                    </div>
                </div>
                <form method="dialog" className="modal-backdrop">
                    <button onClick={handleCancelClose}>{t('app.modal.close')}</button>
                </form>
            </dialog>
        </nav>
    );
};

export default NavBar; 