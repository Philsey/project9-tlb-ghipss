# Requirements Specification — Project 9: Translating Trust

## 1. Functional Requirements

### 1.1 Two-Level Page-Table Walker
| ID | Requirement | Priority |
|---|---|---|
| FR-1.1 | Decompose VA into PD index, PT index, and page offset | Mandatory |
| FR-1.2 | Traverse PD (L1) then PT (L2) on TLB miss | Mandatory |
| FR-1.3 | Concatenate PPN with offset to produce PA | Mandatory |
| FR-1.4 | Charge configurable per-level walk latency (miss = 2× latency) | Mandatory |
| FR-1.5 | Support 4 KB (12-bit offset) and 2 MB (21-bit offset) pages | Mandatory |

### 1.2 TLB
| ID | Requirement | Priority |
|---|---|---|
| FR-2.1 | Cache VA→PA translations | Mandatory |
| FR-2.2 | Configurable size, associativity, replacement policy (LRU/random) | Mandatory |
| FR-2.3 | Report per-access hit/miss and running miss rate | Mandatory |
| FR-2.4 | Tag entries by Tenant ID to prevent cross-tenant leakage | Mandatory |
| FR-2.5 | On tenant switch, flush or match tenant ID consistently | Mandatory |

### 1.3 Multi-Tenant Isolation
| ID | Requirement | Priority |
|---|---|---|
| FR-3.1 | Each tenant has independent page-table root | Mandatory |
| FR-3.2 | Route translation through correct root per access | Mandatory |
| FR-3.3 | No cross-tenant translation leakage under any circumstance | Mandatory |
| FR-3.4 | Adversarial test: overlapping VAs across tenants map to distinct PAs | Mandatory |

### 1.4 Trace & Workload
| ID | Requirement | Priority |
|---|---|---|
| FR-4.1 | Ingest trace: `&lt;Tenant_ID&gt; &lt;VA_Hex&gt; &lt;Access_Type&gt;` | Mandatory |
| FR-4.2 | Seeded, reproducible trace generation | Mandatory |
| FR-4.3 | Combined working set exceeds single-tenant TLB reach at threshold | Mandatory |

### 1.5 Huge-Page Extension
| ID | Requirement | Priority |
|---|---|---|
| FR-5.1 | 2 MB page configuration with fewer TLB entries for same reach | Mandatory |
| FR-5.2 | Dynamic offset bit-width adjustment (12 vs. 21) | Mandatory |
| FR-5.3 | Quantify reach improvement: reach = entries × page_size | Mandatory |

### 1.6 Analysis
| ID | Requirement | Priority |
|---|---|---|
| FR-6.1 | Compute TLB miss rate per configuration | Mandatory |
| FR-6.2 | Compute cycles lost: miss_rate × walk_cycles | Mandatory |
| FR-6.3 | Per-tenant working-set vs. TLB-reach comparison | Mandatory |
| FR-6.4 | Tabulated results and miss-rate vs. tenant-count plots | Mandatory |

### 1.7 Level 3 Extensions (Optional)
| ID | Requirement | Priority |
|---|---|---|
| FR-7.1 | Two-level TLB (L1 + L2) with miss-rate improvement measurement | Optional |
| FR-7.2 | Simplified direct segment for large contiguous region | Optional |
| FR-7.3 | TLB partitioning scheme under adversarial thrashing | Optional |

## 2. Non-Functional Requirements

| ID | Requirement | Priority |
|---|---|---|
| NFR-1.1 | Hand-computed oracle verification before full-trace runs | Mandatory |
| NFR-1.2 | Address-decomposition unit test as first artifact | Mandatory |
| NFR-1.3 | Adversarial cross-tenant isolation test | Mandatory |
| NFR-2.1 | Runtime-configurable parameters (no recompilation) | Mandatory |
| NFR-3.1 | Log full config (seed, TLB geometry, page size, tenants) per run | Mandatory |
| NFR-3.2 | Bit-identical results across repeated executions | Mandatory |
| NFR-4.1 | Handle 100K accesses across 16 tenants without OOM | Mandatory |
| NFR-5.1 | Compile clean under `gcc -Wall -Wextra` | Mandatory |
| NFR-5.2 | Remove all TODO markers from starter code | Mandatory |
| NFR-5.3 | Meaningful incremental commit history | Mandatory |

## 3. Input / Output Format

**Input trace:**
