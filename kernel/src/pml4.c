#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

typedef uint64_t pml4e_t;
typedef uint64_t pdpe_t;
typedef uint64_t pde_t;
typedef uint64_t pte_t;

void printf(const char *format, ...);

void print_compacted_memory_ranges(pml4e_t *pml4, uint64_t offset) 
{
    // Adjust the pml4 pointer by the offset
    pml4 = (pml4e_t *)((uint64_t)pml4 + offset);

    printf("Compacted virtual memory ranges with flags:\n");
    uint64_t from = 0;
    uint64_t to = 0;
    uint64_t flags = 0;
    int range_started = 0;
    
    // Mask to only consider RWE and cache-related bits
    uint64_t FLAG_MASK = 0x803;  // 0x800 for NX (no-execute), 0x3 for R/W/E bits
    
    for (int i = 0; i < 512; i++) {
        pml4e_t pml4e = pml4[i];
        if (!pml4e) continue;
        pdpe_t *pdpe = (pdpe_t *)((pml4e & 0x7FFFFFFFFFFFF000) + offset); // Apply offset here
        for (int j = 0; j < 512; j++) {
            pdpe_t pdpe_entry = pdpe[j];
            if (!pdpe_entry) continue;

            pde_t *pde = (pde_t *)((pdpe_entry & 0x7FFFFFFFFFFFF000) + offset); // Apply offset here
            for (int k = 0; k < 512; k++) {
                pde_t pde_entry = pde[k];
                if (!pde_entry) continue;

                if (pde_entry & (1ull << 7)) {
                    // This is a large page, with size 2 MB
                    uint64_t start = (((uint64_t)i << 39) | ((uint64_t)j << 30) | ((uint64_t)k << 21));
                    if (start & (1ull << 47)) start += 0xffff000000000000ULL;
                    uint64_t end = start + (1ull << 21) - 1;

                    uint64_t current_flags = pde_entry & FLAG_MASK; // Mask out irrelevant bits
                    if (!range_started) {
                        from = start;
                        to = end;
                        flags = current_flags;
                        range_started = 1;
                    } else if (start == to + 1 && (current_flags == flags)) {
                        // Contiguous and same relevant flags, compact
                        to = end;
                    } else {
                        // Print the previous range with flags and raw flag (no offset in the output)
                        printf("%#18lx - %#18lx Flags: R:%d W:%d X:%d Cache:%d [RAW: %#lx]\n", 
                               from, to, 
                               (flags & 0x1),      // Read
                               (flags & 0x2) >> 1, // Write
                               !(flags & 0x800),   // Execute (no-execute bit is bit 63)
                               (flags & 0x10) >> 4, // Cache (e.g., PCD - page-level cache disable bit)
                               flags               // Raw flag value (masked)
                        );
                        // Start a new range
                        from = start;
                        to = end;
                        flags = current_flags;
                    }
                } else {
                    // This is a 4 KB page table
                    pte_t *pte = (pte_t *)((pde_entry & 0x7FFFFFFFFFFFF000) + offset); // Apply offset here
                    for (int l = 0; l < 512; l++) {
                        pte_t pte_entry = pte[l];
                        if (!pte_entry) continue;

                        uint64_t start = (((uint64_t)i << 39) | ((uint64_t)j << 30) | ((uint64_t)k << 21) | ((uint64_t)l << 12));
                        if (start & (1ull << 47)) start += 0xffff000000000000ULL;
                        uint64_t end = start + (1ull << 12) - 1;

                        uint64_t current_flags = pte_entry & FLAG_MASK; // Mask out irrelevant bits
                        if (!range_started) {
                            from = start;
                            to = end;
                            flags = current_flags;
                            range_started = 1;
                        } else if (start == to + 1 && (current_flags == flags)) {
                            // Contiguous and same relevant flags, compact
                            to = end;
                        } else {
                            // Print the previous range with flags and raw flag (no offset in the output)
                            printf("%#18lx - %#18lx Flags: R:%d W:%d X:%d Cache:%d [RAW: %#lx]\n", 
                                   from, to, 
                                   (flags & 0x1),      // Read
                                   (flags & 0x2) >> 1, // Write
                                   !(flags & 0x800),   // Execute (no-execute bit is bit 63)
                                   (flags & 0x10) >> 4, // Cache (e.g., PCD - page-level cache disable bit)
                                   flags               // Raw flag value (masked)
                            );
                            // Start a new range
                            from = start;
                            to = end;
                            flags = current_flags;
                        }
                    }
                }
            }
        }
    }
    if (range_started) {
        // Print the last range with flags and raw flag (no offset in the output)
        printf("%#18lx - %#18lx Flags: R:%d W:%d X:%d Cache:%d [RAW: %#lx]\n", 
               from, to, 
               (flags & 0x1),      // Read
               (flags & 0x2) >> 1, // Write
               !(flags & 0x800),   // Execute (no-execute bit is bit 63)
               (flags & 0x10) >> 4, // Cache (e.g., PCD - page-level cache disable bit)
               flags               // Raw flag value (masked)
        );
    }
}
