/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use super::action::Action;
use bits_client::{
    bits_protocol::{
        CancelJobFailure, CompleteJobFailure, HResultMessage, MonitorJobFailure, ResumeJobFailure,
        SetJobPriorityFailure, SetNoProgressTimeoutFailure, SetUpdateIntervalFailure,
        StartJobFailure, SuspendJobFailure,
    },
    PipeError,
};
use comedy::error::HResult as ComedyError;
use nserror::{nsresult, NS_ERROR_FAILURE};
use nsstring::nsCString;
use std::convert::From;
use xpcom::interfaces::nsIBits;

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum ErrorType {
    NullArgument,
    InvalidArgument,
    NotInitialized,
    NoUtf8Conversion,
    InvalidGuid,
    PipeNotConnected,
    PipeTimeout,
    PipeBadWriteCount,
    PipeApiError,
    FailedToCreateBitsJob,
    FailedToAddFileToJob,
    FailedToApplyBitsJobSettings,
    FailedToResumeBitsJob,
    OtherBitsError,
    OtherBitsClientError,
    BitsJobNotFound,
    FailedToGetBitsJob,
    FailedToSuspendBitsJob,
    FailedToCompleteBitsJob,
    PartiallyCompletedBitsJob,
    FailedToCancelBitsJob,
    MissingResultData,
    MissingCallback,
    CallbackOnWrongThread,
    MissingBitsService,
    BitsServiceOnWrongThread,
    MissingBitsRequest,
    BitsRequestOnWrongThread,
    MissingObserver,
    ObserverOnWrongThread,
    MissingContext,
    ContextOnWrongThread,
    FailedToStartThread,
    FailedToConstructTaskRunnable,
    FailedToDispatchRunnable,
    TransferAlreadyComplete,
    OperationAlreadyInProgress,
    MissingBitsClient,
    FailedToGetJobStatus,
    BitsStateError,
    BitsStateTransientError,
    BitsStateCancelled,
    BitsStateUnexpected,
    FailedToConnectToBcm,
}

impl ErrorType {
    pub fn bits_code(&self) -> i32 {
        let val = match self {
            ErrorType::NullArgument => nsIBits::ERROR_TYPE_NULL_ARGUMENT,
            ErrorType::InvalidArgument => nsIBits::ERROR_TYPE_INVALID_ARGUMENT,
            ErrorType::NotInitialized => nsIBits::ERROR_TYPE_NOT_INITIALIZED,
            ErrorType::NoUtf8Conversion => nsIBits::ERROR_TYPE_NO_UTF8_CONVERSION,
            ErrorType::InvalidGuid => nsIBits::ERROR_TYPE_INVALID_GUID,
            ErrorType::PipeNotConnected => nsIBits::ERROR_TYPE_PIPE_NOT_CONNECTED,
            ErrorType::PipeTimeout => nsIBits::ERROR_TYPE_PIPE_TIMEOUT,
            ErrorType::PipeBadWriteCount => nsIBits::ERROR_TYPE_PIPE_BAD_WRITE_COUNT,
            ErrorType::PipeApiError => nsIBits::ERROR_TYPE_PIPE_API_ERROR,
            ErrorType::FailedToCreateBitsJob => nsIBits::ERROR_TYPE_FAILED_TO_CREATE_BITS_JOB,
            ErrorType::FailedToAddFileToJob => nsIBits::ERROR_TYPE_FAILED_TO_ADD_FILE_TO_JOB,
            ErrorType::FailedToApplyBitsJobSettings => {
                nsIBits::ERROR_TYPE_FAILED_TO_APPLY_BITS_JOB_SETTINGS
            }
            ErrorType::FailedToResumeBitsJob => nsIBits::ERROR_TYPE_FAILED_TO_RESUME_BITS_JOB,
            ErrorType::OtherBitsError => nsIBits::ERROR_TYPE_OTHER_BITS_ERROR,
            ErrorType::OtherBitsClientError => nsIBits::ERROR_TYPE_OTHER_BITS_CLIENT_ERROR,
            ErrorType::BitsJobNotFound => nsIBits::ERROR_TYPE_BITS_JOB_NOT_FOUND,
            ErrorType::FailedToGetBitsJob => nsIBits::ERROR_TYPE_FAILED_TO_GET_BITS_JOB,
            ErrorType::FailedToSuspendBitsJob => nsIBits::ERROR_TYPE_FAILED_TO_SUSPEND_BITS_JOB,
            ErrorType::FailedToCompleteBitsJob => nsIBits::ERROR_TYPE_FAILED_TO_COMPLETE_BITS_JOB,
            ErrorType::PartiallyCompletedBitsJob => {
                nsIBits::ERROR_TYPE_PARTIALLY_COMPLETED_BITS_JOB
            }
            ErrorType::FailedToCancelBitsJob => nsIBits::ERROR_TYPE_FAILED_TO_CANCEL_BITS_JOB,
            ErrorType::MissingResultData => nsIBits::ERROR_TYPE_MISSING_RESULT_DATA,
            ErrorType::MissingCallback => nsIBits::ERROR_TYPE_MISSING_CALLBACK,
            ErrorType::CallbackOnWrongThread => nsIBits::ERROR_TYPE_CALLBACK_ON_WRONG_THREAD,
            ErrorType::MissingBitsService => nsIBits::ERROR_TYPE_MISSING_BITS_SERVICE,
            ErrorType::BitsServiceOnWrongThread => nsIBits::ERROR_TYPE_BITS_SERVICE_ON_WRONG_THREAD,
            ErrorType::MissingBitsRequest => nsIBits::ERROR_TYPE_MISSING_BITS_REQUEST,
            ErrorType::BitsRequestOnWrongThread => nsIBits::ERROR_TYPE_BITS_REQUEST_ON_WRONG_THREAD,
            ErrorType::MissingObserver => nsIBits::ERROR_TYPE_MISSING_OBSERVER,
            ErrorType::ObserverOnWrongThread => nsIBits::ERROR_TYPE_OBSERVER_ON_WRONG_THREAD,
            ErrorType::MissingContext => nsIBits::ERROR_TYPE_MISSING_CONTEXT,
            ErrorType::ContextOnWrongThread => nsIBits::ERROR_TYPE_CONTEXT_ON_WRONG_THREAD,
            ErrorType::FailedToStartThread => nsIBits::ERROR_TYPE_FAILED_TO_START_THREAD,
            ErrorType::FailedToConstructTaskRunnable => {
                nsIBits::ERROR_TYPE_FAILED_TO_CONSTRUCT_TASK_RUNNABLE
            }
            ErrorType::FailedToDispatchRunnable => nsIBits::ERROR_TYPE_FAILED_TO_DISPATCH_RUNNABLE,
            ErrorType::TransferAlreadyComplete => nsIBits::ERROR_TYPE_TRANSFER_ALREADY_COMPLETE,
            ErrorType::OperationAlreadyInProgress => {
                nsIBits::ERROR_TYPE_OPERATION_ALREADY_IN_PROGRESS
            }
            ErrorType::MissingBitsClient => nsIBits::ERROR_TYPE_MISSING_BITS_CLIENT,
            ErrorType::FailedToGetJobStatus => nsIBits::ERROR_TYPE_FAILED_TO_GET_JOB_STATUS,
            ErrorType::BitsStateError => nsIBits::ERROR_TYPE_BITS_STATE_ERROR,
            ErrorType::BitsStateTransientError => nsIBits::ERROR_TYPE_BITS_STATE_TRANSIENT_ERROR,
            ErrorType::BitsStateCancelled => nsIBits::ERROR_TYPE_BITS_STATE_CANCELLED,
            ErrorType::BitsStateUnexpected => nsIBits::ERROR_TYPE_BITS_STATE_UNEXPECTED,
            ErrorType::FailedToConnectToBcm => nsIBits::ERROR_TYPE_FAILED_TO_CONNECT_TO_BCM,
        };
        val as i32
    }
}

impl From<&PipeError> for ErrorType {
    fn from(error: &PipeError) -> Self {
        match error {
            PipeError::NotConnected => ErrorType::PipeNotConnected,
            PipeError::Timeout => ErrorType::PipeTimeout,
            PipeError::WriteCount(_, _) => ErrorType::PipeBadWriteCount,
            PipeError::Api(_) => ErrorType::PipeApiError,
        }
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum ErrorStage {
    Pretask,
    CommandThread,
    AgentCommunication,
    BitsClient,
    MainThread,
}

impl ErrorStage {
    pub fn bits_code(&self) -> i32 {
        let val = match self {
            ErrorStage::Pretask => nsIBits::ERROR_STAGE_PRETASK,
            ErrorStage::CommandThread => nsIBits::ERROR_STAGE_COMMAND_THREAD,
            ErrorStage::AgentCommunication => nsIBits::ERROR_STAGE_AGENT_COMMUNICATION,
            ErrorStage::BitsClient => nsIBits::ERROR_STAGE_BITS_CLIENT,
            ErrorStage::MainThread => nsIBits::ERROR_STAGE_MAIN_THREAD,
        };
        val as i32
    }
}

#[derive(Debug, Clone)]
pub enum ErrorCode {
    None,
    Hresult(i32),
    Nsresult(nsresult),
    Message(nsCString),
}

impl From<ComedyError> for ErrorCode {
    fn from(error: ComedyError) -> Self {
        ErrorCode::Hresult(error.code())
    }
}

impl From<HResultMessage> for ErrorCode {
    fn from(result: HResultMessage) -> Self {
        ErrorCode::Hresult(result.hr)
    }
}

#[derive(Debug, Clone)]
pub struct BitsTaskError {
    pub error_type: ErrorType,
    pub error_action: Action,
    pub error_stage: ErrorStage,
    pub error_code: ErrorCode,
}

impl BitsTaskError {
    pub fn new(
        error_type: ErrorType,
        error_action: Action,
        error_stage: ErrorStage,
    ) -> BitsTaskError {
        BitsTaskError {
            error_type,
            error_action,
            error_stage,
            error_code: ErrorCode::None,
        }
    }

    pub fn missing_result(error_action: Action) -> BitsTaskError {
        BitsTaskError {
            error_type: ErrorType::MissingResultData,
            error_action,
            error_stage: ErrorStage::MainThread,
            error_code: ErrorCode::None,
        }
    }

    pub fn from_nsresult(
        error_type: ErrorType,
        error_action: Action,
        error_stage: ErrorStage,
        error_code: nsresult,
    ) -> BitsTaskError {
        BitsTaskError {
            error_type,
            error_action,
            error_stage,
            error_code: ErrorCode::Nsresult(error_code),
        }
    }

    pub fn from_hresult(
        error_type: ErrorType,
        error_action: Action,
        error_stage: ErrorStage,
        error_code: i32,
    ) -> BitsTaskError {
        BitsTaskError {
            error_type,
            error_action,
            error_stage,
            error_code: ErrorCode::Hresult(error_code),
        }
    }

    pub fn from_comedy(
        error_type: ErrorType,
        error_action: Action,
        error_stage: ErrorStage,
        comedy_error: ComedyError,
    ) -> BitsTaskError {
        BitsTaskError {
            error_type,
            error_action,
            error_stage,
            error_code: comedy_error.into(),
        }
    }

    pub fn from_pipe(error_action: Action, pipe_error: PipeError) -> BitsTaskError {
        let error_type = (&pipe_error).into();
        match pipe_error {
            PipeError::Api(comedy_error) => BitsTaskError {
                error_type,
                error_action,
                error_stage: ErrorStage::AgentCommunication,
                error_code: comedy_error.into(),
            },
            _ => BitsTaskError {
                error_type,
                error_action,
                error_stage: ErrorStage::AgentCommunication,
                error_code: ErrorCode::None,
            },
        }
    }
}

impl From<BitsTaskError> for nsresult {
    fn from(error: BitsTaskError) -> Self {
        if let ErrorCode::Nsresult(rv) = error.error_code {
            rv
        } else {
            NS_ERROR_FAILURE
        }
    }
}

impl From<StartJobFailure> for BitsTaskError {
    fn from(error: StartJobFailure) -> Self {
        let error_stage = ErrorStage::BitsClient;
        let error_action = Action::StartDownload;
        match error {
            StartJobFailure::ArgumentValidation(message) => BitsTaskError {
                error_type: ErrorType::InvalidArgument,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
            StartJobFailure::Create(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToCreateBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            StartJobFailure::AddFile(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToAddFileToJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            StartJobFailure::ApplySettings(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToApplyBitsJobSettings,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            StartJobFailure::Resume(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToResumeBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            StartJobFailure::ConnectBcm(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToConnectToBcm,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            StartJobFailure::OtherBITS(error_code) => BitsTaskError {
                error_type: ErrorType::OtherBitsError,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            StartJobFailure::Other(message) => BitsTaskError {
                error_type: ErrorType::OtherBitsClientError,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
        }
    }
}

impl From<MonitorJobFailure> for BitsTaskError {
    fn from(error: MonitorJobFailure) -> Self {
        let error_stage = ErrorStage::BitsClient;
        let error_action = Action::MonitorDownload;
        match error {
            MonitorJobFailure::ArgumentValidation(message) => BitsTaskError {
                error_type: ErrorType::InvalidArgument,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
            MonitorJobFailure::NotFound => BitsTaskError {
                error_type: ErrorType::BitsJobNotFound,
                error_action,
                error_stage,
                error_code: ErrorCode::None,
            },
            MonitorJobFailure::GetJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToGetBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            MonitorJobFailure::ConnectBcm(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToConnectToBcm,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            MonitorJobFailure::OtherBITS(error_code) => BitsTaskError {
                error_type: ErrorType::OtherBitsError,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            MonitorJobFailure::Other(message) => BitsTaskError {
                error_type: ErrorType::OtherBitsClientError,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
        }
    }
}

impl From<SuspendJobFailure> for BitsTaskError {
    fn from(error: SuspendJobFailure) -> Self {
        let error_stage = ErrorStage::BitsClient;
        let error_action = Action::Suspend;
        match error {
            SuspendJobFailure::NotFound => BitsTaskError {
                error_type: ErrorType::BitsJobNotFound,
                error_action,
                error_stage,
                error_code: ErrorCode::None,
            },
            SuspendJobFailure::GetJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToGetBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            SuspendJobFailure::SuspendJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToSuspendBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            SuspendJobFailure::ConnectBcm(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToConnectToBcm,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            SuspendJobFailure::OtherBITS(error_code) => BitsTaskError {
                error_type: ErrorType::OtherBitsError,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            SuspendJobFailure::Other(message) => BitsTaskError {
                error_type: ErrorType::OtherBitsClientError,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
        }
    }
}

impl From<ResumeJobFailure> for BitsTaskError {
    fn from(error: ResumeJobFailure) -> Self {
        let error_stage = ErrorStage::BitsClient;
        let error_action = Action::Resume;
        match error {
            ResumeJobFailure::NotFound => BitsTaskError {
                error_type: ErrorType::BitsJobNotFound,
                error_action,
                error_stage,
                error_code: ErrorCode::None,
            },
            ResumeJobFailure::GetJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToGetBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            ResumeJobFailure::ResumeJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToResumeBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            ResumeJobFailure::ConnectBcm(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToConnectToBcm,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            ResumeJobFailure::OtherBITS(error_code) => BitsTaskError {
                error_type: ErrorType::OtherBitsError,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            ResumeJobFailure::Other(message) => BitsTaskError {
                error_type: ErrorType::OtherBitsClientError,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
        }
    }
}

impl From<SetJobPriorityFailure> for BitsTaskError {
    fn from(error: SetJobPriorityFailure) -> Self {
        let error_stage = ErrorStage::BitsClient;
        let error_action = Action::SetPriority;
        match error {
            SetJobPriorityFailure::NotFound => BitsTaskError {
                error_type: ErrorType::BitsJobNotFound,
                error_action,
                error_stage,
                error_code: ErrorCode::None,
            },
            SetJobPriorityFailure::GetJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToGetBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            SetJobPriorityFailure::ApplySettings(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToApplyBitsJobSettings,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            SetJobPriorityFailure::ConnectBcm(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToConnectToBcm,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            SetJobPriorityFailure::OtherBITS(error_code) => BitsTaskError {
                error_type: ErrorType::OtherBitsError,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            SetJobPriorityFailure::Other(message) => BitsTaskError {
                error_type: ErrorType::OtherBitsClientError,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
        }
    }
}

impl From<SetNoProgressTimeoutFailure> for BitsTaskError {
    fn from(error: SetNoProgressTimeoutFailure) -> Self {
        use self::SetNoProgressTimeoutFailure::*;

        let error_stage = ErrorStage::BitsClient;
        let error_action = Action::SetNoProgressTimeout;
        match error {
            NotFound => BitsTaskError {
                error_type: ErrorType::BitsJobNotFound,
                error_action,
                error_stage,
                error_code: ErrorCode::None,
            },
            GetJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToGetBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            ApplySettings(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToApplyBitsJobSettings,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            ConnectBcm(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToConnectToBcm,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            OtherBITS(error_code) => BitsTaskError {
                error_type: ErrorType::OtherBitsError,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            Other(message) => BitsTaskError {
                error_type: ErrorType::OtherBitsClientError,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
        }
    }
}

impl From<SetUpdateIntervalFailure> for BitsTaskError {
    fn from(error: SetUpdateIntervalFailure) -> Self {
        let error_stage = ErrorStage::BitsClient;
        let error_action = Action::SetMonitorInterval;
        match error {
            SetUpdateIntervalFailure::ArgumentValidation(message) => BitsTaskError {
                error_type: ErrorType::InvalidArgument,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
            SetUpdateIntervalFailure::NotFound => BitsTaskError {
                error_type: ErrorType::BitsJobNotFound,
                error_action,
                error_stage,
                error_code: ErrorCode::None,
            },
            SetUpdateIntervalFailure::Other(message) => BitsTaskError {
                error_type: ErrorType::OtherBitsClientError,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
        }
    }
}

impl From<CompleteJobFailure> for BitsTaskError {
    fn from(error: CompleteJobFailure) -> Self {
        let error_stage = ErrorStage::BitsClient;
        let error_action = Action::Complete;
        match error {
            CompleteJobFailure::NotFound => BitsTaskError {
                error_type: ErrorType::BitsJobNotFound,
                error_action,
                error_stage,
                error_code: ErrorCode::None,
            },
            CompleteJobFailure::GetJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToGetBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            CompleteJobFailure::CompleteJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToCompleteBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            CompleteJobFailure::PartialComplete => BitsTaskError {
                error_type: ErrorType::PartiallyCompletedBitsJob,
                error_action,
                error_stage,
                error_code: ErrorCode::None,
            },
            CompleteJobFailure::ConnectBcm(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToConnectToBcm,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            CompleteJobFailure::OtherBITS(error_code) => BitsTaskError {
                error_type: ErrorType::OtherBitsError,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            CompleteJobFailure::Other(message) => BitsTaskError {
                error_type: ErrorType::OtherBitsClientError,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
        }
    }
}

impl From<CancelJobFailure> for BitsTaskError {
    fn from(error: CancelJobFailure) -> Self {
        let error_stage = ErrorStage::BitsClient;
        let error_action = Action::Cancel;
        match error {
            CancelJobFailure::NotFound => BitsTaskError {
                error_type: ErrorType::BitsJobNotFound,
                error_action,
                error_stage,
                error_code: ErrorCode::None,
            },
            CancelJobFailure::GetJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToGetBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            CancelJobFailure::CancelJob(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToCancelBitsJob,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            CancelJobFailure::ConnectBcm(error_code) => BitsTaskError {
                error_type: ErrorType::FailedToConnectToBcm,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            CancelJobFailure::OtherBITS(error_code) => BitsTaskError {
                error_type: ErrorType::OtherBitsError,
                error_action,
                error_stage,
                error_code: error_code.into(),
            },
            CancelJobFailure::Other(message) => BitsTaskError {
                error_type: ErrorType::OtherBitsClientError,
                error_action,
                error_stage,
                error_code: ErrorCode::Message(nsCString::from(message)),
            },
        }
    }
}
