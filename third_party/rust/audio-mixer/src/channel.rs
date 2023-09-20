// The number of channels must be unique and start from 0. They will be treated as indice in the
// mixing matrix and used to form unique bitflags in the channel map, which is a bitmap.
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum Channel {
    FrontLeft = 0,
    FrontRight = 1,
    FrontCenter = 2,
    LowFrequency = 3,
    BackLeft = 4,
    BackRight = 5,
    FrontLeftOfCenter = 6,
    FrontRightOfCenter = 7,
    BackCenter = 8,
    SideLeft = 9,
    SideRight = 10,
    TopCenter = 11,
    TopFrontLeft = 12,
    TopFrontCenter = 13,
    TopFrontRight = 14,
    TopBackLeft = 15,
    TopBackCenter = 16,
    TopBackRight = 17,
    Silence = 18,
}

impl Channel {
    pub const fn number(self) -> usize {
        self as usize
    }

    pub const fn count() -> usize {
        Channel::Silence as usize + 1
    }

    pub const fn bitmask(self) -> u32 {
        1 << self as usize
    }
}

bitflags! {
    pub struct ChannelMap: u32 {
        const FRONT_LEFT = Channel::FrontLeft.bitmask();
        const FRONT_RIGHT = Channel::FrontRight.bitmask();
        const FRONT_CENTER = Channel::FrontCenter.bitmask();
        const LOW_FREQUENCY = Channel::LowFrequency.bitmask();
        const BACK_LEFT = Channel::BackLeft.bitmask();
        const BACK_RIGHT = Channel::BackRight.bitmask();
        const FRONT_LEFT_OF_CENTER = Channel::FrontLeftOfCenter.bitmask();
        const FRONT_RIGHT_OF_CENTER = Channel::FrontRightOfCenter.bitmask();
        const BACK_CENTER = Channel::BackCenter.bitmask();
        const SIDE_LEFT = Channel::SideLeft.bitmask();
        const SIDE_RIGHT = Channel::SideRight.bitmask();
        const TOP_CENTER = Channel::TopCenter.bitmask();
        const TOP_FRONT_LEFT = Channel::TopFrontLeft.bitmask();
        const TOP_FRONT_CENTER = Channel::TopFrontCenter.bitmask();
        const TOP_FRONT_RIGHT = Channel::TopFrontRight.bitmask();
        const TOP_BACK_LEFT = Channel::TopBackLeft.bitmask();
        const TOP_BACK_CENTER = Channel::TopBackCenter.bitmask();
        const TOP_BACK_RIGHT = Channel::TopBackRight.bitmask();
        const SILENCE = Channel::Silence.bitmask();
    }
}

// Avoid printing the following types in debugging context {:?} by declaring them in impl
// rather than bitflags! {} scope.
impl ChannelMap {
    pub const FRONT_2: Self = Self::union(Self::FRONT_LEFT, Self::FRONT_RIGHT);
    pub const BACK_2: Self = Self::union(Self::BACK_LEFT, Self::BACK_RIGHT);
    pub const FRONT_2_OF_CENTER: Self =
        Self::union(Self::FRONT_LEFT_OF_CENTER, Self::FRONT_RIGHT_OF_CENTER);
    pub const SIDE_2: Self = Self::union(Self::SIDE_LEFT, Self::SIDE_RIGHT);
}

impl From<Channel> for ChannelMap {
    fn from(channel: Channel) -> Self {
        ChannelMap::from_bits(channel.bitmask()).expect("convert an invalid channel")
    }
}
