#ifndef __HEVCPARSER_H__
#define __HEVCPARSER_H__

#include <assert.h>
#include <err.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

#include "bit_reader_macros.h"
#include "h265_nalu_parser.h"
#include "h265_parser.h"

#define DCHECK assert
#define DVLOG(_n) \
    printf("\n@%d:", __LINE__); \
    std::cout

//#define DVLOG(_n) if (0) std::cout

#define NAL_UNIT_H265_VPS 32
#define NAL_UNIT_H265_SPS 33
#define NAL_UNIT_H265_PPS 34
#define NAL_UNIT_IDR_N_LP 0x14 // Coded slice segment of an IDR picture - slice_segment_layer_rbsp, VLC
#define NAL_UNIT_TRAIL_R 1

#define NAL_UNIT_H264_SEI 6
#define NAL_UNIT_H264_SPS 7
#define NAL_UNIT_H264_PPS 8

struct _DXVA_PicParams_HEVC;
struct _DXVA_Qmatrix_HEVC;
struct _VAPictureParameterBufferHEVC;
struct _VAIQMatrixBufferHEVC;

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanReverse64)
#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanForward64)

static inline uint32_t clz(uint32_t x) {
    unsigned long result;
    _BitScanReverse(&result, x);
    return (uint32_t) result;
}

static inline uint32_t clzll(uint64_t x) {
    unsigned long result;
    _BitScanReverse64(&result, x);
    return (uint32_t) result;
}

static inline uint32_t ctz(uint32_t x) {
    unsigned long result;
    _BitScanForward(&result, x);
    return (uint32_t) result;
}

static inline uint32_t ctzll(uint64_t x) {
    unsigned long result;
    _BitScanForward64(&result, x);
    return (uint32_t) result;
}

#else
#define clz __builtin_clz
#define clzll __builtin_clzll
#define ctz __builtin_ctz
#define ctzll __builtin_ctzll
#endif

class HEVCParser {

    typedef void (*decode_callback_t)(const uint8_t *bytes, size_t compressed_size, void *opaque);

    H264BitReader br_;
    H265NALU nalu;
    H265SliceHeader shdr1;

    H265VPS *vps = nullptr;
    H265SPS *sps = nullptr;
    H265PPS *pps = nullptr;

    bool is_hevc = true;

    enum Result {
        kOk,
        kInvalidStream, // error in stream
        kUnsupportedStream, // stream not supported by the parser
        kEOStream, // end of stream
        kMissingParameterSet,
    };


    template <typename T, int bits = sizeof(T) * 8>
    inline constexpr
        typename std::enable_if<std::is_unsigned<T>::value && sizeof(T) <= 8,
            int>::type
        CountLeadingZeroBits(T value) {
        static_assert(bits > 0, "invalid instantiation");
        return (value)
            ? bits == 64
                ? clzll(static_cast<uint64_t>(value))
                : clz(static_cast<uint32_t>(value)) - (32 - bits)
            : bits;
    }

    template <typename T, int bits = sizeof(T) * 8>
    inline constexpr
        typename std::enable_if<std::is_unsigned<T>::value && sizeof(T) <= 8,
            int>::type
        CountTrailingZeroBits(T value) {
        return (value) ? bits == 64
                ? ctzll(static_cast<uint64_t>(value))
                : ctz(static_cast<uint32_t>(value))
                       : bits;
    }

    constexpr int Log2Floor(uint32_t n) {
        return 31 - CountLeadingZeroBits(n);
    }

    // Returns the integer i such as 2^(i-1) < n <= 2^i.
    constexpr int Log2Ceiling(uint32_t n) {
        // When n == 0, we want the function to return -1.
        // When n == 0, (n - 1) will underflow to 0xFFFFFFFF, which is
        // why the statement below starts with (n ? 32 : -1).
        return (n ? 32 : -1) - CountLeadingZeroBits(n - 1);
    }

    // From Table 7-6.
    static constexpr uint8_t kDefaultScalingListSize1To3Matrix0To2[] = {
        // clang-format off
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 16, 17, 16, 17, 18,
    17, 18, 18, 17, 18, 21, 19, 20, 21, 20, 19, 21, 24, 22, 22, 24,
    24, 22, 22, 24, 25, 25, 27, 30, 27, 25, 25, 29, 31, 35, 35, 31,
    29, 36, 41, 44, 41, 36, 47, 54, 54, 47, 65, 70, 65, 88, 88, 115, };
    // clang-format on

    static constexpr uint8_t kDefaultScalingListSize1To3Matrix3To5[] = {
        // clang-format off
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 18,
    18, 18, 18, 18, 18, 20, 20, 20, 20, 20, 20, 20, 24, 24, 24, 24,
    24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 28, 28, 28, 28, 28,
    28, 33, 33, 33, 33, 33, 41, 41, 41, 41, 54, 54, 54, 71, 71, 91, };
    // clang-format on

    // VUI parameters: Table E-1 "Interpretation of sample aspect ratio indicator"
    static constexpr int kTableSarWidth[] = { 0, 1, 12, 10, 16, 40, 24, 20, 32,
        80, 18, 15, 64, 160, 4, 3, 2 };
    static constexpr int kTableSarHeight[] = { 0, 1, 11, 11, 11, 33, 11, 11, 11,
        33, 11, 11, 33, 99, 3, 2, 1 };
    // static_assert(std::size(kTableSarWidth) == std::size(kTableSarHeight),
    //"sar tables must have the same size");

    Result ParseAndIgnoreHrdParameters(bool common_inf_present_flag, int max_num_sub_layers_minus1);
    Result ParseAndIgnoreSubLayerHrdParameters(int cpb_cnt, bool sub_pic_hrd_params_present_flag);
    Result ParsePPS(H265SPS *sps, H265PPS **ppps);
    Result ParsePredWeightTable(const H265SPS &sps, const H265SliceHeader &shdr, H265PredWeightTable *pred_weight_table);
    Result ParseProfileTierLevel(bool profile_present, int max_num_sub_layers_minus1, H265ProfileTierLevel *profile_tier_level);
    Result ParseRefPicListsModifications(const H265SliceHeader &shdr, H265RefPicListsModifications *rpl_mod);
    Result ParseSPS(H265SPS **psps);
    Result ParseScalingListData(H265ScalingListData *scaling_list_data);
    Result ParseSliceHeader(const H265NALU &nalu, H265SliceHeader *shdr, H265SliceHeader *prior_shdr);
    Result ParseSliceHeaderForPictureParameterSets(const H265NALU &nalu, int *pps_id);
    Result ParseStRefPicSet(int st_rps_idx, const H265SPS &sps, H265StRefPicSet *st_ref_pic_set, bool is_slice_hdr = false);
    Result ParseVPS(H265VPS **pvps);
    Result ParseVuiParameters(const H265SPS &sps, H265VUIParameters *vui);
    void FillInDefaultScalingListData(H265ScalingListData *scaling_list_data, int size_id, int matrix_id);


public:
    bool Parse(const uint8_t *bytes, size_t compressed_size, decode_callback_t cb, void *opaque);
    void FillDXVA(_DXVA_PicParams_HEVC *pp, _DXVA_Qmatrix_HEVC *pim);
    void FillVA(_VAPictureParameterBufferHEVC *pp, _VAIQMatrixBufferHEVC *pim);
    void GetDimensions(int *pw, int *ph);
    void GetUnpaddedDimensions(int *pw, int *ph);

}; // HEVCParser

#endif /* __HEVCPARSER_H__ */
