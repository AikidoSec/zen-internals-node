#![no_std]

pub use zen_ip_matcher::ffi::*;

use core::alloc::{GlobalAlloc, Layout};
use core::ffi::c_void;

extern "C" {
    fn malloc(size: usize) -> *mut c_void;
    fn realloc(ptr: *mut c_void, size: usize) -> *mut c_void;
    fn free(ptr: *mut c_void);
    fn abort() -> !;
}

// malloc returns 16-byte aligned memory on every target we build for.
const MALLOC_ALIGNMENT: usize = 16;

struct CRuntimeAllocator;

unsafe impl GlobalAlloc for CRuntimeAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if layout.align() > MALLOC_ALIGNMENT {
            return core::ptr::null_mut();
        }
        malloc(layout.size()).cast()
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        free(ptr.cast());
    }

    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        if layout.align() > MALLOC_ALIGNMENT {
            return core::ptr::null_mut();
        }
        realloc(ptr.cast(), new_size).cast()
    }
}

#[global_allocator]
static ALLOCATOR: CRuntimeAllocator = CRuntimeAllocator;

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    unsafe { abort() }
}
