use core::{
    future::Future,
    pin::Pin,
    task::{Context, Poll},
};

use crate::error::AcoreResult;

pub use executor;

#[derive(Default)]
struct YieldFuture {
    flag: bool,
}

impl Future for YieldFuture {
    type Output = AcoreResult;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context) -> Poll<Self::Output> {
        if self.flag {
            Poll::Ready(Ok(()))
        } else {
            self.flag = true;
            cx.waker().clone().wake();
            Poll::Pending
        }
    }
}

pub fn yield_now() -> impl Future<Output = AcoreResult> {
    YieldFuture::default()
}
