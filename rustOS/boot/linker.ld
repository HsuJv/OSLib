ENTRY(start)

SECTIONS {
    . = 1M; /* sets the load address of the first section to 1 MiB, which is a conventional place to load a kernel */

    .boot :
    {
        /* ensure that the multiboot header is at the beginning */
        *(.multiboot_header)
    }

    .text :
    {
        *(.text)
    }
}