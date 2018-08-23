mod connection;
mod error;
mod go_away;
mod peer;
mod ping_pong;
mod settings;
mod streams;

pub(crate) use self::connection::{Config, Connection};
pub(crate) use self::error::Error;
pub(crate) use self::peer::{Peer, Dyn as DynPeer};
pub(crate) use self::streams::{StreamRef, OpaqueStreamRef, Streams};
pub(crate) use self::streams::{PollReset, Prioritized, Open};

use codec::Codec;

use self::go_away::GoAway;
use self::ping_pong::PingPong;
use self::settings::Settings;

use frame::{self, Frame};

use futures::{task, Async, Poll};
use futures::task::Task;

use bytes::Buf;

use tokio_io::AsyncWrite;

pub type PingPayload = [u8; 8];

pub type WindowSize = u32;

// Constants
pub const MAX_WINDOW_SIZE: WindowSize = (1 << 31) - 1;
pub const DEFAULT_RESET_STREAM_MAX: usize = 10;
pub const DEFAULT_RESET_STREAM_SECS: u64 = 30;
