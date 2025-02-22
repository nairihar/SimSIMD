/**
 *  @brief      SIMD-accelerated Binary Similarity Measures.
 *  @author     Ash Vardanian
 *  @date       July 1, 2023
 *
 *  Contains:
 *  - Hamming distance
 *  - Jaccard similarity (Tanimoto coefficient)
 *
 *  For datatypes:
 *  - 8-bit bitsets
 *
 *  For hardware architectures:
 *  - Arm (NEON, SVE)
 *  - x86 (AVX512)
 *
 *  x86 intrinsics: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
 *  Arm intrinsics: https://developer.arm.com/architectures/instruction-sets/intrinsics/
 */

#pragma once
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

inline static unsigned char simsimd_popcount_b8(simsimd_b8_t x) {
    static unsigned char lookup_table[] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, //
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};
    return lookup_table[x];
}

inline static simsimd_f32_t simsimd_serial_b8_hamming( //
    simsimd_b8_t const* a, simsimd_b8_t const* b, simsimd_size_t n_words) {
    simsimd_i32_t differences = 0;
    for (simsimd_size_t i = 0; i != n_words; ++i)
        differences += simsimd_popcount_b8(a[i] ^ b[i]);
    return (simsimd_f32_t)differences;
}

inline static simsimd_f32_t simsimd_serial_b8_jaccard( //
    simsimd_b8_t const* a, simsimd_b8_t const* b, simsimd_size_t n_words) {
    simsimd_i32_t intersection = 0, union_ = 0;
    for (simsimd_size_t i = 0; i != n_words; ++i)
        intersection += simsimd_popcount_b8(a[i] & b[i]), union_ += simsimd_popcount_b8(a[i] | b[i]);
    return (union_ != 0) ? 1 - (simsimd_f32_t)intersection / (simsimd_f32_t)union_ : 0;
}

#if SIMSIMD_TARGET_ARM
#if SIMSIMD_TARGET_ARM_NEON

__attribute__((target("+simd"))) //
inline static simsimd_f32_t
simsimd_neon_b8_hamming(simsimd_b8_t const* a, simsimd_b8_t const* b, simsimd_size_t n_words) {
    simsimd_i32_t differences = 0;
    simsimd_size_t i = 0;
    for (; i + 16 <= n_words; i += 16) {
        uint8x16_t a_first = vld1q_u8(a + i);
        uint8x16_t b_first = vld1q_u8(b + i);
        differences += vaddvq_u8(vcntq_u8(veorq_u8(a_first, b_first)));
    }
    // Handle the tail
    for (; i != n_words; ++i)
        differences += simsimd_popcount_b8(a[i] ^ b[i]);
    return (simsimd_f32_t)differences;
}

__attribute__((target("+simd"))) //
inline static simsimd_f32_t
simsimd_neon_b8_jaccard(simsimd_b8_t const* a, simsimd_b8_t const* b, simsimd_size_t n_words) {
    simsimd_i32_t intersection = 0, union_ = 0;
    simsimd_size_t i = 0;
    for (; i + 16 <= n_words; i += 16) {
        uint8x16_t a_first = vld1q_u8(a + i);
        uint8x16_t b_first = vld1q_u8(b + i);
        intersection += vaddvq_u8(vcntq_u8(vandq_u8(a_first, b_first)));
        union_ += vaddvq_u8(vcntq_u8(vorrq_u8(a_first, b_first)));
    }
    // Handle the tail
    for (; i != n_words; ++i)
        intersection += simsimd_popcount_b8(a[i] & b[i]), union_ += simsimd_popcount_b8(a[i] | b[i]);
    return (union_ != 0) ? 1 - (simsimd_f32_t)intersection / (simsimd_f32_t)union_ : 0;
}

#endif // SIMSIMD_TARGET_ARM_NEON

#if SIMSIMD_TARGET_ARM_SVE

__attribute__((target("+sve"))) //
inline static simsimd_f32_t
simsimd_sve_b8_hamming(simsimd_b8_t const* a, simsimd_b8_t const* b, simsimd_size_t n_words) {
    simsimd_size_t i = 0;
    simsimd_i32_t differences = 0;
    do {
        svbool_t pg_vec = svwhilelt_b8((unsigned int)i, (unsigned int)n_words);
        svuint8_t a_vec = svld1_u8(pg_vec, a + i);
        svuint8_t b_vec = svld1_u8(pg_vec, b + i);
        differences += svaddv_u8(svptrue_b8(), svcnt_u8_x(svptrue_b8(), sveor_u8_m(svptrue_b8(), a_vec, b_vec)));
        i += svcntb();
    } while (i < n_words);
    return (simsimd_f32_t)differences;
}

__attribute__((target("+sve"))) //
inline static simsimd_f32_t
simsimd_sve_b8_jaccard(simsimd_b8_t const* a, simsimd_b8_t const* b, simsimd_size_t n_words) {
    simsimd_size_t i = 0;
    simsimd_i32_t intersection = 0, union_ = 0;
    do {
        svbool_t pg_vec = svwhilelt_b8((unsigned int)i, (unsigned int)n_words);
        svuint8_t a_vec = svld1_u8(pg_vec, a + i);
        svuint8_t b_vec = svld1_u8(pg_vec, b + i);
        intersection += svaddv_u8(svptrue_b8(), svcnt_u8_x(svptrue_b8(), svand_u8_m(svptrue_b8(), a_vec, b_vec)));
        union_ += svaddv_u8(svptrue_b8(), svcnt_u8_x(svptrue_b8(), svorr_u8_m(svptrue_b8(), a_vec, b_vec)));
        i += svcntb();
    } while (i < n_words);
    return (union_ != 0) ? 1 - (simsimd_f32_t)intersection / (simsimd_f32_t)union_ : 0;
}

#endif // SIMSIMD_TARGET_ARM_SVE

#endif // SIMSIMD_TARGET_ARM

#if SIMSIMD_TARGET_X86

#if SIMSIMD_TARGET_X86_AVX512

__attribute__((target("avx512vpopcntdq,avx512vl,avx512bw,avx512f,bmi2"))) //
inline static simsimd_f32_t
simsimd_avx512_b8_hamming(simsimd_b8_t const* a, simsimd_b8_t const* b, simsimd_size_t n_words) {
    __m512i differences_vec = _mm512_setzero_si512();
    __m512i a_vec, b_vec;

simsimd_avx512_b8_hamming_cycle:
    if (n_words < 64) {
        __mmask64 mask = _bzhi_u64(0xFFFFFFFFFFFFFFFF, n_words);
        a_vec = _mm512_maskz_loadu_epi8(mask, a);
        b_vec = _mm512_maskz_loadu_epi8(mask, b);
        n_words = 0;
    } else {
        a_vec = _mm512_loadu_epi8(a);
        b_vec = _mm512_loadu_epi8(b);
        a += 64, b += 64, n_words -= 64;
    }
    __m512i xor_vec = _mm512_xor_si512(a_vec, b_vec);
    differences_vec = _mm512_add_epi64(differences_vec, _mm512_popcnt_epi64(xor_vec));
    if (n_words)
        goto simsimd_avx512_b8_hamming_cycle;

    simsimd_size_t differences = _mm512_reduce_add_epi64(differences_vec);
    return (simsimd_f32_t)differences;
}

__attribute__((target("avx512vpopcntdq,avx512vl,avx512bw,avx512f,bmi2"))) //
inline static simsimd_f32_t
simsimd_avx512_b8_jaccard(simsimd_b8_t const* a, simsimd_b8_t const* b, simsimd_size_t n_words) {
    __m512i intersection_vec = _mm512_setzero_si512(), union_vec = _mm512_setzero_si512();
    __m512i a_vec, b_vec;

simsimd_avx512_b8_jaccard_cycle:
    if (n_words < 64) {
        __mmask64 mask = _bzhi_u64(0xFFFFFFFFFFFFFFFF, n_words);
        a_vec = _mm512_maskz_loadu_epi8(mask, a);
        b_vec = _mm512_maskz_loadu_epi8(mask, b);
        n_words = 0;
    } else {
        a_vec = _mm512_loadu_epi8(a);
        b_vec = _mm512_loadu_epi8(b);
        a += 64, b += 64, n_words -= 64;
    }
    __m512i and_vec = _mm512_and_si512(a_vec, b_vec);
    __m512i or_vec = _mm512_or_si512(a_vec, b_vec);
    intersection_vec = _mm512_add_epi64(intersection_vec, _mm512_popcnt_epi64(and_vec));
    union_vec = _mm512_add_epi64(union_vec, _mm512_popcnt_epi64(or_vec));
    if (n_words)
        goto simsimd_avx512_b8_jaccard_cycle;

    simsimd_size_t intersection = _mm512_reduce_add_epi64(intersection_vec),
                   union_ = _mm512_reduce_add_epi64(union_vec);
    return (union_ != 0) ? 1 - (simsimd_f32_t)intersection / (simsimd_f32_t)union_ : 0;
}

#endif // SIMSIMD_TARGET_X86_AVX512
#endif // SIMSIMD_TARGET_X86

#undef popcount64

#ifdef __cplusplus
}
#endif