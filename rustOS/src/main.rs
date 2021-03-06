#![no_std]
#![no_main]
#![feature(custom_test_frameworks)]
#![test_runner(my_os::test_runner)]
#![reexport_test_harness_main = "test_main"]
use core::panic::PanicInfo;

use bootloader::{entry_point, BootInfo};
use my_os::memory;
use x86_64::{structures::paging::Page, VirtAddr};

mod serial;
mod vga_buffer;

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    println!("{}", _info);
    my_os::hlt_loop()
}

#[cfg(test)]
#[panic_handler]
fn pancic(_info: &PanicInfo) -> ! {
    my_os::test_panic_handler(_info)
}

entry_point!(kernel_main);
// name mangling is a technique which
// will rename the function to resolve duplicate
// #[no_mangle]
// pub extern "C" fn _start(boot_info: &'static BootInfo) -> ! {
fn kernel_main(boot_info: &'static BootInfo) -> ! {
    my_os::init();

    #[cfg(test)]
    test_main();
    // panic!("Try panic");

    let phys_mem_off = VirtAddr::new(boot_info.physical_memory_offset);
    let mut mapper = unsafe { memory::init(phys_mem_off) };
    let mut frame_allocator =
        unsafe { memory::BootInfoFrameAllocator::init(&boot_info.memory_map) };

    let page = Page::containing_address(VirtAddr::new(0xdeadbeaf000));
    memory::create_example_mapping(page, &mut mapper, &mut frame_allocator);

    let page_ptr: *mut u64 = page.start_address().as_mut_ptr();
    unsafe {
        page_ptr.offset(400).write_volatile(0x_f021_f077_f065_f04e);
    }
    println!("Hello, World!");

    my_os::hlt_loop()
}
