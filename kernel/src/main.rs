#![no_std]
#![no_main]
#![feature(asm)]
#![feature(llvm_asm)]
#![feature(global_asm)]
#![feature(lang_items)]
#![feature(core_intrinsics)]

#[macro_use]
extern crate log;
#[macro_use]
extern crate alloc;
#[macro_use]
extern crate bitflags;
#[macro_use]
extern crate memoffset;
#[macro_use]
extern crate lazy_static;

mod config;
mod error;
mod lang;
#[macro_use]
mod logging;
mod asynccall;
mod fs;
mod memory;
mod sched;
mod syscall;
mod task;
mod utils;

#[cfg(target_arch = "riscv64")]
#[path = "arch/riscv/mod.rs"]
mod arch;

use core::sync::atomic::{spin_loop_hint, AtomicBool, Ordering};

#[no_mangle]
pub extern "C" fn start_kernel(arg0: usize, arg1: usize) -> ! {
    static AP_CAN_INIT: AtomicBool = AtomicBool::new(false);
    let cpu_id = arch::cpu::id();
    if cpu_id == config::BOOTSTRAP_CPU_ID {
        // init bootstrap CPU
        memory::clear_bss();
        arch::primary_init_early(arg0, arg1);
        logging::init();
        memory::init();
        unsafe { trapframe::init() };
        arch::primary_init(arg0, arg1);
        AP_CAN_INIT.store(true, Ordering::Release);
    } else {
        // init other CPUs
        while !AP_CAN_INIT.load(Ordering::Acquire) {
            spin_loop_hint();
        }
        memory::secondary_init();
        unsafe { trapframe::init() };
        arch::secondary_init(arg0, arg1);
    }

    // update the TLS register on all CPUs
    let per_cpu_ptr = task::PerCpu::from_cpu_id(cpu_id) as *const _ as usize;
    error!("{:x?}", per_cpu_ptr);
    debug_assert!(per_cpu_ptr % task::MAX_CPU_NUM == 0);
    unsafe { arch::cpu::write_tls(per_cpu_ptr + cpu_id) };

    println!("Hello, CPU {}!", cpu_id);
    match cpu_id {
        config::NORMAL_CPU_ID => normal_main(),
        config::IO_CPU_ID => io_main(),
        _ => loop {
            arch::cpu::wait_for_interrupt();
        },
    }
}

pub fn normal_main() -> ! {
    info!("Hello, normal CPU!");
    task::init();
    task::run_forever();
}

pub fn io_main() -> ! {
    info!("Hello, I/O CPU!");
    asynccall::init();
    asynccall::run_forever();
}
