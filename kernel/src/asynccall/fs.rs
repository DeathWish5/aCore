use super::{AsyncCall, AsyncCallResult};
use crate::memory::uaccess::{UserInPtr, UserOutPtr};
use crate::memory::uaccess::check_and_clone_cstr;
use alloc::sync::Arc;
use crate::fs::File;

impl AsyncCall {
    pub async fn async_read(
        &self,
        fd: usize,
        mut base: UserOutPtr<u8>,
        count: usize,
        offset: usize,
    ) -> AsyncCallResult {
        let file = self.thread.shared_res.files.lock().get_file(fd)?;
        let mut buf = vec![0u8; count];
        let count = file.read(offset, &mut buf)?;
        base.write_array(&buf[..count])?;
        Ok(count)
    }

    pub async fn async_write(
        &self,
        fd: usize,
        base: UserInPtr<u8>,
        count: usize,
        offset: usize,
    ) -> AsyncCallResult {
        let file = self.thread.shared_res.files.lock().get_file(fd)?;
        let buf = base.read_array(count)?;
        file.write(offset, &buf)
    }

    pub async fn async_open(&self, path: UserInPtr<u8>, _flags: usize, _mode: usize) -> AsyncCallResult {
        let path = check_and_clone_cstr(unsafe { path.as_ptr() } as *const u8)?;
        let file = Arc::new(File::new_memory_file(path)?);
        Ok(self.thread.shared_res.files.lock().add_file(file)?)
    }

    pub async fn async_close(&self, fd: usize) -> AsyncCallResult {
        let file = self.thread.shared_res.files.lock().get_file(fd)?;
        file.release()?;
        self.thread.shared_res.files.lock().remove_file(fd)?;
        Ok(0)
    }
}
