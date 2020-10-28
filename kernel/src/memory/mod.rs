//! Physical and virtual memory management.

#![allow(dead_code)]

pub mod addr;
pub mod areas;
mod frame;
mod heap;
mod paging;
mod vmm;

use crate::error::AcoreResult;
use crate::task::current;

pub use addr::{PhysAddr, VirtAddr};
pub use frame::Frame;
pub use paging::{MMUFlags, PageTable, PageTableEntry};
pub use vmm::{MemorySet, KERNEL_MEMORY_SET};

pub const PAGE_SIZE: usize = 0x1000;
pub use crate::arch::memory::consts::*;

pub fn clear_bss() {
    extern "C" {
        fn sbss();
        fn ebss();
    }
    let start = sbss as usize;
    let end = ebss as usize;
    let step = core::mem::size_of::<usize>();
    for i in (start..end).step_by(step) {
        unsafe { (i as *mut usize).write(0) };
    }
}

pub fn handle_page_fault(vaddr: VirtAddr, access_flags: MMUFlags) -> AcoreResult {
    debug!("page fault @ {:#x} with access {:?}", vaddr, access_flags);
    current().vm.lock().handle_page_fault(vaddr, access_flags)
}

pub fn init() {
    heap::init();
    frame::init();
    vmm::init();
}

pub fn secondary_init() {
    vmm::secondary_init();
    info!("secondary CPU memory init end.");
}
