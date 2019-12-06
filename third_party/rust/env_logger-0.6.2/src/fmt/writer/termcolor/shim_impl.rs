use std::io;

use fmt::{WriteStyle, Target};

pub(in ::fmt::writer) mod glob {
    
}

pub(in ::fmt::writer) struct BufferWriter {
    target: Target,
}

pub(in ::fmt) struct Buffer(Vec<u8>);

impl BufferWriter {
    pub(in ::fmt::writer) fn stderr(_is_test: bool, _write_style: WriteStyle) -> Self {
        BufferWriter {
            target: Target::Stderr,
        }
    }

    pub(in ::fmt::writer) fn stdout(_is_test: bool, _write_style: WriteStyle) -> Self {
        BufferWriter {
            target: Target::Stdout,
        }
    }

    pub(in ::fmt::writer) fn buffer(&self) -> Buffer {
        Buffer(Vec::new())
    }

    pub(in ::fmt::writer) fn print(&self, buf: &Buffer) -> io::Result<()> {
        // This impl uses the `eprint` and `print` macros
        // instead of using the streams directly.
        // This is so their output can be captured by `cargo test`
        let log = String::from_utf8_lossy(&buf.0);

        match self.target {
            Target::Stderr => eprint!("{}", log),
            Target::Stdout => print!("{}", log),
        }

        Ok(())
    }
}

impl Buffer {
    pub(in ::fmt) fn clear(&mut self) {
        self.0.clear();
    }

    pub(in ::fmt) fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.0.extend(buf);
        Ok(buf.len())
    }

    pub(in ::fmt) fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }

    #[cfg(test)]
    pub(in ::fmt) fn bytes(&self) -> &[u8] {
        &self.0
    }
}