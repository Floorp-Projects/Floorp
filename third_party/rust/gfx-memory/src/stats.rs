use crate::Size;
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

            let line = "|".repeat(fill) + &(" ".repeat(50 - fill));
            writeln!(
                fmt,
                "Heap {}:\n{:6} / {:<6} or{} {{ effective:{} }} [{}]",
                format!("{}", index),
                format!("{}MB", used / MB),
                format!("{}MB", size / MB),
                format_basis_points(usage_basis_points),
                format_basis_points(effective_basis_points),
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
                    format_basis_points(effective_basis_points),
                    properties,
                )?;
            }
        }

        Ok(())
    }
}

fn format_basis_points(basis_points: Size) -> String {
    debug_assert!(basis_points <= 10000);
    format!("{:>3}.{:02}%", basis_points / 100, basis_points % 100)
}
