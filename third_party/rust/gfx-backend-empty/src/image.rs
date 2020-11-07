use hal::image::Kind;
use hal::memory::Requirements as MemoryRequirements;

#[derive(Debug)]
pub struct Image {
    /// What type of image this is, as well as its extent.
    kind: Kind,
}

impl Image {
    pub fn new(kind: Kind) -> Self {
        Image { kind }
    }

    pub fn get_requirements(&self) -> MemoryRequirements {
        let size = match self.kind {
            Kind::D2(width, height, layers, samples) => {
                assert_eq!(layers, 1, "Multi-layer images are not supported");
                assert_eq!(samples, 1, "Multisampled images are not supported");
                u64::from(width) * u64::from(height)
            }
            _ => unimplemented!("Unsupported image kind"),
        };
        MemoryRequirements {
            size,
            alignment: 1,
            type_mask: !0,
        }
    }
}
