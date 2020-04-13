use crate::Size;
#[cfg(feature = "colorful")]
use colorful::{core::color_string::CString, Color, Colorful as _};
use hal::memory::Properties;

/// Memory utilization stats.
#[derive(Clone, Copy, Debug)]
pub struct MemoryUtilization {
    /// Total number of bytes allocated.
    pub used: Size,
    /// Effective number bytes allocated.
    pub effective: Size,
}

/// Memory utilization of one heap.
#[derive(Clone, Copy, Debug)]
pub struct MemoryHeapUtilization {
    /// Utilization.
    pub utilization: MemoryUtilization,

    /// Memory heap size.
    pub size: Size,
}

/// Memory utilization of one type.
#[derive(Clone, Copy, Debug)]
pub struct MemoryTypeUtilization {
    /// Utilization.
    pub utilization: MemoryUtilization,

    /// Memory type info.
    pub properties: Properties,

    /// Index of heap this memory type uses.
    pub heap_index: usize,
}

/// Total memory utilization.
#[derive(Clone, Debug)]
pub struct TotalMemoryUtilization {
    /// Utilization by types.
    pub types: Vec<MemoryTypeUtilization>,

    /// Utilization by heaps.
    pub heaps: Vec<MemoryHeapUtilization>,
}

#[cfg(feature = "colorful")]
impl std::fmt::Display for TotalMemoryUtilization {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        const MB: Size = 1024 * 1024;

        writeln!(fmt, "!!! Memory utilization !!!")?;
        for (index, heap) in self.heaps.iter().enumerate() {
            let size = heap.size;
            let MemoryUtilization { used, effective } = heap.utilization;
            let usage_basis_points = used * 10000 / size;
            let fill = if usage_basis_points > 10000 {
                // Shouldn't happen, but just in case.
                50
            } else {
                (usage_basis_points / 200) as usize
            };
            let effective_basis_points = if used > 0 {
                effective * 10000 / used
            } else {
                10000
            };

            let line = ("|".repeat(fill) + &(" ".repeat(50 - fill)))
                .gradient_with_color(Color::Green, Color::Red);
            writeln!(
                fmt,
                "Heap {}:\n{:6} / {:<6} or{} {{ effective:{} }} [{}]",
                format!("{}", index).magenta(),
                format!("{}MB", used / MB),
                format!("{}MB", size / MB),
                format_basis_points(usage_basis_points),
                format_basis_points_inverted(effective_basis_points),
                line
            )?;

            for ty in self.types.iter().filter(|ty| ty.heap_index == index) {
                let properties = ty.properties;
                let MemoryUtilization { used, effective } = ty.utilization;
                let usage_basis_points = used * 10000 / size;
                let effective_basis_points = if used > 0 {
                    effective * 10000 / used
                } else {
                    0
                };

                writeln!(
                    fmt,
                    "         {:>6} or{} {{ effective:{} }} | {:?}",
                    format!("{}MB", used / MB),
                    format_basis_points(usage_basis_points),
                    format_basis_points_inverted(effective_basis_points),
                    properties,
                )?;
            }
        }

        Ok(())
    }
}

#[cfg(feature = "colorful")]
fn format_basis_points(basis_points: Size) -> CString {
    debug_assert!(basis_points <= 10000);
    let s = format!("{:>3}.{:02}%", basis_points / 100, basis_points % 100);
    if basis_points > 7500 {
        s.red()
    } else if basis_points > 5000 {
        s.yellow()
    } else if basis_points > 2500 {
        s.green()
    } else if basis_points > 100 {
        s.blue()
    } else {
        s.white()
    }
}

#[cfg(feature = "colorful")]
fn format_basis_points_inverted(basis_points: Size) -> CString {
    debug_assert!(basis_points <= 10000);
    let s = format!("{:>3}.{:02}%", basis_points / 100, basis_points % 100);
    if basis_points > 9900 {
        s.white()
    } else if basis_points > 7500 {
        s.blue()
    } else if basis_points > 5000 {
        s.green()
    } else if basis_points > 2500 {
        s.yellow()
    } else {
        s.red()
    }
}
