#!/usr/bin/env python3
"""
gen_ghipss_trace.py

Generates a seeded, multi-tenant memory-access trace for the GhIPSS TLB simulator.
Each tenant has its own working set; combined working sets exceed single-tenant
TLB reach once tenant count passes a threshold.
"""

import argparse
import random

DEFAULT_SEED = 42
DEFAULT_NUM_TENANTS = 4
DEFAULT_ACCESSES_PER_TENANT = 1000
DEFAULT_WORKING_SET_PAGES = 64   # pages per tenant
PAGE_SIZE = 4096


def generate_trace(seed, num_tenants, accesses_per_tenant, working_set_pages, output_file):
    random.seed(seed)
    
    with open(output_file, 'w') as f:
        # Interleave tenant accesses to simulate concurrent execution
        for i in range(accesses_per_tenant):
            for tenant in range(num_tenants):
                # Each tenant's working set starts at a distinct virtual base
                tenant_base_page = tenant * 0x10000  # 64K-page gap between tenants
                page_offset = random.randint(0, working_set_pages - 1)
                va = (tenant_base_page + page_offset) * PAGE_SIZE
                # Add random byte offset within page
                va += random.randint(0, PAGE_SIZE - 1)
                access_type = random.choice(['R', 'W'])
                f.write(f"{tenant} 0x{va:08X} {access_type}\n")
    
    total_accesses = num_tenants * accesses_per_tenant
    total_working_set = num_tenants * working_set_pages * PAGE_SIZE
    print(f"Trace written to: {output_file}")
    print(f"  Seed: {seed}")
    print(f"  Tenants: {num_tenants}")
    print(f"  Accesses per tenant: {accesses_per_tenant}")
    print(f"  Total accesses: {total_accesses}")
    print(f"  Working set per tenant: {working_set_pages} pages ({working_set_pages * PAGE_SIZE} bytes)")
    print(f"  Combined working set: {total_working_set} bytes ({total_working_set / (1024*1024):.2f} MB)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate multi-tenant GhIPSS trace")
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED, help="Random seed for reproducibility")
    parser.add_argument("--tenants", type=int, default=DEFAULT_NUM_TENANTS, help="Number of tenants")
    parser.add_argument("--accesses", type=int, default=DEFAULT_ACCESSES_PER_TENANT, help="Accesses per tenant")
    parser.add_argument("--working-set", type=int, default=DEFAULT_WORKING_SET_PAGES, help="Working set pages per tenant")
    parser.add_argument("--output", type=str, default="ghipss_trace.txt", help="Output trace file")
    
    args = parser.parse_args()
    generate_trace(args.seed, args.tenants, args.accesses, args.working_set, args.output)
