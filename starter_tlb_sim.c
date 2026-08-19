#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * CONFIGURATION PARAMETERS
 * ============================================================================ */
#define PAGE_SIZE_4K        4096
#define PAGE_SIZE_2M        (2 * 1024 * 1024)
#define PAGE_OFFSET_4K      12
#define PAGE_OFFSET_2M      21
#define WALK_LATENCY        100
#define MAX_TENANTS         16
#define MAX_TLB_ENTRIES     256

/* ============================================================================
 * TLB STRUCTURE
 * ============================================================================ */
typedef struct {
    uint32_t vpn;
    uint32_t ppn;
    uint32_t tenant_id;
    int valid;
    int lru_counter;
} TLBEntry;

TLBEntry tlb[MAX_TLB_ENTRIES];
int tlb_size = 64;
int tlb_assoc = 4;          /* 1 = direct-mapped, MAX_TLB_ENTRIES = fully assoc */
int use_lru = 1;            /* 1 = LRU, 0 = random */

/* ============================================================================
 * PAGE TABLES (per tenant)
 * ============================================================================ */
/* TODO: Replace single-level page_table with two-level structure.
 * Each tenant needs:
 *   - Page Directory (Level 1): array of page directory entries
 *   - Page Table (Level 2): array of page table entries
 * 
 * For 4K pages: VA = [PD Index | PT Index | 12-bit Offset]
 * For 2M pages: VA = [PD Index | 21-bit Offset] (PT level bypassed)
 */
uint32_t page_table[MAX_TENANTS][1024];  /* Placeholder: single-level per tenant */

/* ============================================================================
 * ADDRESS DECOMPOSITION
 * ============================================================================ */
/* TODO: Implement correct bit extraction for both 4K and 2M page sizes.
 * 
 * 4K mode (12-bit offset):
 *   pd_index  = (va >> 22) & 0x3FF;   // bits 31:22
 *   pt_index  = (va >> 12) & 0x3FF;   // bits 21:12
 *   offset    = va & 0xFFF;           // bits 11:0
 * 
 * 2M mode (21-bit offset):
 *   pd_index  = (va >> 21) & 0x7FF;   // bits 31:21
 *   offset    = va & 0x1FFFFF;        // bits 20:0
 */
uint32_t get_pd_index(uint32_t va, int page_size) {
    /* TODO: Implement based on page_size (4K or 2M) */
    return 0;
}

uint32_t get_pt_index(uint32_t va, int page_size) {
    /* TODO: Implement based on page_size (4K or 2M) */
    /* For 2M pages, this function may not be called (PT bypassed) */
    return 0;
}

uint32_t get_offset(uint32_t va, int page_size) {
    /* TODO: Implement based on page_size (4K or 2M) */
    return 0;
}

/* ============================================================================
 * TLB OPERATIONS
 * ============================================================================ */
void tlb_init(void) {
    for (int i = 0; i < MAX_TLB_ENTRIES; i++) {
        tlb[i].valid = 0;
        tlb[i].lru_counter = 0;
    }
}

int tlb_lookup(uint32_t vpn, uint32_t tenant_id, uint32_t *ppn) {
    /* TODO: Add tenant_id to lookup key.
     * Current code does NOT check tenant_id — this allows cross-tenant leakage!
     * Fix: match both vpn AND tenant_id for a valid hit.
     */
    int set_size = (tlb_assoc == 0) ? tlb_size : tlb_assoc;
    int num_sets = tlb_size / set_size;
    int set_idx = (vpn + tenant_id) % num_sets;
    
    for (int i = 0; i < set_size; i++) {
        int idx = set_idx * set_size + i;
        if (tlb[idx].valid && tlb[idx].vpn == vpn /* && tlb[idx].tenant_id == tenant_id */) {
            *ppn = tlb[idx].ppn;
            /* TODO: Update LRU counter on hit */
            return 1; /* HIT */
        }
    }
    return 0; /* MISS */
}

void tlb_insert(uint32_t vpn, uint32_t ppn, uint32_t tenant_id) {
    /* TODO: Add tenant_id to inserted entry.
     * TODO: Implement LRU replacement (or random if configured).
     * TODO: Handle set-associative placement correctly.
     */
    int set_size = (tlb_assoc == 0) ? tlb_size : tlb_assoc;
    int num_sets = tlb_size / set_size;
    int set_idx = (vpn + tenant_id) % num_sets;
    
    /* Placeholder: insert into first invalid entry or first slot */
    int idx = set_idx * set_size;
    tlb[idx].vpn = vpn;
    tlb[idx].ppn = ppn;
    tlb[idx].tenant_id = tenant_id;  /* TODO: Verify this is set everywhere */
    tlb[idx].valid = 1;
}

/* ============================================================================
 * PAGE-TABLE WALKER
 * ============================================================================ */
uint32_t page_table_walk(uint32_t va, uint32_t tenant_id, int page_size, uint32_t *cycles) {
    /* TODO: Implement two-level walk.
     * 
     * Step 1: Extract PD index from VA using page_size
     * Step 2: Look up page directory entry for tenant_id
     * Step 3: If 4K page, extract PT index and look up page table entry
     * Step 4: If 2M page, PPN comes directly from PD entry (PT bypassed)
     * Step 5: Concatenate PPN with offset to form physical address
     * 
     * Charge walk_latency per level accessed (1 for 2M, 2 for 4K)
     */
    
    /* Placeholder: single-level lookup using tenant's page table */
    uint32_t vpn = va >> ((page_size == PAGE_SIZE_2M) ? PAGE_OFFSET_2M : PAGE_OFFSET_4K);
    if (vpn >= 1024) {
        fprintf(stderr, "Error: VPN out of bounds for tenant %u\n", tenant_id);
        exit(1);
    }
    *cycles = WALK_LATENCY * 2;  /* TODO: Should be 1× for 2M, 2× for 4K */
    return page_table[tenant_id][vpn];
}

/* ============================================================================
 * SIMULATION LOOP
 * ============================================================================ */
void run_simulation(const char *trace_file, int page_size, int num_tenants) {
    FILE *fp = fopen(trace_file, "r");
    if (!fp) {
        perror("Failed to open trace file");
        exit(1);
    }
    
    tlb_init();
    
    /* TODO: Initialize per-tenant page tables with distinct mappings */
    for (int t = 0; t < num_tenants; t++) {
        for (int p = 0; p < 1024; p++) {
            /* Each tenant gets a distinct physical base to prevent overlap */
            page_table[t][p] = (t * 0x1000) + p + 0x100;
        }
    }
    
    uint32_t tenant_id, va;
    char access_type;
    uint64_t total_cycles = 0;
    uint64_t total_hits = 0, total_misses = 0;
    uint64_t access_count = 0;
    
    printf("TLB Simulator: %s pages, %d tenants\n", 
           (page_size == PAGE_SIZE_2M) ? "2M" : "4K", num_tenants);
    printf("TLB: %d entries, %d-way associative, %s replacement\n",
           tlb_size, tlb_assoc, use_lru ? "LRU" : "Random");
    printf("------------------------------------------------------------------\n");
    
    while (fscanf(fp, "%u %x %c", &tenant_id, &va, &access_type) == 3) {
        if (tenant_id >= (uint32_t)num_tenants) {
            fprintf(stderr, "Error: Invalid tenant ID %u\n", tenant_id);
            continue;
        }
        
        uint32_t vpn = va >> ((page_size == PAGE_SIZE_2M) ? PAGE_OFFSET_2M : PAGE_OFFSET_4K);
        uint32_t ppn;
        uint32_t cycles = 1;
        
        if (tlb_lookup(vpn, tenant_id, &ppn)) {
            total_hits++;
        } else {
            uint32_t walk_cycles = 0;
            ppn = page_table_walk(va, tenant_id, page_size, &walk_cycles);
            tlb_insert(vpn, ppn, tenant_id);
            cycles = walk_cycles;
            total_misses++;
        }
        
        uint32_t offset = get_offset(va, page_size);
        uint32_t pa = (ppn << ((page_size == PAGE_SIZE_2M) ? PAGE_OFFSET_2M : PAGE_OFFSET_4K)) | offset;
        total_cycles += cycles;
        access_count++;
        
        if (access_count <= 20) {  /* Print first 20 for sanity check */
            printf("%8lu | %6u | 0x%08X | %6s | 0x%08X | %6u\n",
                   access_count, tenant_id, va, 
                   (cycles == 1) ? "HIT" : "MISS", pa, cycles);
        }
    }
    
    printf("------------------------------------------------------------------\n");
    printf("Total accesses: %lu\n", access_count);
    printf("TLB hits: %lu (%.4f%%)\n", total_hits, 100.0 * total_hits / access_count);
    printf("TLB misses: %lu (%.4f%%)\n", total_misses, 100.0 * total_misses / access_count);
    printf("Total cycles: %lu\n", total_cycles);
    printf("Cycles lost to translation: %lu\n", total_misses * WALK_LATENCY * 2);
    
    fclose(fp);
}

/* ============================================================================
 * MAIN
 * ============================================================================ */
int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <trace_file> <page_size: 4K|2M> <num_tenants>\n", argv[0]);
        return 1;
    }
    
    const char *trace_file = argv[1];
    int page_size = (strcmp(argv[2], "2M") == 0) ? PAGE_SIZE_2M : PAGE_SIZE_4K;
    int num_tenants = atoi(argv[3]);
    
    if (num_tenants < 1 || num_tenants > MAX_TENANTS) {
        fprintf(stderr, "Error: num_tenants must be between 1 and %d\n", MAX_TENANTS);
        return 1;
    }
    
    run_simulation(trace_file, page_size, num_tenants);
    return 0;
}
