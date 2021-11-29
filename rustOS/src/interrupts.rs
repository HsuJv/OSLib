use crate::{gdt, hlt_loop};
use crate::{print, println};
use lazy_static::lazy_static;
use pic8259::ChainedPics;
use spin;
use x86_64::structures::idt::*;

pub const PIC_1_OFFSET: u8 = 32;
pub const PIC_2_OFFSET: u8 = PIC_1_OFFSET + 8;

pub static PICS: spin::Mutex<ChainedPics> =
    spin::Mutex::new(unsafe { ChainedPics::new(PIC_1_OFFSET, PIC_2_OFFSET) });

#[derive(Debug, Clone, Copy)]
#[repr(u8)]
pub enum InterruptIndex {
    Timer = PIC_1_OFFSET,
    Keyboard,
}

impl InterruptIndex {
    fn as_u8(self) -> u8 {
        self as u8
    }
    fn as_usize(self) -> usize {
        usize::from(self.as_u8())
    }
}

lazy_static! {
    static ref IDT: InterruptDescriptorTable = {
        let mut idt = InterruptDescriptorTable::new();
        idt.breakpoint.set_handler_fn(breakpoint_handler);
        idt.page_fault.set_handler_fn(page_faulter_handler);
        unsafe {
            idt.double_fault
                .set_handler_fn(double_fault_handler)
                .set_stack_index(gdt::DOUBLE_FAULT_IST_INDEX);
        }
        idt[InterruptIndex::Timer.as_usize()].set_handler_fn(timer_interrupt_handler);
        idt[InterruptIndex::Keyboard.as_usize()].set_handler_fn(keyboard_interrupt_handler);

        idt
    };
}

pub fn init_idt() {
    IDT.load();
}

/*
 * Instead of extern "C" fn type,
 * The x86-interrupt call convention guarantees that all
 * registers are saved after entering and will be restored before return
 * Contrast to the "C" convention who only saves rbp, rbx, rsp, r12, r13, r14, r15
*/

extern "x86-interrupt" fn breakpoint_handler(stack_frame: InterruptStackFrame) {
    println!("Exception: breakpoint\n{:#?}", stack_frame);
}

extern "x86-interrupt" fn page_faulter_handler(
    stack_frame: InterruptStackFrame,
    error_code: PageFaultErrorCode,
) {
    use x86_64::registers::control::Cr2;

    println!("Exception: pagefault:");
    // Cr2 contains the virtual address which raise a page fault
    println!("Accessed Address: {:?}", Cr2::read());
    println!("Error Code:{:?}", error_code);
    println!("{:#?}", stack_frame);
    hlt_loop();
}

// double fault exception can occur when a second exception occurs during the handling of a prior (first) exception handler
/*
 * Double fault will raised if the faults occur in the following sequence:
 * --------------------------------------------------
 * |    First Exception    |    Second Exception    |
 * --------------------------------------------------
 * | Divide-by-zero,       |                        |
 * | Invalid TSS,          | Invalid TSS,           |
 * | Segment Not Present   | Segment Not Present,   |
 * | Stack-Segment Fault,  | Stack-Segment Fault,   |
 * | General Protection    | General Protection     |
 * | Fault                 | Fault                  |
 * --------------------------------------------------
 * |                       | Page Fault,            |
 * |                       | Invalid TSS,           |
 * |                       | Segment Not Present,   |
 * |      Page Fault       | Stack-Segment Fault,   |
 * |                       | General Protection     |
 * |                       | Fault                  |
 * --------------------------------------------------
*/
extern "x86-interrupt" fn double_fault_handler(
    stack_frame: InterruptStackFrame,
    _error_code: u64,
) -> ! {
    panic!("Exception: double fault\n{:#?}", stack_frame)
}

extern "x86-interrupt" fn timer_interrupt_handler(_stack_frame: InterruptStackFrame) {
    print!(".");

    unsafe {
        PICS.lock()
            .notify_end_of_interrupt(InterruptIndex::Timer.as_u8());
    }
}

extern "x86-interrupt" fn keyboard_interrupt_handler(_stack_frame: InterruptStackFrame) {
    use pc_keyboard::{layouts, DecodedKey, HandleControl, Keyboard, ScancodeSet1};
    use spin::Mutex;
    use x86_64::instructions::port::Port;

    lazy_static! {
        static ref KEYBOARD: Mutex<Keyboard<layouts::Us104Key, ScancodeSet1>> = Mutex::new(
            Keyboard::new(layouts::Us104Key, ScancodeSet1, HandleControl::Ignore)
        );
    }

    let mut keyboard = KEYBOARD.lock();
    let mut port = Port::new(0x60);

    let scancode: u8 = unsafe { port.read() };

    // translate the input scancode
    if let Ok(Some(key_event)) = keyboard.add_byte(scancode) {
        if let Some(key) = keyboard.process_keyevent(key_event) {
            match key {
                DecodedKey::Unicode(character) => print!("{}", character),
                DecodedKey::RawKey(key) => print!("{:?}", key),
            }
        }
    }

    unsafe {
        PICS.lock()
            .notify_end_of_interrupt(InterruptIndex::Keyboard.as_u8());
    }
}

#[test_case]
fn test_breakpoint_exception() {
    // invoke a breakpoint exception
    x86_64::instructions::interrupts::int3();
}
