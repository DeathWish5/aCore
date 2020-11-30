use crate::arch::timer;
use crate::error::AcoreResult;
use core::sync::atomic::{AtomicUsize, Ordering};
pub static TICK: AtomicUsize = AtomicUsize::new(0);

pub fn init() {
    crate::arch::timer::init()
}

pub fn handle_timer() -> AcoreResult {
    timer::set_next();
    TICK.fetch_add(1, Ordering::Relaxed);
    trace!("TICK = {}", TICK.load(Ordering::Relaxed));
    Ok(())
}

pub fn timer_now() -> u64 {
    timer::timer_now()
}
