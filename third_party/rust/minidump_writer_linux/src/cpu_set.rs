use crate::Result;
use core::mem::size_of;
use std::fs::File;
use std::io::Read;

type MaskWordType = u32;
const MAX_CPUS: usize = 1024;
const MASK_WORD_BITS: usize = 8 * size_of::<MaskWordType>();
const MASK_WORD_COUNT: usize = (MAX_CPUS + MASK_WORD_BITS - 1) / MASK_WORD_BITS;

pub struct CpuSet {
    // The maximum number of supported CPUs.
    mask: [MaskWordType; MASK_WORD_COUNT],
}

impl CpuSet {
    pub fn new() -> Self {
        CpuSet {
            mask: [0; MASK_WORD_COUNT],
        }
    }

    pub fn get_count(&self) -> usize {
        let mut result = 0 as usize;
        for idx in 0..MASK_WORD_COUNT {
            result += self.mask[idx].count_ones() as usize;
        }
        return result;
    }

    pub fn set_bit(&mut self, idx: usize) {
        println!("Setting2 {}", idx);
        if idx < MAX_CPUS {
            self.mask[idx / MASK_WORD_BITS] |= 1 << (idx % MASK_WORD_BITS);
        }
    }

    pub fn parse_sys_file(&mut self, file: &mut File) -> Result<()> {
        let mut content = String::new();
        file.read_to_string(&mut content)?;
        // Expected format: comma-separated list of items, where each
        // item can be a decimal integer, or two decimal integers separated
        // by a dash.
        // E.g.:
        //       0
        //       0,1,2,3
        //       0-3
        //       1,10-23
        for items in content.split(',') {
            let items = items.trim();
            if items.is_empty() {
                continue;
            }
            // TODO: ranges
            println!("Setting {}", items);
            let cores: std::result::Result<Vec<_>, _> =
                items.split("-").map(|x| x.parse::<usize>()).collect();
            let cores = cores?;
            match cores.as_slice() {
                [x] => self.set_bit(*x),
                [x, y] => {
                    for core in *x..=*y {
                        self.set_bit(core)
                    }
                }
                _ => {
                    return Err(format!("Unparsable cores: {:?}", cores).into());
                }
            }
        }
        Ok(())
    }

    pub fn intersect_with(&mut self, other: &Self) {
        for idx in 0..MASK_WORD_COUNT {
            self.mask[idx] &= other.mask[idx];
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    // In tests we can have access to std
    extern crate std;
    use std::io::Write;

    fn new_file(content: &str) -> File {
        let mut file = tempfile::Builder::new()
            .prefix("cpu_sets")
            .tempfile()
            .unwrap();
        write!(file, "{}", content).unwrap();
        std::fs::File::open(file).unwrap()
    }

    #[test]
    fn test_empty_count() {
        let set = CpuSet::new();
        assert_eq!(set.get_count(), 0);
    }

    #[test]
    fn test_one_cpu() {
        let mut file = new_file("10");
        let mut set = CpuSet::new();
        let res = set.parse_sys_file(&mut file);
        assert!(res.is_ok());
        assert_eq!(set.get_count(), 1);
    }

    #[test]
    fn test_one_cpu_newline() {
        let mut file = new_file("10\n");
        let mut set = CpuSet::new();
        let res = set.parse_sys_file(&mut file);
        assert!(res.is_ok());
        assert_eq!(set.get_count(), 1);
    }

    #[test]
    fn test_two_cpus() {
        let mut file = new_file("1,10\n");
        let mut set = CpuSet::new();
        let res = set.parse_sys_file(&mut file);
        assert!(res.is_ok());
        assert_eq!(set.get_count(), 2);
        // TODO: Actually check the cpus?!
    }

    #[test]
    fn test_two_cpus_with_range() {
        let mut file = new_file("1-2\n");
        let mut set = CpuSet::new();
        let res = set.parse_sys_file(&mut file);
        assert!(res.is_ok());
        assert_eq!(set.get_count(), 2);
        // TODO: Actually check the cpus?!
    }

    #[test]
    fn test_ten_cpus_with_range() {
        let mut file = new_file("9-18\n");
        let mut set = CpuSet::new();
        let res = set.parse_sys_file(&mut file);
        assert!(res.is_ok());
        assert_eq!(set.get_count(), 10);
        // TODO: Actually check the cpus?!
    }

    #[test]
    fn test_multiple_items() {
        let mut file = new_file("0, 2-4, 128\n");
        let mut set = CpuSet::new();
        let res = set.parse_sys_file(&mut file);
        assert!(res.is_ok());
        assert_eq!(set.get_count(), 5);
        // TODO: Actually check the cpus?!
    }

    #[test]
    fn test_intersects_with() {
        let mut file1 = new_file("9-19\n");
        let mut set1 = CpuSet::new();
        let res = set1.parse_sys_file(&mut file1);
        assert!(res.is_ok());
        assert_eq!(set1.get_count(), 11);
        // TODO: Actually check the cpus?!

        let mut file2 = new_file("16-24\n");
        let mut set2 = CpuSet::new();
        let res = set2.parse_sys_file(&mut file2);
        assert!(res.is_ok());
        assert_eq!(set2.get_count(), 9);
        set1.intersect_with(&set2);
        assert_eq!(set1.get_count(), 4);
        assert_eq!(set2.get_count(), 9);
    }

    #[test]
    fn test_bad_input() {
        let mut file = new_file("abc\n");
        let mut set = CpuSet::new();
        let res = set.parse_sys_file(&mut file);
        assert!(res.is_err());
        assert_eq!(set.get_count(), 0);
        // TODO: Actually check the cpus?!
    }

    #[test]
    fn test_bad_input_range() {
        let mut file = new_file("1-abc\n");
        let mut set = CpuSet::new();
        let res = set.parse_sys_file(&mut file);
        assert!(res.is_err());
        assert_eq!(set.get_count(), 0);
        // TODO: Actually check the cpus?!
    }
}

/*

TEST(CpuSetTest, IntersectWith) {
  ScopedTestFile file1("9-19");
  ASSERT_TRUE(file1.IsOk());
  CpuSet set1;
  ASSERT_TRUE(set1.ParseSysFile(file1.GetFd()));
  ASSERT_EQ(11, set1.GetCount());

  ScopedTestFile file2("16-24");
  ASSERT_TRUE(file2.IsOk());
  CpuSet set2;
  ASSERT_TRUE(set2.ParseSysFile(file2.GetFd()));
  ASSERT_EQ(9, set2.GetCount());

  set1.IntersectWith(set2);
  ASSERT_EQ(4, set1.GetCount());
  ASSERT_EQ(9, set2.GetCount());
}

TEST(CpuSetTest, SelfIntersection) {
  ScopedTestFile file1("9-19");
  ASSERT_TRUE(file1.IsOk());
  CpuSet set1;
  ASSERT_TRUE(set1.ParseSysFile(file1.GetFd()));
  ASSERT_EQ(11, set1.GetCount());

  set1.IntersectWith(set1);
  ASSERT_EQ(11, set1.GetCount());
}

TEST(CpuSetTest, EmptyIntersection) {
  ScopedTestFile file1("0-19");
  ASSERT_TRUE(file1.IsOk());
  CpuSet set1;
  ASSERT_TRUE(set1.ParseSysFile(file1.GetFd()));
  ASSERT_EQ(20, set1.GetCount());

  ScopedTestFile file2("20-39");
  ASSERT_TRUE(file2.IsOk());
  CpuSet set2;
  ASSERT_TRUE(set2.ParseSysFile(file2.GetFd()));
  ASSERT_EQ(20, set2.GetCount());

  set1.IntersectWith(set2);
  ASSERT_EQ(0, set1.GetCount());

  ASSERT_EQ(20, set2.GetCount());
}
*/
