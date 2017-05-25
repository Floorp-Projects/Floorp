// Copyright © 2015, Peter Atashian
// Licensed under the MIT License <LICENSE.md>
//! FFI bindings to ktmw32.
#![cfg(windows)]
extern crate winapi;
use winapi::*;
extern "system" {
    // pub fn CommitComplete();
    // pub fn CommitEnlistment();
    pub fn CommitTransaction(TransactionHandle: HANDLE) -> BOOL;
    // pub fn CommitTransactionAsync();
    // pub fn CreateEnlistment();
    // pub fn CreateResourceManager();
    pub fn CreateTransaction(
        lpTransactionAttributes: LPSECURITY_ATTRIBUTES, UOW: LPGUID, CreateOptions: DWORD,
        IsolationLevel: DWORD, IsolationFlags: DWORD, Timeout: DWORD, Description: LPWSTR,
    ) -> HANDLE;
    // pub fn CreateTransactionManager();
    // pub fn GetCurrentClockTransactionManager();
    // pub fn GetEnlistmentId();
    // pub fn GetEnlistmentRecoveryInformation();
    // pub fn GetNotificationResourceManager();
    // pub fn GetNotificationResourceManagerAsync();
    // pub fn GetTransactionId();
    // pub fn GetTransactionInformation();
    // pub fn GetTransactionManagerId();
    // pub fn OpenEnlistment();
    // pub fn OpenResourceManager();
    // pub fn OpenTransaction();
    // pub fn OpenTransactionManager();
    // pub fn OpenTransactionManagerById();
    // pub fn PrepareComplete();
    // pub fn PrepareEnlistment();
    // pub fn PrePrepareComplete();
    // pub fn PrePrepareEnlistment();
    // pub fn PrivCreateTransaction();
    // pub fn PrivIsLogWritableTransactionManager();
    // pub fn PrivPropagationComplete();
    // pub fn PrivPropagationFailed();
    // pub fn PrivRegisterProtocolAddressInformation();
    // pub fn ReadOnlyEnlistment();
    // pub fn RecoverEnlistment();
    // pub fn RecoverResourceManager();
    // pub fn RecoverTransactionManager();
    // pub fn RenameTransactionManager();
    // pub fn RollbackComplete();
    // pub fn RollbackEnlistment();
    pub fn RollbackTransaction(TransactionHandle: HANDLE) -> BOOL;
    // pub fn RollbackTransactionAsync();
    // pub fn RollforwardTransactionManager();
    // pub fn SetEnlistmentRecoveryInformation();
    // pub fn SetResourceManagerCompletionPort();
    // pub fn SetTransactionInformation();
    // pub fn SinglePhaseReject();
}
