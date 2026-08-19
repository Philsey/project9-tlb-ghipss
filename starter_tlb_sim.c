#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * PROJECT 9: TRANSLATING TRUST
 * Two-Level Page-Table Walker + Multi-Tenant TLB Simulator
 * Week 2 Deliverable — Working Implementation
 * ============================================================================ */

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */
#define PAGE_SIZE_4K        4096
#define PAGE_SIZE_2M        (2 * 1024 * 1024)
#define OFFSET_BITS_4K      12
#define OFFSET_BITS_2M      21
#define PD_ENTRIES_4K       1024
#define PT_ENTRIES_4K       1024
#define PD_ENTRIES_2M       2048
#define WALK_LATENCY        100
#define MAX_TENANTS         16
#define MAX_TLB_ENTRIES     256

/* TLB geometry — configurable at runtime */
int tlb_size = 64;
int tlb_assoc = 4;          /* 1 = direct-mapped, MAX_TLB_ENTRIES = fully assoc */
int use_lru = 1;            /* 1 = LRU, 0 = random */
int page_size_mode = PAGE_SIZE_4K;
int page_offset_bits = OFFSET_BITS_4K;

/* Global access counter for LRU aging */
uint64_t g_access_counter = 0;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

typedef struct {
    uint32_t vpn;
    uint32_t ppn;
    uint32_t tenant_id;
    int valid;
    uint64_t lru_age;
} TLBEntry;

/* TLB implemented as a set-associative cache */
TLBEntry tlb[MAX_TLB_ENTRIES];

/* Page Directory Entry */
typedef struct {
    uint32_t pt_base[PT_ENTRIES_4K];  /* For 4K: PT[j] = PPN. For 2M: unused */
    uint32_t huge_ppn;                /* For 2M: direct PPN. For 4K: unused */
} PageDir;

/* Per-tenant page table root */
PageDir *page_dirs[MAX_TENANTS];
int num_tenants = 1;

/* ============================================================================
 * ADDRESS DECOMPOSITION
 * ============================================================================ */

/* Extract page-directory index from virtual address */
uint32_t get_pd_index(uint32_t va) {
    if (page_size_mode == PAGE_SIZE_2M) {
        /* 2M: bits 31:21 (11 bits) */
        return (va >> 21) & 0x7FF;
    } else {
        /* 4K: bits 31:22 (10 bits) */
        return (va >> 22) & 0x3FF;
    }
}

/* Extract page-table index from virtual address (4K only) */
uint32_t get_pt_index(uint32_t va) {
    /* 4K: bits 21:12 (10 bits) */
    return (va >> 12) & 0x3FF;
}

/* Extract page offset from virtual address */
uint32_t get_offset(uint32_t va) {
    if (page_size_mode == PAGE_SIZE_2M) {
        return va & 0x1FFFFF;       /* 21 bits */
    } else {
        return va & 0xFFF;          /* 12 bits */
    }
}

/* Extract VPN from VA */
uint32_t get_vpn(uint32_t va) {
    return va >> page_offset_bits;
}

/* ============================================================================
 * UNIT TEST: Address Decomposition
 * ============================================================================ */

int test_addr_decomp(void) {
    int pass = 1;
    uint32_t va = 0x00401ABC;

    printf("=== ADDRESS DECOMPOSITION UNIT TEST ===\n");
    printf("Test VA: 0x%08X\n\n", va);

    /* 4K mode test */
    page_size_mode = PAGE_SIZE_4K;
    page_offset_bits = OFFSET_BITS_4K;

    uint32_t pd_4k = get_pd_index(va);
    uint32_t pt_4k = get_pt_index(va);
    uint32_t off_4k = get_offset(va);

    printf("4K Mode:\n");
    printf("  PD Index  = (0x%08X >> 22) & 0x3FF = %u  (expected: 1)\n", va, pd_4k);
    printf("  PT Index  = (0x%08X >> 12) & 0x3FF = %u  (expected: 1)\n", va, pt_4k);
    printf("  Offset    = 0x%08X & 0xFFF = 0x%03X  (expected: 0xABC)\n", va, off_4k);

    if (pd_4k != 1 || pt_4k != 1 || off_4k != 0xABC) {
        printf("  [FAIL] 4K decomposition mismatch!\n");
        pass = 0;
    } else {
        printf("  [PASS] 4K decomposition correct.\n");
    }

    /* 2M mode test */
    page_size_mode = PAGE_SIZE_2M;
    page_offset_bits = OFFSET_BITS_2M;

    uint32_t pd_2m = get_pd_index(va);
    uint32_t off_2m = get_offset(va);

    printf("\n2M Mode:\n");
    printf("  PD Index  = (0x%08X >> 21) & 0x7FF = %u  (expected: 2)\n", va, pd_2m);
    printf("  Offset    = 0x%08X & 0x1FFFFF = 0x%05X  (expected: 0x01ABC)\n", va, off_2m);

    if (pd_2m != 2 || off_2m != 0x01ABC) {
        printf("  [FAIL] 2M decomposition mismatch!\n");
        pass = 0;
    } else {
        printf("  [PASS] 2M decomposition correct.\n");
    }

    /* Restore default */
    page_size_mode = PAGE_SIZE_4K;
    page_offset_bits = OFFSET_BITS_4K;

    printf("\n");
    return pass;
}

/* ============================================================================
 * TLB OPERATIONS
 * ============================================================================ */

void tlb_init(void) {
    for (int i = 0; i < MAX_TLB_ENTRIES; i++) {
        tlb[i].valid = 0;
        tlb[i].lru_age = 0;
    }
    g_access_counter = 0;
}

/* Compute set index from VPN and tenant ID */
int tlb_get_set(uint32_t vpn, uint32_t tenant_id) {
    int num_sets = tlb_size / tlb_assoc;
    return ((vpn + tenant_id) * 2654435761u) % num_sets;  /* Knuth multiplicative hash */
}

/* Lookup TLB: returns 1 on hit, 0 on miss. Outputs PPN. */
int tlb_lookup(uint32_t vpn, uint32_t tenant_id, uint32_t *ppn) {
    int set = tlb_get_set(vpn, tenant_id);
    int base = set * tlb_assoc;

    g_access_counter++;

    for (int i = 0; i < tlb_assoc; i++) {
        int idx = base + i;
        if (tlb[idx].valid && tlb[idx].vpn == vpn && tlb[idx].tenant_id == tenant_id) {
            *ppn = tlb[idx].ppn;
            tlb[idx].lru_age = g_access_counter;  /* Update LRU on hit */
            return 1; /* HIT */
        }
    }
    return 0; /* MISS */
}

/* Insert into TLB with LRU or random replacement */
void tlb_insert(uint32_t vpn, uint32_t ppn, uint32_t tenant_id) {
    int set = tlb_get_set(vpn, tenant_id);
    int base = set * tlb_assoc;
    int victim = -1;

    g_access_counter++;

    /* First: find invalid entry */
    for (int i = 0; i < tlb_assoc; i++) {
        int idx = base + i;
        if (!tlb[idx].valid) {
            victim = i;
            break;
        }
    }

    /* If no invalid entry, select victim by policy */
    if (victim == -1) {
        if (use_lru) {
            /* LRU: evict oldest (minimum age) */
            uint64_t min_age = g_access_counter + 1;
            for (int i = 0; i < tlb_assoc; i++) {
                int idx = base + i;
                if (tlb[idx].lru_age < min_age) {
                    min_age = tlb[idx].lru_age;
                    victim = i;
                }
            }
        } else {
            /* Random replacement */
            victim = rand() % tlb_assoc;
        }
    }

    int idx = base + victim;
    tlb[idx].vpn = vpn;
    tlb[idx].ppn = ppn;
    tlb[idx].tenant_id = tenant_id;
    tlb[idx].valid = 1;
    tlb[idx].lru_age = g_access_counter;
}

/* ============================================================================
 * PAGE-TABLE INITIALIZATION
 * ============================================================================ */

void init_page_tables(int n_tenants, int pgsz) {
    num_tenants = n_tenants;
    page_size_mode = pgsz;
    page_offset_bits = (pgsz == PAGE_SIZE_2M) ? OFFSET_BITS_2M : OFFSET_BITS_4K;

    for (int t = 0; t < MAX_TENANTS; t++) {
        if (page_dirs[t]) {
            free(page_dirs[t]);
            page_dirs[t] = NULL;
        }
    }

    for (int t = 0; t < n_tenants; t++) {
        page_dirs[t] = (PageDir *)calloc(PD_ENTRIES_4K, sizeof(PageDir));

        if (pgsz == PAGE_SIZE_4K) {
            /* 4K mode: each PD entry points to a PT with distinct PPNs */
            for (int i = 0; i < PD_ENTRIES_4K; i++) {
                for (int j = 0; j < PT_ENTRIES_4K; j++) {
                    /* Deterministic mapping: distinct per tenant and per entry */
                    page_dirs[t][i].pt_base[j] = ((t + 1) * 0x100000) + (i * PT_ENTRIES_4K) + j;
                }
            }
        } else {
            /* 2M mode: PD entry directly contains PPN for 2MB region */
            for (int i = 0; i < PD_ENTRIES_2M; i++) {
                page_dirs[t][i].huge_ppn = ((t + 1) * 0x100000) + i;
            }
        }
    }
}

/* ============================================================================
 * TWO-LEVEL PAGE-TABLE WALKER
 * ============================================================================ */

uint32_t page_table_walk(uint32_t va, uint32_t tenant_id, uint32_t *cycles_charged) {
    if (tenant_id >= (uint32_t)num_tenants) {
        fprintf(stderr, "Error: Invalid tenant ID %u\n", tenant_id);
        exit(1);
    }

    PageDir *pd = page_dirs[tenant_id];
    uint32_t ppn = 0;
    uint32_t cycles = 0;

    if (page_size_mode == PAGE_SIZE_4K) {
        /* Two-level walk for 4K pages */
        uint32_t pd_idx = get_pd_index(va);
        uint32_t pt_idx = get_pt_index(va);

        /* Walk Step 1: Page Directory */
        cycles += WALK_LATENCY;

        /* Walk Step 2: Page Table */
        ppn = pd[pd_idx].pt_base[pt_idx];
        cycles += WALK_LATENCY;

    } else {
        /* Single-level walk for 2M huge pages (PT bypassed) */
        uint32_t pd_idx = get_pd_index(va);

        /* Walk Step 1: Page Directory only */
        ppn = pd[pd_idx].huge_ppn;
        cycles += WALK_LATENCY;
    }

    *cycles_charged = cycles;
    return ppn;
}

/* ============================================================================
 * MANUAL TRACE VERIFICATION
 * ============================================================================ */

/* Hardcoded test matching the hand-computed oracle */
int test_manual_trace(void) {
    int pass = 1;

    printf("=== MANUAL TRACE VERIFICATION ===\n");
    printf("Testing 4K mode with 2 tenants...\n\n");

    /* Setup */
    tlb_init();
    init_page_tables(2, PAGE_SIZE_4K);

    /* Override page tables to match hand-computed oracle exactly */
    /* Tenant 0: VA 0x00401000 -> PD[1] -> PT[1] -> PPN 0x200 */
    page_dirs[0][1].pt_base[1] = 0x200;
    /* Tenant 1: VA 0x00401000 -> PD[1] -> PT[1] -> PPN 0x800 */
    page_dirs[1][1].pt_base[1] = 0x800;

    struct { uint32_t tenant; uint32_t va; uint32_t exp_pa; int exp_hit; } tests[] = {
        {0, 0x00401000, 0x200000, 0},  /* Miss: cold start */
        {0, 0x00401004, 0x200004, 1},  /* Hit: same page */
        {1, 0x00401000, 0x800000, 0},  /* Miss: different tenant, same VA -> different PA */
        {0, 0x00401000, 0x200000, 1},  /* Hit: tenant 0's entry still cached */
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);

    printf("Access | Tenant | VA         | Exp PA     | Exp Hit | Got PA     | Got Hit | Status\n");
    printf("-------|--------|------------|------------|---------|------------|---------|-------\n");

    for (int i = 0; i < num_tests; i++) {
        uint32_t tenant = tests[i].tenant;
        uint32_t va = tests[i].va;
        uint32_t vpn = get_vpn(va);
        uint32_t offset = get_offset(va);
        uint32_t ppn;
        uint32_t cycles = 1;
        int hit = tlb_lookup(vpn, tenant, &ppn);

        if (!hit) {
            uint32_t walk_cycles = 0;
            ppn = page_table_walk(va, tenant, &walk_cycles);
            tlb_insert(vpn, ppn, tenant);
            cycles = walk_cycles;
        }

        uint32_t pa = (ppn << page_offset_bits) | offset;
        int ok = (pa == tests[i].exp_pa && hit == tests[i].exp_hit);

        printf("  %3d  |   %2u   | 0x%08X | 0x%08X |    %c    | 0x%08X |    %c    |  %s\n",
               i, tenant, va, tests[i].exp_pa, tests[i].exp_hit ? 'H' : 'M',
               pa, hit ? 'H' : 'M', ok ? "PASS" : "FAIL");

        if (!ok) pass = 0;
    }

    printf("\n");

    /* Cross-tenant isolation check */
    printf("CROSS-TENANT ISOLATION CHECK:\n");
    uint32_t va_common = 0x00401000;
    uint32_t pa0 = (page_dirs[0][get_pd_index(va_common)].pt_base[get_pt_index(va_common)] << page_offset_bits) | get_offset(va_common);
    uint32_t pa1 = (page_dirs[1][get_pd_index(va_common)].pt_base[get_pt_index(va_common)] << page_offset_bits) | get_offset(va_common);

    printf("  Tenant 0, VA 0x%08X -> PA 0x%08X\n", va_common, pa0);
    printf("  Tenant 1, VA 0x%08X -> PA 0x%08X\n", va_common, pa1);

    if (pa0 != pa1) {
        printf("  [PASS] Same VA maps to different PAs. No cross-tenant leakage.\n");
    } else {
        printf("  [FAIL] Same VA maps to same PA! Isolation broken.\n");
        pass = 0;
    }

    printf("\n");
    return pass;
}

/* ============================================================================
 * FULL TRACE SIMULATION
 * ============================================================================ */

void run_simulation(const char *trace_file, int pgsz, int n_tenants) {
    FILE *fp = fopen(trace_file, "r");
    if (!fp) {
        perror("Failed to open trace file");
        exit(1);
    }

    init_page_tables(n_tenants, pgsz);
    tlb_init();

    uint32_t tenant_id, va;
    char access_type;
    uint64_t total_cycles = 0;
    uint64_t total_hits = 0, total_misses = 0;
    uint64_t access_count = 0;

    printf("=================================================================\n");
    printf("TLB Simulator: %s pages, %d tenants\n", 
           (pgsz == PAGE_SIZE_2M) ? "2M" : "4K", n_tenants);
    printf("TLB: %d entries, %d-way associative, %s replacement\n",
           tlb_size, tlb_assoc, use_lru ? "LRU" : "Random");
    printf("Walk latency: %d cycles per level\n", WALK_LATENCY);
    printf("=================================================================\n");
    printf("Access # | Tenant | VA         | VPN      | Hit/Miss | PA         | Cycles\n");
    printf("---------|--------|------------|----------|----------|------------|-------\n");

    while (fscanf(fp, "%u %x %c", &tenant_id, &va, &access_type) == 3) {
        if (tenant_id >= (uint32_t)n_tenants) {
            fprintf(stderr, "Error: Invalid tenant ID %u\n", tenant_id);
            continue;
        }

        uint32_t vpn = get_vpn(va);
        uint32_t ppn;
        uint32_t cycles = 1;

        if (tlb_lookup(vpn, tenant_id, &ppn)) {
            total_hits++;
        } else {
            uint32_t walk_cycles = 0;
            ppn = page_table_walk(va, tenant_id, &walk_cycles);
            tlb_insert(vpn, ppn, tenant_id);
            cycles = walk_cycles;
            total_misses++;
        }

        uint32_t offset = get_offset(va);
        uint32_t pa = (ppn << page_offset_bits) | offset;
        total_cycles += cycles;
        access_count++;

        if (access_count <= 20 || access_count % 10000 == 0) {
            printf("%8lu |   %2u   | 0x%08X | 0x%06X | %-8s | 0x%08X | %6u\n",
                   access_count, tenant_id, va, vpn,
                   (cycles == 1) ? "HIT" : "MISS", pa, cycles);
        }
    }

    printf("=================================================================\n");
    printf("Total accesses: %lu\n", access_count);
    printf("TLB hits: %lu (%.4f%%)\n", total_hits, 100.0 * total_hits / access_count);
    printf("TLB misses: %lu (%.4f%%)\n", total_misses, 100.0 * total_misses / access_count);
    printf("Total cycles: %lu\n", total_cycles);
    printf("Cycles lost to translation: %lu\n", total_misses * WALK_LATENCY * 
           ((pgsz == PAGE_SIZE_2M) ? 1 : 2));
    printf("=================================================================\n");

    fclose(fp);
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(int argc, char *argv[]) {
    srand((unsigned)time(NULL));

    /* If no args, run unit tests and manual verification */
    if (argc < 2) {
        printf("Running unit tests and manual trace verification...\n\n");

        int ok1 = test_addr_decomp();
        int ok2 = test_manual_trace();

        printf("\n=== SUMMARY ===\n");
        printf("Address decomposition test: %s\n", ok1 ? "PASS" : "FAIL");
        printf("Manual trace verification:  %s\n", ok2 ? "PASS" : "FAIL");

        return (ok1 && ok2) ? 0 : 1;
    }

    /* Full simulation mode */
    if (argc < 4) {
        printf("Usage: %s <trace_file> <page_size: 4K|2M> <num_tenants> [tlb_size] [tlb_assoc]\n", argv[0]);
        printf("   or: %s                          (run unit tests)\n", argv[0]);
        return 1;
    }

    const char *trace_file = argv[1];
    int pgsz = (strcmp(argv[2], "2M") == 0) ? PAGE_SIZE_2M : PAGE_SIZE_4K;
    int n_tenants = atoi(argv[3]);

    if (n_tenants < 1 || n_tenants > MAX_TENANTS) {
        fprintf(stderr, "Error: num_tenants must be between 1 and %d\n", MAX_TENANTS);
        return 1;
    }

    if (argc > 4) tlb_size = atoi(argv[4]);
    if (argc > 5) tlb_assoc = atoi(argv[5]);

    if (tlb_size < tlb_assoc || tlb_size > MAX_TLB_ENTRIES || (tlb_size % tlb_assoc) != 0) {
        fprintf(stderr, "Error: Invalid TLB geometry (size=%d, assoc=%d)\n", tlb_size, tlb_assoc);
        return 1;
    }

    run_simulation(trace_file, pgsz, n_tenants);
    return 0;
}
