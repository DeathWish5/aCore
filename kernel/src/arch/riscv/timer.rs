use super::sbi;
use riscv::register::*;

#[cfg(target_arch = "riscv64")]
pub fn get_cycle() -> u64 {
    time::read() as u64
}

#[cfg(target_arch = "riscv32")]
pub fn get_cycle() -> u64 {
    loop {
        let hi = timeh::read();
        let lo = time::read();
        let tmp = timeh::read();
        if hi == tmp {
            return ((hi as u64) << 32) | (lo as u64);
        }
    }
}

/// Enable timer interrupt
pub fn init() {
    // Enable supervisor timer interrupt
    unsafe {
        sie::set_stimer();
    }
    set_next();
}

/// Set the next timer interrupt
pub fn set_next() {
    // 100Hz @ QEMU
    let timebase = 250000;
    sbi::set_timer(get_cycle() + timebase);
}

pub fn timer_now() -> u64 {
    // TODO: get actual freq
    const FREQUENCY: u16 = 2600;
    let time = get_cycle();
    time * 1000 / FREQUENCY as u64
}
