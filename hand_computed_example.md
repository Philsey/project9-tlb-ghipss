
# Hand-Computed TLB-Reach Worked Example

## Configuration

| Parameter | Value |
|-----------|-------|
| TLB entries | 64 |
| Associativity | 4-way |
| Replacement policy | LRU |
| Per-tenant working set | 1 MB |
| Number of tenants | 4 |
| Page sizes tested | 4 KB and 2 MB |

## 1. TLB Reach Arithmetic

### 4 KB Pages
```
TLB Reach = 64 entries × 4 KB = 64 × 4096 = 262,144 bytes = 256 KB
```

### 2 MB Huge Pages
```
TLB Reach = 64 entries × 2 MB = 64 × 2,097,152 = 134,217,728 bytes = 128 MB
```

## 2. Working Set vs. TLB Reach

| Metric | 4 KB Pages | 2 MB Pages |
|--------|-----------|-----------|
| TLB Reach | 256 KB | 128 MB |
| Combined working set (4 tenants) | 4 MB | 4 MB |
| Reach covers combined WS? | **No** | **Yes** |
| Predicted miss rate | **High** (capacity misses guaranteed) | **Low** (fits comfortably) |

## 3. Address Decomposition (4 KB Mode)

**VA = 0x00401ABC**

```
PD Index  = bits 31:22 = (0x00401ABC >> 22) & 0x3FF = 1
PT Index  = bits 21:12 = (0x00401ABC >> 12) & 0x3FF = 1
Offset    = bits 11:0  = 0x00401ABC & 0xFFF = 0xABC
```

## 4. Translation Oracle (Two Tenants, 4 KB)

| Tenant | Page Table Root | VA 0x00401000 maps to |
|--------|-----------------|----------------------|
| 0 | Root 0 | Physical Frame 0x200 |
| 1 | Root 1 | Physical Frame 0x800 |

### Translation 1: Tenant 0, VA = 0x00401ABC
- VPN = 0x401, Offset = 0xABC
- PD[1] → PT Base X, PT[1] → PPN = 0x200
- **PA = (0x200 << 12) | 0xABC = 0x200ABC**

### Translation 2: Tenant 1, VA = 0x00401ABC
- VPN = 0x401, Offset = 0xABC
- PD[1] → PT Base Y, PT[1] → PPN = 0x800
- **PA = (0x800 << 12) | 0xABC = 0x800ABC**

**Isolation verified:** Same VA, different PAs. No cross-tenant leakage.

## 5. Huge-Page Mode (2 MB)

**VA = 0x00401ABC**

```
PD Index = bits 31:21 = (0x00401ABC >> 21) & 0x7FF = 2
Offset   = bits 20:0  = 0x00401ABC & 0x1FFFFF = 0x1ABC
```

- PD[2] → PPN = 0x200 (points directly to 2 MB region)
- **PA = (0x200 << 21) | 0x1ABC = 0x40001ABC**
- Only **1 memory reference** (PD only, PT bypassed)

## 6. Validation Oracle for Simulator

| Access # | Tenant | VA | Expected PA (4K) | Expected PA (2M) | Expected Result |
|----------|--------|----|------------------|------------------|-----------------|
| 0 | 0 | 0x00401000 | 0x200000 | 0x40000000 | MISS (cold) |
| 1 | 0 | 0x00401004 | 0x200004 | 0x40000004 | HIT |
| 2 | 1 | 0x00401000 | 0x800000 | 0x48000000 | MISS (cold, different tenant) |
| 3 | 0 | 0x00401000 | 0x200000 | 0x40000000 | HIT (still in TLB) |

**Simulator output must match this oracle exactly before full-trace experiments are trusted.**


