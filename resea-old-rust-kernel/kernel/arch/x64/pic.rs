use super::asm;

pub unsafe fn init() {
    // Disable PIC. We use IOAPIC instead.
    asm::outb(0xa1, 0xff);
    asm::outb(0x21, 0xff);

    asm::outb(0x20, 0x11);
    asm::outb(0xa0, 0x11);
    asm::outb(0x21, 0x20);
    asm::outb(0xa1, 0x28);
    asm::outb(0x21, 0x04);
    asm::outb(0xa1, 0x02);
    asm::outb(0x21, 0x01);
    asm::outb(0xa1, 0x01);

    asm::outb(0xa1, 0xff);
    asm::outb(0x21, 0xff);
}