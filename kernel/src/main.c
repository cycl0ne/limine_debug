#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
void init_print();
void printf(const char *format, ...);

// Set the base revision to 2, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(2);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_smp_request _smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((used, section(".requests")))
struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0, .response = NULL
};

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}


// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }
    init_print();
    printf("Booting Limine Template Kernel\n");

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        //hcf();
        printf("No Framebuffer found\n");
    } else {
        // Fetch the first framebuffer.
        struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
        // Note: we assume the framebuffer model is RGB with 32-bit pixels.
        for (size_t i = 0; i < 100; i++) {
            volatile uint32_t *fb_ptr = framebuffer->address;
            fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
        }
    }

   if (hhdm_request.response == NULL) {
        printf("HHDM not passed\n");
        // hcf();
    } else {
        struct limine_hhdm_response *hhdm_response = hhdm_request.response;
        printf("HHDM feature, revision %d\n", hhdm_response->revision);
        printf("Higher half direct map at: %x\n", hhdm_response->offset);
    }

    if (kernel_address_request.response == NULL) {
        printf("Kernel address not passed\n");
        // hcf();
    } else {
        struct limine_kernel_address_response *ka_response = kernel_address_request.response;
        printf("Kernel address feature, revision %d\n", ka_response->revision);
        printf("Physical base: %x\n", ka_response->physical_base);
        printf("Virtual base: %x\n", ka_response->virtual_base);
    }

    uint64_t *cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=a"(cr3));
    print_compacted_memory_ranges(cr3);
    // We're done, just hang...
    hcf();
}
