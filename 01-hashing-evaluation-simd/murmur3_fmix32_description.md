# Murmur3-Based 64-to-32 Bit Hash Finalizer

This document provides a technical overview of the `hash_murmur3_32` function. The function is designed to take a 64-bit key, process it using the MurmurHash3 finalization mix (`fmix32`), and uniformly distribute the resulting output within a power-of-two range.

---

## Source Code

```cpp
inline uint32_t hash_murmur3_32(uint64_t key, uint32_t p_exp) {
    // Step 1: Folding
    uint32_t h = static_cast<uint32_t>(key ^ (key >> 32));
    
    // Step 2: Avalanche Effect (fmix32)
    h ^= h >> 16;
    h *= 0x85ebca6bU;
    h ^= h >> 13;
    h *= 0xc2b2ae35U;
    h ^= h >> 16;

    // Step 3: Range Selection
    return h >> (32 - p_exp);
}
```

---

## Functional Breakdown

The execution of the function is divided into three logical steps:

### 1. 64-bit to 32-bit Folding
> `uint32_t h = static_cast<uint32_t>(key ^ (key >> 32));`

The function accepts a 64-bit `key`. To reduce this to a 32-bit state without discarding the entropy of the upper 32 bits, a folding operation is applied. By shifting the key right by 32 bits and applying a bitwise XOR (`^`) with the original key, every bit of the 64-bit input actively influences the initial 32-bit state.

### 2. Avalanche Effect (Mixing)
> `h ^= h >> 16; h *= 0x85ebca6bU; ...`

This segment implements the standard `fmix32` block from the MurmurHash3 algorithm. Its primary purpose is to enforce a strict **avalanche effect**, ensuring that even a single-bit change in the original input results in an uncorrelated, pseudo-random distribution of the 32 output bits. 
* The operations alternate between XOR-shifts (to distribute bits across different positions) and multiplications by specific large magic constants (`0x85ebca6bU` and `0xc2b2ae35U`). 
* The multiplications intentionally cause integer overflows, heavily mixing the bits and eliminating potential input clustering.

### 3. Power-of-Two Range Selection
> `return h >> (32 - p_exp);`

The final step maps the 32-bit hash value to a partition index in `[0, P)`, 
where `P = 2^p_exp`. Shifting right by `32 - p_exp` retains exactly the top p_exp bits of the hash. This provides a uniform mapping into the partition space `[0, P)` without requiring an expensive modulo operation, while advantageously utilizing the most significant bits (which typically exhibit the strongest avalanche properties).