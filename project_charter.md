# Project 9: Translating Trust — Project Charter

## 1. Project Identification

| Field | Details |
|-------|---------|
| **Project ID** | P9-TLB-GhIPSS |
| **Project Title** | Translating Trust: TLB and Virtual Memory Design for Multi-Tenant GhIPSS Workloads |
| **Course Context** | Computer Architecture Portfolio — Week 1 Deliverables |
| **Anchor Papers** | Basu et al. (ISCA '13); Hennessy & Patterson (CACM '19) |
| **Primary Languages** | C/C++, Python, MATLAB |

## 2. Team Structure

| Role | Responsibility |
|------|----------------|
| Architecture Lead | Page-table walker design, TLB core, address decomposition |
| C/C++ Lead | Multi-tenant isolation logic, memory management, unit tests |
| Python/Analysis Lead | Trace generation, miss-rate analysis, visualization |
| Integration/Test Lead | Validation pipeline, cross-tenant adversarial tests, reproducibility |

## 3. Scope

### 3.1 In-Scope
- Two-level page-table walker with configurable per-level walk latency
- Configurable TLB (size, associativity, LRU/random replacement)
- Multi-tenant address-space isolation with tenant-ID tagging
- 4 KB and 2 MB huge-page support with correct address decomposition
- Seeded multi-tenant GhIPSS-style trace generation
- TLB miss-rate and cycles-lost measurement and visualization
- Hand-computed validation oracle for translation correctness
- Level-3 extensions: two-level TLB, direct segment, TLB partitioning

### 3.2 Out-of-Scope
- Full OS kernel integration or system-call interception
- FPGA/ASIC hardware implementation
- GPU or accelerator memory translation
- gem5 or industrial simulator integration
- Real-time networked GhIPSS deployment
- OS-level huge-page management (THP, defragmentation)

## 4. Timeline

| Week | Focus | Key Deliverables |
|------|-------|------------------|
| 1 | Literature, foundations, specification | Paper-review slides, charter, requirements, architecture diagram, repo, seeded trace, hand-computed example |
| 2 | Design & initial implementation | Design spec, two-level walker, multi-tenant isolation, unit tests, manual trace verification |
| 3 | Integration, experimentation, optimisation | Huge-page config, full sweep, tables/plots, innovation prototype |
| 4 | Validation, reporting, defence | Final code, technical report, presentation, live demo, contribution statements, peer assessment |

## 5. Risk Plan

| Risk | Mitigation |
|------|------------|
| Off-by-one address-decomposition bug | Build unit test as first artifact before any full-trace run |
| Cross-tenant isolation failure | Adversarial test with overlapping VAs across tenants |
| Time constraint | Prioritize mandatory requirements; defer Level-3 extensions if needed |
| Trace reproducibility issues | Log seed and all parameters per run; version-control trace generator |

## 6. Success Criteria

1. All translations match hand-computed oracle across all tested configurations
2. Zero cross-tenant address leakage under adversarial test
3. TLB miss-rate trend correctly explained via reach arithmetic
4. Quantified huge-page benefit (2 MB vs. 4 KB) with reach-based justification
5. Full experimental configuration recorded for every run
