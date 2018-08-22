use codec::RecvError;
use frame;
use proto::*;

#[derive(Debug)]
pub(crate) struct Settings {
    /// Received SETTINGS frame pending processing. The ACK must be written to
    /// the socket first then the settings applied **before** receiving any
    /// further frames.
    pending: Option<frame::Settings>,
}

impl Settings {
    pub fn new() -> Self {
        Settings {
            pending: None,
        }
    }

    pub fn recv_settings(&mut self, frame: frame::Settings) {
        if frame.is_ack() {
            debug!("received remote settings ack");
        // TODO: handle acks
        } else {
            assert!(self.pending.is_none());
            self.pending = Some(frame);
        }
    }

    pub fn send_pending_ack<T, B, C, P>(
        &mut self,
        dst: &mut Codec<T, B>,
        streams: &mut Streams<C, P>,
    ) -> Poll<(), RecvError>
    where
        T: AsyncWrite,
        B: Buf,
        C: Buf,
        P: Peer,
    {
        trace!("send_pending_ack; pending={:?}", self.pending);

        if let Some(ref settings) = self.pending {
            if !dst.poll_ready()?.is_ready() {
                trace!("failed to send ACK");
                return Ok(Async::NotReady);
            }

            // Create an ACK settings frame
            let frame = frame::Settings::ack();

            // Buffer the settings frame
            dst.buffer(frame.into())
                .ok()
                .expect("invalid settings frame");

            trace!("ACK sent; applying settings");

            if let Some(val) = settings.max_frame_size() {
                dst.set_max_send_frame_size(val as usize);
            }

            streams.apply_remote_settings(settings)?;
        }

        self.pending = None;

        Ok(().into())
    }
}
