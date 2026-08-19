#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define PAGE_SIZE       4096
#define PAGE_OFFSET_BITS 12
#define NUM_TLB_ENTRIES  8
#define WALK_LATENCY     100
#define NUM_ACCESSES     50
#define NUM_PAGES        16

uint32_t page_table[NUM_PAGES];

typedef struct {
    uint32_t vpn;
    uint32_t ppn;
    int valid;
} TLBEntry;

TLBEntry tlb[NUM_TLB_ENTRIES];

uint32_t get_vpn(uint32_t va)     { return va >> PAGE_OFFSET_BITS; }
uint32_t get_offset(uint32_t va)  { return va & (PAGE_SIZE - 1); }

uint32_t tlb_lookup(uint32_t vpn, int *hit) {
    int idx = vpn % NUM_TLB_ENTRIES;
    if (tlb[idx].valid && tlb[idx].vpn == vpn) {
        *hit = 1;
        return tlb[idx].ppn;
    }
    *hit = 0;
    return 0;
}

void tlb_insert(uint32_t vpn, uint32_t ppn) {
    int idx = vpn % NUM_TLB_ENTRIES;
    tlb[idx].vpn = vpn;
    tlb[idx].ppn = ppn;
    tlb[idx].valid = 1;
}

uint32_t page_table_walk(uint32_t vpn) {
    if (vpn >= NUM_PAGES) {
        fprintf(stderr, "Error: VPN out of bounds\n");
        exit(1);
    }
    return page_table[vpn];
}

int main() {
    for (int i = 0; i < NUM_PAGES; i++) {
        page_table[i] = i + 0x100;
    }
    for (int i = 0; i < NUM_TLB_ENTRIES; i++) {
        tlb[i].valid = 0;
    }

    printf("Demo TLB Simulator: Single-Tenant, Single-Level\n");
    printf("TLB: %d entries, direct-mapped\n", NUM_TLB_ENTRIES);
    printf("Page size: %d bytes\n", PAGE_SIZE);
    printf("Walk latency: %d cycles\n\n", WALK_LATENCY);
    printf("Access # | Tenant | VA       | VPN | Hit/Miss | PA       | Cycles\n");
    printf("------------------------------------------------------------------\n");

    uint64_t total_cycles = 0;
    int total_hits = 0, total_misses = 0;

    for (int i = 0; i < NUM_ACCESSES; i++) {
        uint32_t va = ((i * 3 + i / 2) % NUM_PAGES) * PAGE_SIZE + (i * 4) % PAGE_SIZE;
        uint32_t vpn = get_vpn(va);
        uint32_t offset = get_offset(va);

        int hit = 0;
        uint32_t ppn = tlb_lookup(vpn, &hit);
        uint32_t cycles = 1;

        if (!hit) {
            ppn = page_table_walk(vpn);
            tlb_insert(vpn, ppn);
            cycles = WALK_LATENCY;
            total_misses++;
        } else {
            total_hits++;
        }

        uint32_t pa = (ppn << PAGE_OFFSET_BITS) | offset;
        total_cycles += cycles;

        printf("%8d | %6d | 0x%06X | %3d | %-8s | 0x%06X | %6d\n",
               i, 0, va, vpn, hit ? "HIT" : "MISS", pa, cycles);
    }

    printf("------------------------------------------------------------------\n");
    printf("Total accesses: %d\n", NUM_ACCESSES);
    printf("TLB hits: %d (%.2f%%)\n", total_hits, 100.0 * total_hits / NUM_ACCESSES);
    printf("TLB misses: %d (%.2f%%)\n", total_misses, 100.0 * total_misses / NUM_ACCESSES);
    printf("Total cycles: %lu\n", total_cycles);
    printf("Cycles lost to translation: %lu\n", (uint64_t)total_misses * WALK_LATENCY);

    return 0;
}
