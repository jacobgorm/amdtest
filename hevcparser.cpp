// The majority of the code in this file is
//
// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H265 Annex-B video stream parser,
// but it only handles NALU parsing.
//
// The code that does not overlap with Chromium is Copyright 2023 Jamscape ApS.

#undef NDEBUG
#include <algorithm>

#include "hevcparser.h"

#include <windows.h>
#include <dxva.h>

H265NALU::H265NALU() {
    memset(this, 0, sizeof(*this));
}

H265PPS::H265PPS() {
    memset(this, 0, sizeof(*this));
}

H265SPS::H265SPS() {
    memset(this, 0, sizeof(*this));
}

H265VPS::H265VPS() {
    memset(this, 0, sizeof(*this));
}

H265StRefPicSet::H265StRefPicSet() {
    memset(this, 0, sizeof(*this));
}

H265VUIParameters::H265VUIParameters() {
    memset(this, 0, sizeof(*this));
}

H265ScalingListData::H265ScalingListData() {
    memset(this, 0, sizeof(*this));
}

H265ProfileTierLevel::H265ProfileTierLevel() {
    memset(this, 0, sizeof(*this));
}

H265SliceHeader::H265SliceHeader() {
    memset(this, 0, sizeof(*this));
}

H265PredWeightTable::H265PredWeightTable() {
    memset(this, 0, sizeof(*this));
}

H265RefPicListsModifications::H265RefPicListsModifications() {
    memset(this, 0, sizeof(*this));
}

bool H265SliceHeader::IsISlice() const {
    return slice_type == kSliceTypeI;
}

bool H265SliceHeader::IsPSlice() const {
    return slice_type == kSliceTypeP;
}

bool H265SliceHeader::IsBSlice() const {
    return slice_type == kSliceTypeB;
}

int H265ProfileTierLevel::GetMaxLumaPs() const {
    // From Table A.8 - General tier and level limits.
    // |general_level_idc| is 30x the actual level.
    if (general_level_idc <= 30) { // level 1
        return 36864;
    }
    if (general_level_idc <= 60) { // level 2
        return 122880;
    }
    if (general_level_idc <= 63) { // level 2.1
        return 245760;
    }
    if (general_level_idc <= 90) { // level 3
        return 552960;
    }
    if (general_level_idc <= 93) { // level 3.1
        return 983040;
    }
    if (general_level_idc <= 123) { // level 4, 4.1
        return 2228224;
    }
    if (general_level_idc <= 156) { // level 5, 5.1, 5.2
        return 8912896;
    }
    // level 6, 6.1, 6.2 - beyond that there's no actual limit.
    return 35651584;
}

size_t H265ProfileTierLevel::GetDpbMaxPicBuf() const {
    // From A.4.2 - Profile-specific level limits for the video profiles.
    // If sps_curr_pic_ref_enabled_flag is required to be zero, than this is 6
    // otherwise it is 7.
    return (general_profile_idc >= kProfileIdcMain && general_profile_idc <= kProfileIdcHighThroughput)
        ? 6
        : 7;
}

#define ARG_SEL(_1, _2, NAME, ...) NAME
#define SPS_TO_PP1(a) pp->a = sps->a;
#define SPS_TO_PPEXT(a) pp->a = sps->a;
#define SPS_TO_PP2(a, b) pp->a = sps->b;
#define SPS_TO_PP(...) ARG_SEL(__VA_ARGS__, SPS_TO_PP2, SPS_TO_PP1)(__VA_ARGS__)

#define PPS_TO_PPEXT(a) pp->a = pps->a;
#define PPS_TO_PP(a) pp->a = pps->a

#ifdef USE_LIBVA
static void InitVAPicture(VAPictureHEVC *va_pic) {
    va_pic->picture_id = VA_INVALID_ID;
    va_pic->flags = VA_PICTURE_HEVC_INVALID;
}
#endif

void HEVCParser::GetDimensions(int *pw, int *ph) {
    if (!sps) {
        errx(1, "%s: no SPS", __PRETTY_FUNCTION__);
    }
    *pw = sps->pic_width_in_luma_samples;
    *ph = sps->pic_height_in_luma_samples;
}

void HEVCParser::GetUnpaddedDimensions(int *pw, int *ph) {
    if (!sps) {
        errx(1, "%s: no SPS", __PRETTY_FUNCTION__);
    }
#if 0
    printf("adjust offsets %d %d %d %d\n",
        sps->conf_win_left_offset,
        sps->conf_win_right_offset,
        sps->conf_win_top_offset,
        sps->conf_win_bottom_offset);
#endif

    int width_crop = sps->conf_win_left_offset + sps->conf_win_right_offset;
    width_crop += sps->vui_parameters.def_disp_win_left_offset;
    width_crop += sps->vui_parameters.def_disp_win_right_offset;
    width_crop *= sps->sub_width_c;

    // base::CheckedNumeric<int>
    int height_crop = sps->conf_win_top_offset + sps->conf_win_bottom_offset;
    height_crop += sps->vui_parameters.def_disp_win_top_offset;
    height_crop += sps->vui_parameters.def_disp_win_bottom_offset;
    height_crop *= sps->sub_height_c;

    *pw = sps->pic_width_in_luma_samples - width_crop;
    *ph = sps->pic_height_in_luma_samples - height_crop;
}


void HEVCParser::FillDXVA(DXVA_PicParams_HEVC *pp, DXVA_Qmatrix_HEVC *pim) {
    memset(pp, 0, sizeof(*pp));
    memset(pim, 0, sizeof(*pim));

    // Refer to formula 7-14 and 7-16 of HEVC spec.
    int min_cb_log2_size_y = sps->log2_min_luma_coding_block_size_minus3 + 3;
    pp->PicWidthInMinCbsY = sps->pic_width_in_luma_samples >> min_cb_log2_size_y;
    pp->PicHeightInMinCbsY = sps->pic_height_in_luma_samples >> min_cb_log2_size_y;
    // wFormatAndSequenceInfoFlags from SPS
    SPS_TO_PP(chroma_format_idc);
    SPS_TO_PP(separate_colour_plane_flag);
    SPS_TO_PP(bit_depth_luma_minus8);
    SPS_TO_PP(bit_depth_chroma_minus8);
    SPS_TO_PP(log2_max_pic_order_cnt_lsb_minus4);

    if (sps->profile_tier_level.general_profile_idc == 4) {
        // bool is_rext = true;
    }
    // HEVC DXVA spec does not clearly state which slot
    // in sps->sps_max_dec_pic_buffering_minus1 should
    // be used here. However section A4.1 of HEVC spec
    // requires the slot of highest tid to be used for
    // indicating the maximum DPB size if level is not
    // 8.5.
    int highest_tid = sps->sps_max_sub_layers_minus1;
    pp->sps_max_dec_pic_buffering_minus1 = sps->sps_max_dec_pic_buffering_minus1[highest_tid];

    SPS_TO_PP(log2_min_luma_coding_block_size_minus3);
    SPS_TO_PP(log2_diff_max_min_luma_coding_block_size);

    // DXVA spec names them differently with HEVC spec.
    //#define SPS_TO_PP2(a, b) pp->a = sps->b;
    pp->log2_min_transform_block_size_minus2 = sps->log2_min_luma_transform_block_size_minus2;
    pp->log2_diff_max_min_transform_block_size = sps->log2_diff_max_min_luma_transform_block_size;

    SPS_TO_PP(max_transform_hierarchy_depth_inter);
    SPS_TO_PP(max_transform_hierarchy_depth_intra);
    SPS_TO_PP(num_short_term_ref_pic_sets);
    SPS_TO_PP(num_long_term_ref_pics_sps);

    // dwCodingParamToolFlags extracted from SPS
    SPS_TO_PP(scaling_list_enabled_flag);
    SPS_TO_PP(amp_enabled_flag);
    SPS_TO_PP(sample_adaptive_offset_enabled_flag);
    SPS_TO_PP(pcm_enabled_flag);

    if (sps->pcm_enabled_flag) {
        SPS_TO_PP(pcm_sample_bit_depth_luma_minus1);
        SPS_TO_PP(pcm_sample_bit_depth_chroma_minus1);
        SPS_TO_PP(log2_min_pcm_luma_coding_block_size_minus3);
        SPS_TO_PP(log2_diff_max_min_pcm_luma_coding_block_size);
        SPS_TO_PP(pcm_loop_filter_disabled_flag);
    }

    pp->NoPicReorderingFlag = (sps->sps_max_num_reorder_pics[highest_tid] == 0) ? 1 : 0;

    SPS_TO_PP(long_term_ref_pics_present_flag);
    SPS_TO_PP(sps_temporal_mvp_enabled_flag);
    SPS_TO_PP(strong_intra_smoothing_enabled_flag);

    if (sps->sps_range_extension_flag) {
        errx(1, "sps_range_extension_flag was set!");
#if 0
        SPS_TO_PPEXT(transform_skip_rotation_enabled_flag);
        SPS_TO_PPEXT(transform_skip_context_enabled_flag);
        SPS_TO_PPEXT(implicit_rdpcm_enabled_flag);
        SPS_TO_PPEXT(explicit_rdpcm_enabled_flag);
        SPS_TO_PPEXT(extended_precision_processing_flag);
        SPS_TO_PPEXT(intra_smoothing_disabled_flag);
        SPS_TO_PPEXT(high_precision_offsets_enabled_flag);
        SPS_TO_PPEXT(persistent_rice_adaptation_enabled_flag);
        SPS_TO_PPEXT(cabac_bypass_alignment_enabled_flag);
#endif
    }

    PPS_TO_PP(num_ref_idx_l0_default_active_minus1);
    PPS_TO_PP(num_ref_idx_l1_default_active_minus1);
    PPS_TO_PP(init_qp_minus26);

    // dwCodingParamToolFlags from PPS
    PPS_TO_PP(dependent_slice_segments_enabled_flag);
    PPS_TO_PP(output_flag_present_flag);
    PPS_TO_PP(num_extra_slice_header_bits);
    PPS_TO_PP(sign_data_hiding_enabled_flag);
    PPS_TO_PP(cabac_init_present_flag);

    // dwCodingSettingPicturePropertyFlags from PPS
    PPS_TO_PP(constrained_intra_pred_flag);
    PPS_TO_PP(transform_skip_enabled_flag);
    PPS_TO_PP(cu_qp_delta_enabled_flag);
    PPS_TO_PP(pps_slice_chroma_qp_offsets_present_flag);
    PPS_TO_PP(weighted_pred_flag);
    PPS_TO_PP(weighted_bipred_flag);
    PPS_TO_PP(transquant_bypass_enabled_flag);
    PPS_TO_PP(tiles_enabled_flag);
    PPS_TO_PP(entropy_coding_sync_enabled_flag);
    PPS_TO_PP(uniform_spacing_flag);
    if (pps->tiles_enabled_flag) {
        PPS_TO_PP(loop_filter_across_tiles_enabled_flag);
    }
    PPS_TO_PP(pps_loop_filter_across_slices_enabled_flag);
    PPS_TO_PP(deblocking_filter_override_enabled_flag);
    PPS_TO_PP(pps_deblocking_filter_disabled_flag);
    PPS_TO_PP(lists_modification_present_flag);
    PPS_TO_PP(slice_segment_header_extension_present_flag);

    PPS_TO_PP(pps_cb_qp_offset);
    PPS_TO_PP(pps_cr_qp_offset);
    if (pps->tiles_enabled_flag) {
        PPS_TO_PP(num_tile_columns_minus1);
        PPS_TO_PP(num_tile_rows_minus1);
        if (!pps->uniform_spacing_flag) {
            for (int i = 0; i <= pps->num_tile_columns_minus1; i++) {
                PPS_TO_PP(column_width_minus1[i]);
            }
            for (int j = 0; j <= pps->num_tile_rows_minus1; j++) {
                PPS_TO_PP(row_height_minus1[j]);
            }
        }
    }
    PPS_TO_PP(diff_cu_qp_delta_depth);
    PPS_TO_PP(pps_beta_offset_div2);
    PPS_TO_PP(pps_tc_offset_div2);
    PPS_TO_PP(log2_parallel_merge_level_minus2);
    if (pps->pps_range_extension_flag) {
        errx(1, "pps_range_extension_flag was set!");
#if 0
        PPS_TO_PPEXT(cross_component_prediction_enabled_flag);
        PPS_TO_PPEXT(chroma_qp_offset_list_enabled_flag);
        if (pps->chroma_qp_offset_list_enabled_flag) {
            PPS_TO_PPEXT(diff_cu_chroma_qp_offset_depth);
            PPS_TO_PPEXT(chroma_qp_offset_list_len_minus1);
            for (int i = 0; i <= pps->chroma_qp_offset_list_len_minus1; i++) {
                PPS_TO_PPEXT(cb_qp_offset_list[i]);
                PPS_TO_PPEXT(cr_qp_offset_list[i]);
            }
        }
        PPS_TO_PPEXT(log2_sao_offset_scale_luma);
        PPS_TO_PPEXT(log2_sao_offset_scale_chroma);
        if (pps->transform_skip_enabled_flag) {
            PPS_TO_PPEXT(log2_max_transform_skip_block_size_minus2);
        }
#endif
    }

    // slice header
    // IDR_W_RADL and IDR_N_LP NALUs do not contain st_rps in slice header.
    // Otherwise if short_term_ref_pic_set_sps_flag is 1, host decoder
    // shall set ucNumDeltaPocsOfRefRpsIdx to 0.
    auto slice_hdr = &shdr1;
    if (slice_hdr->short_term_ref_pic_set_sps_flag) {
        pp->ucNumDeltaPocsOfRefRpsIdx = 0;
        pp->wNumBitsForShortTermRPSInSlice = 0;
    } else {
        pp->ucNumDeltaPocsOfRefRpsIdx = slice_hdr->st_ref_pic_set.rps_idx_num_delta_pocs;
        pp->wNumBitsForShortTermRPSInSlice = slice_hdr->st_rps_bits;
    }
    pp->IrapPicFlag = slice_hdr->irap_pic;
    auto nal_unit_type = slice_hdr->nal_unit_type;
    pp->IdrPicFlag = (nal_unit_type == H265NALU::IDR_W_RADL || nal_unit_type == H265NALU::IDR_N_LP);
    pp->IntraPicFlag = slice_hdr->irap_pic;
    pp->StatusReportFeedbackNumber = 1;
    if (sps->scaling_list_enabled_flag) {
        // Fill up the quantitization matrix data structure when
        // pps->scaling_list_enabled is true. See section 4.2
        // of DXVA spec for HEVC.
        //
        const H265ScalingListData *scaling_lists = pps->pps_scaling_list_data_present_flag ? &pps->scaling_list_data
                                                                                           : &sps->scaling_list_data;

        memcpy(pim->ucScalingLists0, scaling_lists->scaling_list_4x4,
            sizeof pim->ucScalingLists0);

        memcpy(pim->ucScalingLists1, scaling_lists->scaling_list_8x8,
            sizeof pim->ucScalingLists1);

        memcpy(pim->ucScalingLists2, scaling_lists->scaling_list_16x16,
            sizeof pim->ucScalingLists2);

        memcpy(pim->ucScalingLists3[0], scaling_lists->scaling_list_32x32[0],
            sizeof(pim->ucScalingLists3[0]));
        memcpy(pim->ucScalingLists3[1], scaling_lists->scaling_list_32x32[3],
            sizeof(pim->ucScalingLists3[1]));

        memcpy(pim->ucScalingListDCCoefSizeID2,
            scaling_lists->scaling_list_dc_coef_16x16,
            sizeof(pim->ucScalingListDCCoefSizeID2));
        pim->ucScalingListDCCoefSizeID3[0] = scaling_lists->scaling_list_dc_coef_32x32[0];
        pim->ucScalingListDCCoefSizeID3[1] = scaling_lists->scaling_list_dc_coef_32x32[3];
    }

    uint32_t pic_order_count = shdr1.slice_pic_order_cnt_lsb;

    pp->CurrPicOrderCntVal = pic_order_count;
    pp->CurrPic.Index7Bits = pic_order_count;
    pp->CurrPic.AssociatedFlag = 0;
    pp->PicOrderCntValList[0] = pic_order_count;

    memset(pp->RefPicSetStCurrBefore, 0xff, sizeof(pp->RefPicSetStCurrBefore));
    memset(pp->RefPicSetStCurrAfter, 0xff, sizeof(pp->RefPicSetStCurrAfter));
    memset(pp->RefPicSetLtCurr, 0xff, sizeof(pp->RefPicSetLtCurr));
    memset(pp->RefPicList, 0xff, sizeof(pp->RefPicList));

#if 0
    //printf("num_short_term_ref_pic_sets=%d\n", sps->num_short_term_ref_pic_sets);
    for (int i = 0; i < sps->num_short_term_ref_pic_sets; ++i) {
        auto st_ref_pic_set = &sps->st_ref_pic_set[i];
        for (int j = 0; j < st_ref_pic_set->num_negative_pics; ++j) {
            printf("sps i=%d negative pic %d\n", i, st_ref_pic_set->delta_poc_s0[j]);
        }
    }
#endif

#if 0
    auto st_ref_pic_set = &shdr1.st_ref_pic_set;
    for (int i = 0; i < st_ref_pic_set->num_negative_pics; ++i) {
        //printf("slice negative pic %d\n", st_ref_pic_set->delta_poc_s0[i]);
         pp->RefPicList[i].Index7Bits = i;
         pp->RefPicList[i].AssociatedFlag = 0;
         pp->PicOrderCntValList[i] = pic_order_count + st_ref_pic_set->delta_poc_s0[i];
    }
#endif

    if (pic_order_count > 0) {
        pp->RefPicSetStCurrBefore[0] = 0;
         pp->RefPicList[0].Index7Bits = 0;
         pp->RefPicList[0].AssociatedFlag = 0;
         pp->PicOrderCntValList[0] = pic_order_count - 1;
    }

#if 0
    if (pic_order_count > 2) {
        pp->RefPicList[0].Index7Bits = pic_order_count - 2;
    }



    //p.PicOrderCntValList[i] = i;

    pp->RefPicList[0].Index7Bits = 0;//pic_order_count;
    pp->RefPicList[0].AssociatedFlag = 0;
    pp->PicOrderCntValList[0] = 0;

    pp->RefPicSetStCurrBefore[0] = pic_order_count > 0 ? pic_order_count - 2 : 0;
#endif

#if 0
    uint32_t pic_order_count = shdr1.slice_pic_order_cnt_lsb / 2;
    printf("pic_order_count=%u\n", pic_order_count);

    pp->RefPicList[i].Index7Bits = i;
    pp->RefPicList[i].AssociatedFlag = 0;
    pp->PicOrderCntValList[i] = i;
#endif
}

#ifdef USE_LIBVA
void HEVCParser::FillVA(_VAPictureParameterBufferHEVC *pp, _VAIQMatrixBufferHEVC *pim) {
    auto slice_hdr = &shdr1;

    int highest_tid = sps->sps_max_sub_layers_minus1;
#define FROM_SPS_TO_PP(a) pp->a = sps->a
#define FROM_SPS_TO_PP2(a, b) pp->b = sps->a
#define FROM_PPS_TO_PP(a) pp->a = pps->a
#define FROM_SPS_TO_PP_PF(a) pp->pic_fields.bits.a = sps->a
#define FROM_PPS_TO_PP_PF(a) pp->pic_fields.bits.a = pps->a
#define FROM_SPS_TO_PP_SPF(a) pp->slice_parsing_fields.bits.a = sps->a
#define FROM_PPS_TO_PP_SPF(a) pp->slice_parsing_fields.bits.a = pps->a
#define FROM_PPS_TO_PP_SPF2(a, b) pp->slice_parsing_fields.bits.b = pps->a
    FROM_SPS_TO_PP(pic_width_in_luma_samples);
    FROM_SPS_TO_PP(pic_height_in_luma_samples);
    FROM_SPS_TO_PP_PF(chroma_format_idc);
    FROM_SPS_TO_PP_PF(separate_colour_plane_flag);
    FROM_SPS_TO_PP_PF(pcm_enabled_flag);
    FROM_SPS_TO_PP_PF(scaling_list_enabled_flag);
    FROM_PPS_TO_PP_PF(transform_skip_enabled_flag);
    FROM_SPS_TO_PP_PF(amp_enabled_flag);
    FROM_SPS_TO_PP_PF(strong_intra_smoothing_enabled_flag);
    FROM_PPS_TO_PP_PF(sign_data_hiding_enabled_flag);
    FROM_PPS_TO_PP_PF(constrained_intra_pred_flag);
    FROM_PPS_TO_PP_PF(cu_qp_delta_enabled_flag);
    FROM_PPS_TO_PP_PF(weighted_pred_flag);
    FROM_PPS_TO_PP_PF(weighted_bipred_flag);
    FROM_PPS_TO_PP_PF(transquant_bypass_enabled_flag);
    FROM_PPS_TO_PP_PF(tiles_enabled_flag);
    FROM_PPS_TO_PP_PF(entropy_coding_sync_enabled_flag);
    FROM_PPS_TO_PP_PF(pps_loop_filter_across_slices_enabled_flag);
    FROM_PPS_TO_PP_PF(loop_filter_across_tiles_enabled_flag);
    FROM_SPS_TO_PP_PF(pcm_loop_filter_disabled_flag);
    pp->pic_fields.bits.NoPicReorderingFlag = (sps->sps_max_num_reorder_pics[highest_tid] == 0) ? 1 : 0;

    FROM_SPS_TO_PP2(sps_max_dec_pic_buffering_minus1[highest_tid],
        sps_max_dec_pic_buffering_minus1);
    FROM_SPS_TO_PP(bit_depth_luma_minus8);
    FROM_SPS_TO_PP(bit_depth_chroma_minus8);
    FROM_SPS_TO_PP(pcm_sample_bit_depth_luma_minus1);
    FROM_SPS_TO_PP(pcm_sample_bit_depth_chroma_minus1);
    FROM_SPS_TO_PP(log2_min_luma_coding_block_size_minus3);
    FROM_SPS_TO_PP(log2_diff_max_min_luma_coding_block_size);
    FROM_SPS_TO_PP2(log2_min_luma_transform_block_size_minus2,
        log2_min_transform_block_size_minus2);
    FROM_SPS_TO_PP2(log2_diff_max_min_luma_transform_block_size,
        log2_diff_max_min_transform_block_size);
    FROM_SPS_TO_PP(log2_min_pcm_luma_coding_block_size_minus3);
    FROM_SPS_TO_PP(log2_diff_max_min_pcm_luma_coding_block_size);
    FROM_SPS_TO_PP(max_transform_hierarchy_depth_intra);
    FROM_SPS_TO_PP(max_transform_hierarchy_depth_inter);
    FROM_PPS_TO_PP(init_qp_minus26);
    FROM_PPS_TO_PP(diff_cu_qp_delta_depth);
    FROM_PPS_TO_PP(pps_cb_qp_offset);
    FROM_PPS_TO_PP(pps_cr_qp_offset);
    FROM_PPS_TO_PP(log2_parallel_merge_level_minus2);
    FROM_PPS_TO_PP(num_tile_columns_minus1);
    FROM_PPS_TO_PP(num_tile_rows_minus1);
    if (pps->uniform_spacing_flag) {
        // We need to calculate this ourselves per 6.5.1 in the spec. We subtract 1
        // as well so it matches the 'minus1' usage in the struct.
        for (int i = 0; i <= pps->num_tile_columns_minus1; ++i) {
            pp->column_width_minus1[i] = (((i + 1) * sps->pic_width_in_ctbs_y) / (pps->num_tile_columns_minus1 + 1)) - ((i * sps->pic_width_in_ctbs_y) / (pps->num_tile_columns_minus1 + 1)) - 1;
        }
        for (int j = 0; j <= pps->num_tile_rows_minus1; ++j) {
            pp->row_height_minus1[j] = (((j + 1) * sps->pic_height_in_ctbs_y) / (pps->num_tile_rows_minus1 + 1)) - ((j * sps->pic_height_in_ctbs_y) / (pps->num_tile_rows_minus1 + 1)) - 1;
        }
    } else {
        for (int i = 0; i <= pps->num_tile_columns_minus1; ++i) {
            FROM_PPS_TO_PP(column_width_minus1[i]);
        }
        for (int i = 0; i <= pps->num_tile_rows_minus1; ++i) {
            FROM_PPS_TO_PP(row_height_minus1[i]);
        }
    }
    FROM_PPS_TO_PP_SPF(lists_modification_present_flag);
    FROM_SPS_TO_PP_SPF(long_term_ref_pics_present_flag);
    FROM_SPS_TO_PP_SPF(sps_temporal_mvp_enabled_flag);
    FROM_PPS_TO_PP_SPF(cabac_init_present_flag);
    FROM_PPS_TO_PP_SPF(output_flag_present_flag);
    FROM_PPS_TO_PP_SPF(dependent_slice_segments_enabled_flag);
    FROM_PPS_TO_PP_SPF(pps_slice_chroma_qp_offsets_present_flag);
    FROM_SPS_TO_PP_SPF(sample_adaptive_offset_enabled_flag);
    FROM_PPS_TO_PP_SPF(deblocking_filter_override_enabled_flag);
    FROM_PPS_TO_PP_SPF2(pps_deblocking_filter_disabled_flag,
        pps_disable_deblocking_filter_flag);
    FROM_PPS_TO_PP_SPF(slice_segment_header_extension_present_flag);
    pp->slice_parsing_fields.bits.RapPicFlag = slice_hdr->nal_unit_type >= H265NALU::BLA_W_LP && slice_hdr->nal_unit_type <= H265NALU::CRA_NUT;
    pp->slice_parsing_fields.bits.IdrPicFlag = slice_hdr->nal_unit_type >= H265NALU::IDR_W_RADL && slice_hdr->nal_unit_type <= H265NALU::IDR_N_LP;
    pp->slice_parsing_fields.bits.IntraPicFlag = slice_hdr->irap_pic;

    FROM_SPS_TO_PP(log2_max_pic_order_cnt_lsb_minus4);
    FROM_SPS_TO_PP(num_short_term_ref_pic_sets);
    FROM_SPS_TO_PP2(num_long_term_ref_pics_sps, num_long_term_ref_pic_sps);
    FROM_PPS_TO_PP(num_ref_idx_l0_default_active_minus1);
    FROM_PPS_TO_PP(num_ref_idx_l1_default_active_minus1);
    FROM_PPS_TO_PP(pps_beta_offset_div2);
    FROM_PPS_TO_PP(pps_tc_offset_div2);
    FROM_PPS_TO_PP(num_extra_slice_header_bits);
#undef FROM_SPS_TO_PP
#undef FROM_SPS_TO_PP2
#undef FROM_PPS_TO_PP
#undef FROM_SPS_TO_PP_PF
#undef FROM_PPS_TO_PP_PF
#undef FROM_SPS_TO_PP_SPF
#undef FROM_PPS_TO_PP_SPF
#undef FROM_PPS_TO_PP_SPF2
    if (slice_hdr->short_term_ref_pic_set_sps_flag) {
        pp->st_rps_bits = 0;
    } else {
        pp->st_rps_bits = slice_hdr->st_rps_bits;
    }

    InitVAPicture(&pp->CurrPic);
    //XXX FillVAPicture(&pp->CurrPic, std::move(pic));

    // Init reference pictures' array.
    for (size_t i = 0; i < std::size(pp->ReferenceFrames); ++i) {
        InitVAPicture(&pp->ReferenceFrames[i]);
    }

    // And fill it with picture info from DPB.
    //FillVARefFramesFromRefList(ref_pic_list, pp->ReferenceFrames);

    if (sps->scaling_list_enabled_flag) {
        memset(pim, 0, sizeof(*pim));

        // We already populated the IQMatrix with default values in the parser if they
        // are not present in the stream, so just fill them all in.
        const H265ScalingListData &scaling_list = pps->pps_scaling_list_data_present_flag ? pps->scaling_list_data
                                                                                          : sps->scaling_list_data;

        // We need another one of these since we can't use |scaling_list| above in
        // the static_assert checks below.
        H265ScalingListData checker;
        memcpy(pim->ScalingList4x4, scaling_list.scaling_list_4x4,
            sizeof(pim->ScalingList4x4));
        memcpy(pim->ScalingList8x8, scaling_list.scaling_list_8x8,
            sizeof(pim->ScalingList8x8));
        memcpy(pim->ScalingList16x16, scaling_list.scaling_list_16x16,
            sizeof(pim->ScalingList16x16));
        memcpy(pim->ScalingList32x32[0], scaling_list.scaling_list_32x32[0],
            sizeof(pim->ScalingList32x32[0]));
        memcpy(pim->ScalingList32x32[1], scaling_list.scaling_list_32x32[3],
            sizeof(pim->ScalingList32x32[1]));
        memcpy(pim->ScalingListDC16x16,
            scaling_list.scaling_list_dc_coef_16x16,
            sizeof(pim->ScalingListDC16x16));
        pim->ScalingListDC32x32[0] = scaling_list.scaling_list_dc_coef_32x32[0];
        pim->ScalingListDC32x32[1] = scaling_list.scaling_list_dc_coef_32x32[3];
    }
}
#endif

HEVCParser::Result HEVCParser::ParseStRefPicSet(int st_rps_idx, const H265SPS &sps, H265StRefPicSet *st_ref_pic_set, bool is_slice_hdr) {
    // 7.4.8
    bool inter_ref_pic_set_prediction_flag = false;
    if (st_rps_idx != 0) {
        READ_BOOL_OR_RETURN(&inter_ref_pic_set_prediction_flag);
    }
    if (inter_ref_pic_set_prediction_flag) {
        int delta_idx_minus1 = 0;
        if (st_rps_idx == sps.num_short_term_ref_pic_sets) {
            READ_UE_OR_RETURN(&delta_idx_minus1);
            IN_RANGE_OR_RETURN(delta_idx_minus1, 0, st_rps_idx - 1);
        }
        int ref_rps_idx = st_rps_idx - (delta_idx_minus1 + 1);
        int delta_rps_sign;
        int abs_delta_rps_minus1;
        READ_BOOL_OR_RETURN(&delta_rps_sign);
        READ_UE_OR_RETURN(&abs_delta_rps_minus1);
        IN_RANGE_OR_RETURN(abs_delta_rps_minus1, 0, 0x7FFF);
        int delta_rps = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);
        const H265StRefPicSet &ref_set = sps.st_ref_pic_set[ref_rps_idx];
        if (is_slice_hdr) {
            st_ref_pic_set->rps_idx_num_delta_pocs = ref_set.num_delta_pocs;
        }
        bool used_by_curr_pic_flag[kMaxShortTermRefPicSets];
        bool use_delta_flag[kMaxShortTermRefPicSets];
        // 7.4.8 - use_delta_flag defaults to 1 if not present.
        std::fill_n(use_delta_flag, kMaxShortTermRefPicSets, true);

        for (int j = 0; j <= ref_set.num_delta_pocs; j++) {
            READ_BOOL_OR_RETURN(&used_by_curr_pic_flag[j]);
            if (!used_by_curr_pic_flag[j]) {
                READ_BOOL_OR_RETURN(&use_delta_flag[j]);
            }
        }
        // Calculate delta_poc_s{0,1}, used_by_curr_pic_s{0,1}, num_negative_pics
        // and num_positive_pics.
        // Equation 7-61
        int i = 0;
        for (int j = ref_set.num_positive_pics - 1; j >= 0; --j) {
            int d_poc = ref_set.delta_poc_s1[j] + delta_rps;
            if (d_poc < 0 && use_delta_flag[ref_set.num_negative_pics + j]) {
                st_ref_pic_set->delta_poc_s0[i] = d_poc;
                st_ref_pic_set->used_by_curr_pic_s0[i++] = used_by_curr_pic_flag[ref_set.num_negative_pics + j];
            }
        }
        if (delta_rps < 0 && use_delta_flag[ref_set.num_delta_pocs]) {
            st_ref_pic_set->delta_poc_s0[i] = delta_rps;
            st_ref_pic_set->used_by_curr_pic_s0[i++] = used_by_curr_pic_flag[ref_set.num_delta_pocs];
        }
        for (int j = 0; j < ref_set.num_negative_pics; ++j) {
            int d_poc = ref_set.delta_poc_s0[j] + delta_rps;
            if (d_poc < 0 && use_delta_flag[j]) {
                st_ref_pic_set->delta_poc_s0[i] = d_poc;
                st_ref_pic_set->used_by_curr_pic_s0[i++] = used_by_curr_pic_flag[j];
            }
        }
        st_ref_pic_set->num_negative_pics = i;
        // Equation 7-62
        i = 0;
        for (int j = ref_set.num_negative_pics - 1; j >= 0; --j) {
            int d_poc = ref_set.delta_poc_s0[j] + delta_rps;
            if (d_poc > 0 && use_delta_flag[j]) {
                st_ref_pic_set->delta_poc_s1[i] = d_poc;
                st_ref_pic_set->used_by_curr_pic_s1[i++] = used_by_curr_pic_flag[j];
            }
        }
        if (delta_rps > 0 && use_delta_flag[ref_set.num_delta_pocs]) {
            st_ref_pic_set->delta_poc_s1[i] = delta_rps;
            st_ref_pic_set->used_by_curr_pic_s1[i++] = used_by_curr_pic_flag[ref_set.num_delta_pocs];
        }
        for (int j = 0; j < ref_set.num_positive_pics; ++j) {
            int d_poc = ref_set.delta_poc_s1[j] + delta_rps;
            if (d_poc > 0 && use_delta_flag[ref_set.num_negative_pics + j]) {
                st_ref_pic_set->delta_poc_s1[i] = d_poc;
                st_ref_pic_set->used_by_curr_pic_s1[i++] = used_by_curr_pic_flag[ref_set.num_negative_pics + j];
            }
        }
        st_ref_pic_set->num_positive_pics = i;
        IN_RANGE_OR_RETURN(
            st_ref_pic_set->num_negative_pics, 0,
            sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1]);
        IN_RANGE_OR_RETURN(
            st_ref_pic_set->num_positive_pics, 0,
            sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1] - st_ref_pic_set->num_negative_pics);
    } else {
        READ_UE_OR_RETURN(&st_ref_pic_set->num_negative_pics);
        READ_UE_OR_RETURN(&st_ref_pic_set->num_positive_pics);
        IN_RANGE_OR_RETURN(
            st_ref_pic_set->num_negative_pics, 0,
            sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1]);
        IN_RANGE_OR_RETURN(
            st_ref_pic_set->num_positive_pics, 0,
            sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1] - st_ref_pic_set->num_negative_pics);
        for (int i = 0; i < st_ref_pic_set->num_negative_pics; ++i) {
            //printf("negative pic i=%d\n", i);
            int delta_poc_s0_minus1;
            READ_UE_OR_RETURN(&delta_poc_s0_minus1);
            IN_RANGE_OR_RETURN(delta_poc_s0_minus1, 0, 0x7FFF);
            if (i == 0) {
                st_ref_pic_set->delta_poc_s0[i] = -(delta_poc_s0_minus1 + 1);
            } else {
                st_ref_pic_set->delta_poc_s0[i] = st_ref_pic_set->delta_poc_s0[i - 1] - (delta_poc_s0_minus1 + 1);
            }
            READ_BOOL_OR_RETURN(&st_ref_pic_set->used_by_curr_pic_s0[i]);
        }
        for (int i = 0; i < st_ref_pic_set->num_positive_pics; ++i) {
            printf("positive pic %d\n", i);
            int delta_poc_s1_minus1;
            READ_UE_OR_RETURN(&delta_poc_s1_minus1);
            IN_RANGE_OR_RETURN(delta_poc_s1_minus1, 0, 0x7FFF);
            if (i == 0) {
                st_ref_pic_set->delta_poc_s1[i] = delta_poc_s1_minus1 + 1;
            } else {
                st_ref_pic_set->delta_poc_s1[i] = st_ref_pic_set->delta_poc_s1[i - 1] + delta_poc_s1_minus1 + 1;
            }
            READ_BOOL_OR_RETURN(&st_ref_pic_set->used_by_curr_pic_s1[i]);
        }
    }
    // Calculate num_delta_pocs.
    st_ref_pic_set->num_delta_pocs = st_ref_pic_set->num_negative_pics + st_ref_pic_set->num_positive_pics;
    return kOk;
}


HEVCParser::Result HEVCParser::ParseSPS(H265SPS **psps) {
    // 7.4.3.2
    DVLOG(4) << "Parsing SPS\n";
    Result res = kOk;

    // DCHECK(sps_id);
    //*sps_id = -1;

    auto sps = new H265SPS();
    *psps = sps;

    READ_BITS_OR_RETURN(4, &sps->sps_video_parameter_set_id);
    IN_RANGE_OR_RETURN(sps->sps_video_parameter_set_id, 0, 15);
    READ_BITS_OR_RETURN(3, &sps->sps_max_sub_layers_minus1);
    IN_RANGE_OR_RETURN(sps->sps_max_sub_layers_minus1, 0, 6);
    READ_BOOL_OR_RETURN(&sps->sps_temporal_id_nesting_flag);

    res = ParseProfileTierLevel(true, sps->sps_max_sub_layers_minus1,
        &sps->profile_tier_level);
    if (res != kOk) {
        errx(1, "failed parsing profile");
        return res;
    }

    READ_UE_OR_RETURN(&sps->sps_seq_parameter_set_id);
    IN_RANGE_OR_RETURN(sps->sps_seq_parameter_set_id, 0, 15);
    READ_UE_OR_RETURN(&sps->chroma_format_idc);
    IN_RANGE_OR_RETURN(sps->chroma_format_idc, 0, 3);
    if (sps->chroma_format_idc == 3) {
        READ_BOOL_OR_RETURN(&sps->separate_colour_plane_flag);
    }
    sps->chroma_array_type = sps->separate_colour_plane_flag ? 0 : sps->chroma_format_idc;
    // Table 6-1.
    if (sps->chroma_format_idc == 1) {
        sps->sub_width_c = sps->sub_height_c = 2;
    } else if (sps->chroma_format_idc == 2) {
        sps->sub_width_c = 2;
        sps->sub_height_c = 1;
    } else {
        sps->sub_width_c = sps->sub_height_c = 1;
    }
    READ_UE_OR_RETURN(&sps->pic_width_in_luma_samples);
    READ_UE_OR_RETURN(&sps->pic_height_in_luma_samples);

    TRUE_OR_RETURN(sps->pic_width_in_luma_samples != 0);
    TRUE_OR_RETURN(sps->pic_height_in_luma_samples != 0);

    // Equation A-2: Calculate max_dpb_size.
    int max_luma_ps = sps->profile_tier_level.GetMaxLumaPs();
    // base::CheckedNumeric<int>
    int pic_size = sps->pic_height_in_luma_samples;
    pic_size *= sps->pic_width_in_luma_samples;
#if 0
if (!pic_size.IsValid()) {
    return kInvalidStream;
}
#endif
    int pic_size_in_samples_y = pic_size; //.ValueOrDefault(0);
    size_t max_dpb_pic_buf = sps->profile_tier_level.GetDpbMaxPicBuf();

    if (pic_size_in_samples_y <= (max_luma_ps >> 2)) {
        sps->max_dpb_size = std::min(4 * max_dpb_pic_buf, size_t { 16 });
    } else if (pic_size_in_samples_y <= (max_luma_ps >> 1)) {
        sps->max_dpb_size = std::min(2 * max_dpb_pic_buf, size_t { 16 });
    } else if (pic_size_in_samples_y <= ((3 * max_luma_ps) >> 2)) {
        sps->max_dpb_size = std::min((4 * max_dpb_pic_buf) / 3, size_t { 16 });
    } else {
        sps->max_dpb_size = max_dpb_pic_buf;
    }

    bool conformance_window_flag;
    READ_BOOL_OR_RETURN(&conformance_window_flag);
    if (conformance_window_flag) {
        READ_UE_OR_RETURN(&sps->conf_win_left_offset);
        READ_UE_OR_RETURN(&sps->conf_win_right_offset);
        READ_UE_OR_RETURN(&sps->conf_win_top_offset);
        READ_UE_OR_RETURN(&sps->conf_win_bottom_offset);
#if 0
        printf("offsets %d %d %d %d\n",
            sps->conf_win_left_offset,
            sps->conf_win_right_offset,
            sps->conf_win_top_offset,
            sps->conf_win_bottom_offset);
#endif

        // base::CheckedNumeric<int>
        int width_crop = sps->conf_win_left_offset;
        width_crop += sps->conf_win_right_offset;
        width_crop *= sps->sub_width_c;
#if 0
    if (!width_crop.IsValid()) {
        return kInvalidStream;
    }
#endif
        TRUE_OR_RETURN(width_crop /* .ValueOrDefault(0) */ < sps->pic_width_in_luma_samples);
        // base::CheckedNumeric<int>
        int height_crop = sps->conf_win_top_offset;
        height_crop += sps->conf_win_bottom_offset;
        height_crop *= sps->sub_height_c;
#if 0
    if (!height_crop.IsValid()) {
        return kInvalidStream;
    }
#endif
        TRUE_OR_RETURN(height_crop /* .ValueOrDefault(0) */ < sps->pic_height_in_luma_samples);
    }
    READ_UE_OR_RETURN(&sps->bit_depth_luma_minus8);
    IN_RANGE_OR_RETURN(sps->bit_depth_luma_minus8, 0, 8);
    sps->bit_depth_y = sps->bit_depth_luma_minus8 + 8;
    READ_UE_OR_RETURN(&sps->bit_depth_chroma_minus8);
    IN_RANGE_OR_RETURN(sps->bit_depth_chroma_minus8, 0, 8);
    sps->bit_depth_c = sps->bit_depth_chroma_minus8 + 8;
    READ_UE_OR_RETURN(&sps->log2_max_pic_order_cnt_lsb_minus4);
    IN_RANGE_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4, 0, 12);
    sps->max_pic_order_cnt_lsb = (int) std::pow(2, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
    bool sps_sub_layer_ordering_info_present_flag;
    READ_BOOL_OR_RETURN(&sps_sub_layer_ordering_info_present_flag);
    for (int i = sps_sub_layer_ordering_info_present_flag
             ? 0
             : sps->sps_max_sub_layers_minus1;
         i <= sps->sps_max_sub_layers_minus1; ++i) {
        READ_UE_OR_RETURN(&sps->sps_max_dec_pic_buffering_minus1[i]);
        IN_RANGE_OR_RETURN(sps->sps_max_dec_pic_buffering_minus1[i], 0,
            static_cast<int>(sps->max_dpb_size) - 1);
        READ_UE_OR_RETURN(&sps->sps_max_num_reorder_pics[i]);
        IN_RANGE_OR_RETURN(sps->sps_max_num_reorder_pics[i], 0,
            sps->sps_max_dec_pic_buffering_minus1[i]);
        if (i > 0) {
            TRUE_OR_RETURN(sps->sps_max_dec_pic_buffering_minus1[i] >= sps->sps_max_dec_pic_buffering_minus1[i - 1]);
            TRUE_OR_RETURN(sps->sps_max_num_reorder_pics[i] >= sps->sps_max_num_reorder_pics[i - 1]);
        }
        READ_UE_OR_RETURN(&sps->sps_max_latency_increase_plus1[i]);
        IN_RANGE_OR_RETURN(sps->sps_max_latency_increase_plus1[i], 0, 0xFFFFFFFE);
    }
    if (!sps_sub_layer_ordering_info_present_flag) {
        // Fill in the default values for the other sublayers.
        for (int i = 0; i < sps->sps_max_sub_layers_minus1; ++i) {
            sps->sps_max_dec_pic_buffering_minus1[i] = sps->sps_max_dec_pic_buffering_minus1[sps->sps_max_sub_layers_minus1];
            sps->sps_max_num_reorder_pics[i] = sps->sps_max_num_reorder_pics[sps->sps_max_sub_layers_minus1];
            sps->sps_max_latency_increase_plus1[i] = sps->sps_max_latency_increase_plus1[sps->sps_max_sub_layers_minus1];
        }
    }

#if 0
// Equation 7-9: Calculate SpsMaxLatencyPictures.
for (int i = 0; i <= sps->sps_max_sub_layers_minus1; ++i) {
    if (sps->sps_max_latency_increase_plus1[i] != 0) {
        sps->sps_max_latency_pictures[i] = static_cast<uint32_t>(sps->sps_max_num_reorder_pics[i]) + sps->sps_max_latency_increase_plus1[i] - 1;
    } else {
        sps->sps_max_latency_pictures[i] = 0;
    }
}
#endif

    READ_UE_OR_RETURN(&sps->log2_min_luma_coding_block_size_minus3);
    // This enforces that min_cb_log2_size_y below will be <= 30 and prevents
    // integer overflow math there.
    TRUE_OR_RETURN(sps->log2_min_luma_coding_block_size_minus3 <= 27);
    READ_UE_OR_RETURN(&sps->log2_diff_max_min_luma_coding_block_size);

    int min_cb_log2_size_y = sps->log2_min_luma_coding_block_size_minus3 + 3;
    // base::CheckedNumeric<int>
    int ctb_log2_size_y = min_cb_log2_size_y;
    ctb_log2_size_y += sps->log2_diff_max_min_luma_coding_block_size;
#if 0
if (!ctb_log2_size_y.IsValid()) {
    return kInvalidStream;
}
#endif

    sps->ctb_log2_size_y = ctb_log2_size_y /* .ValueOrDefault(0) */;
    TRUE_OR_RETURN(sps->ctb_log2_size_y <= 30);
    int min_cb_size_y = 1 << min_cb_log2_size_y;
    int ctb_size_y = 1 << sps->ctb_log2_size_y;
#if 0
sps->pic_width_in_ctbs_y = base::ClampCeil(
    static_cast<float>(sps->pic_width_in_luma_samples) / ctb_size_y);
sps->pic_height_in_ctbs_y = base::ClampCeil(
    static_cast<float>(sps->pic_height_in_luma_samples) / ctb_size_y);
#else
    sps->pic_width_in_ctbs_y = ceilf(
        static_cast<float>(sps->pic_width_in_luma_samples) / ctb_size_y);
    sps->pic_height_in_ctbs_y = ceilf(
        static_cast<float>(sps->pic_height_in_luma_samples) / ctb_size_y);
#endif
    // base::CheckedNumeric<int>
    int pic_size_in_ctbs_y = sps->pic_width_in_ctbs_y;
    pic_size_in_ctbs_y *= sps->pic_height_in_ctbs_y;
#if 0
if (!pic_size_in_ctbs_y.IsValid()) {
    return kInvalidStream;
}
#endif
    sps->pic_size_in_ctbs_y = pic_size_in_ctbs_y /* .ValueOrDefault(0) */;

    TRUE_OR_RETURN(sps->pic_width_in_luma_samples % min_cb_size_y == 0);
    TRUE_OR_RETURN(sps->pic_height_in_luma_samples % min_cb_size_y == 0);
    READ_UE_OR_RETURN(&sps->log2_min_luma_transform_block_size_minus2);
    TRUE_OR_RETURN(sps->log2_min_luma_transform_block_size_minus2 < min_cb_log2_size_y - 2);
    int min_tb_log2_size_y = sps->log2_min_luma_transform_block_size_minus2 + 2;
    READ_UE_OR_RETURN(&sps->log2_diff_max_min_luma_transform_block_size);
    TRUE_OR_RETURN(sps->log2_diff_max_min_luma_transform_block_size <= std::min(sps->ctb_log2_size_y, 5) - min_tb_log2_size_y);
    READ_UE_OR_RETURN(&sps->max_transform_hierarchy_depth_inter);
    IN_RANGE_OR_RETURN(sps->max_transform_hierarchy_depth_inter, 0,
        sps->ctb_log2_size_y - min_tb_log2_size_y);
    READ_UE_OR_RETURN(&sps->max_transform_hierarchy_depth_intra);
    IN_RANGE_OR_RETURN(sps->max_transform_hierarchy_depth_intra, 0,
        sps->ctb_log2_size_y - min_tb_log2_size_y);
    READ_BOOL_OR_RETURN(&sps->scaling_list_enabled_flag);
    if (sps->scaling_list_enabled_flag) {
        READ_BOOL_OR_RETURN(&sps->sps_scaling_list_data_present_flag);
    }
    if (sps->sps_scaling_list_data_present_flag) {
        res = ParseScalingListData(&sps->scaling_list_data);
        if (res != kOk) {
            return res;
        }
    } else {
        // Fill it in with the default values.
        for (int size_id = 0; size_id < 4; ++size_id) {
            for (int matrix_id = 0; matrix_id < 6;
                 matrix_id += (size_id == 3) ? 3 : 1) {
                FillInDefaultScalingListData(&sps->scaling_list_data, size_id,
                    matrix_id);
            }
        }
    }
    READ_BOOL_OR_RETURN(&sps->amp_enabled_flag);
    READ_BOOL_OR_RETURN(&sps->sample_adaptive_offset_enabled_flag);
    READ_BOOL_OR_RETURN(&sps->pcm_enabled_flag);
    if (sps->pcm_enabled_flag) {
        READ_BITS_OR_RETURN(4, &sps->pcm_sample_bit_depth_luma_minus1);
        TRUE_OR_RETURN(sps->pcm_sample_bit_depth_luma_minus1 + 1 <= sps->bit_depth_y);
        READ_BITS_OR_RETURN(4, &sps->pcm_sample_bit_depth_chroma_minus1);
        TRUE_OR_RETURN(sps->pcm_sample_bit_depth_chroma_minus1 + 1 <= sps->bit_depth_c);
        READ_UE_OR_RETURN(&sps->log2_min_pcm_luma_coding_block_size_minus3);
        IN_RANGE_OR_RETURN(sps->log2_min_pcm_luma_coding_block_size_minus3, 0, 2);
        int log2_min_ipcm_cb_size_y = sps->log2_min_pcm_luma_coding_block_size_minus3 + 3;
        IN_RANGE_OR_RETURN(log2_min_ipcm_cb_size_y, std::min(min_cb_log2_size_y, 5),
            std::min(sps->ctb_log2_size_y, 5));
        READ_UE_OR_RETURN(&sps->log2_diff_max_min_pcm_luma_coding_block_size);
        TRUE_OR_RETURN(sps->log2_diff_max_min_pcm_luma_coding_block_size <= std::min(sps->ctb_log2_size_y, 5) - log2_min_ipcm_cb_size_y);
        READ_BOOL_OR_RETURN(&sps->pcm_loop_filter_disabled_flag);
    }
    READ_UE_OR_RETURN(&sps->num_short_term_ref_pic_sets);
    IN_RANGE_OR_RETURN(sps->num_short_term_ref_pic_sets, 0,
        kMaxShortTermRefPicSets);
    for (int i = 0; i < sps->num_short_term_ref_pic_sets; ++i) {
        res = ParseStRefPicSet(i, *sps, &sps->st_ref_pic_set[i]);
        if (res != kOk) {
            return res;
        }
    }
    READ_BOOL_OR_RETURN(&sps->long_term_ref_pics_present_flag);
    if (sps->long_term_ref_pics_present_flag) {
        READ_UE_OR_RETURN(&sps->num_long_term_ref_pics_sps);
        IN_RANGE_OR_RETURN(sps->num_long_term_ref_pics_sps, 0,
            kMaxLongTermRefPicSets);
        for (int i = 0; i < sps->num_long_term_ref_pics_sps; ++i) {
            READ_BITS_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                &sps->lt_ref_pic_poc_lsb_sps[i]);
            READ_BOOL_OR_RETURN(&sps->used_by_curr_pic_lt_sps_flag[i]);
        }
    }
    READ_BOOL_OR_RETURN(&sps->sps_temporal_mvp_enabled_flag);
    READ_BOOL_OR_RETURN(&sps->strong_intra_smoothing_enabled_flag);
    bool vui_parameters_present_flag;
    READ_BOOL_OR_RETURN(&vui_parameters_present_flag);
    if (vui_parameters_present_flag) {
        res = ParseVuiParameters(*sps, &sps->vui_parameters);
        if (res != kOk) {
            return res;
        }
        // Verify cropping parameters. We already verified the conformance window
        // ranges previously.
        // base::CheckedNumeric<int>
        int width_crop = sps->conf_win_left_offset + sps->conf_win_right_offset;
        width_crop += sps->vui_parameters.def_disp_win_left_offset;
        width_crop += sps->vui_parameters.def_disp_win_right_offset;
        width_crop *= sps->sub_width_c;
#if 0
    if (!width_crop.IsValid()) {
        return kInvalidStream;
    }
#endif
        TRUE_OR_RETURN(width_crop /* .ValueOrDefault(0) */ < sps->pic_width_in_luma_samples);
        // base::CheckedNumeric<int>
        int height_crop = sps->conf_win_top_offset + sps->conf_win_bottom_offset;
        height_crop += sps->vui_parameters.def_disp_win_top_offset;
        height_crop += sps->vui_parameters.def_disp_win_bottom_offset;
        height_crop *= sps->sub_height_c;
#if 0
    if (!height_crop.IsValid()) {
        return kInvalidStream;
    }
#endif
        TRUE_OR_RETURN(height_crop /* .ValueOrDefault(0) */ < sps->pic_height_in_luma_samples);
    }

    READ_BOOL_OR_RETURN(&sps->sps_extension_present_flag);
    if (sps->sps_extension_present_flag) {
        READ_BOOL_OR_RETURN(&sps->sps_range_extension_flag);
        READ_BOOL_OR_RETURN(&sps->sps_multilayer_extension_flag);
        READ_BOOL_OR_RETURN(&sps->sps_3d_extension_flag);
        READ_BOOL_OR_RETURN(&sps->sps_scc_extension_flag);
        SKIP_BITS_OR_RETURN(4); // sps_extension_4bits
    }
    if (sps->sps_range_extension_flag) {
        READ_BOOL_OR_RETURN(&sps->transform_skip_rotation_enabled_flag);
        READ_BOOL_OR_RETURN(&sps->transform_skip_context_enabled_flag);
        READ_BOOL_OR_RETURN(&sps->implicit_rdpcm_enabled_flag);
        READ_BOOL_OR_RETURN(&sps->explicit_rdpcm_enabled_flag);
        READ_BOOL_OR_RETURN(&sps->extended_precision_processing_flag);
        READ_BOOL_OR_RETURN(&sps->intra_smoothing_disabled_flag);
        READ_BOOL_OR_RETURN(&sps->high_precision_offsets_enabled_flag);
        READ_BOOL_OR_RETURN(&sps->persistent_rice_adaptation_enabled_flag);
        READ_BOOL_OR_RETURN(&sps->cabac_bypass_alignment_enabled_flag);
    }
    if (sps->sps_multilayer_extension_flag) {
        DVLOG(1) << "HEVC multilayer extension not supported";
        return kInvalidStream;
    }
    if (sps->sps_3d_extension_flag) {
        DVLOG(1) << "HEVC 3D extension not supported";
        return kInvalidStream;
    }
    if (sps->sps_scc_extension_flag) {
        DVLOG(1) << "HEVC SCC extension not supported";
        return kInvalidStream;
    }

    sps->wp_offset_half_range_y = 1 << (sps->high_precision_offsets_enabled_flag
                                          ? sps->bit_depth_luma_minus8 + 7
                                          : 7);
    sps->wp_offset_half_range_c = 1 << (sps->high_precision_offsets_enabled_flag
                                          ? sps->bit_depth_chroma_minus8 + 7
                                          : 7);

#if 0
// If an SPS with the same id already exists, replace it.
*sps_id = sps->sps_seq_parameter_set_id;
active_sps_[*sps_id] = std::move(sps);
#endif

    return res;
}

HEVCParser::Result HEVCParser::ParsePPS(H265SPS *sps, H265PPS **ppps) {
    // 7.4.3.3
    DVLOG(4) << "Parsing PPS\n";
    Result res = kOk;

    // DCHECK(pps_id);
    //*pps_id = -1;
    auto pps = new H265PPS();
    *ppps = pps;

    pps->temporal_id = 0; // XXX nalu.nuh_temporal_id_plus1 - 1;

    // Set these defaults if they are not present here.
    pps->loop_filter_across_tiles_enabled_flag = 1;

    // 7.4.3.3.1
    READ_UE_OR_RETURN(&pps->pps_pic_parameter_set_id);
    IN_RANGE_OR_RETURN(pps->pps_pic_parameter_set_id, 0, 63);
    READ_UE_OR_RETURN(&pps->pps_seq_parameter_set_id);
    IN_RANGE_OR_RETURN(pps->pps_seq_parameter_set_id, 0, 15);
    if (!sps) {
        DVLOG(1) << "missing sps";
        return kMissingParameterSet;
    }
    READ_BOOL_OR_RETURN(&pps->dependent_slice_segments_enabled_flag);
    READ_BOOL_OR_RETURN(&pps->output_flag_present_flag);
    READ_BITS_OR_RETURN(3, &pps->num_extra_slice_header_bits);
    READ_BOOL_OR_RETURN(&pps->sign_data_hiding_enabled_flag);
    READ_BOOL_OR_RETURN(&pps->cabac_init_present_flag);
    READ_UE_OR_RETURN(&pps->num_ref_idx_l0_default_active_minus1);
    IN_RANGE_OR_RETURN(pps->num_ref_idx_l0_default_active_minus1, 0,
        kMaxRefIdxActive - 1);
    READ_UE_OR_RETURN(&pps->num_ref_idx_l1_default_active_minus1);
    IN_RANGE_OR_RETURN(pps->num_ref_idx_l1_default_active_minus1, 0,
        kMaxRefIdxActive - 1);
    READ_SE_OR_RETURN(&pps->init_qp_minus26);
    pps->qp_bd_offset_y = 6 * sps->bit_depth_luma_minus8;
    IN_RANGE_OR_RETURN(pps->init_qp_minus26, -(26 + pps->qp_bd_offset_y), 25);
    READ_BOOL_OR_RETURN(&pps->constrained_intra_pred_flag);
    READ_BOOL_OR_RETURN(&pps->transform_skip_enabled_flag);
    READ_BOOL_OR_RETURN(&pps->cu_qp_delta_enabled_flag);
    if (pps->cu_qp_delta_enabled_flag) {
        READ_UE_OR_RETURN(&pps->diff_cu_qp_delta_depth);
        IN_RANGE_OR_RETURN(pps->diff_cu_qp_delta_depth, 0,
            sps->log2_diff_max_min_luma_coding_block_size);
    }
    READ_SE_OR_RETURN(&pps->pps_cb_qp_offset);
    IN_RANGE_OR_RETURN(pps->pps_cb_qp_offset, -12, 12);
    READ_SE_OR_RETURN(&pps->pps_cr_qp_offset);
    IN_RANGE_OR_RETURN(pps->pps_cr_qp_offset, -12, 12);
    READ_BOOL_OR_RETURN(&pps->pps_slice_chroma_qp_offsets_present_flag);
    READ_BOOL_OR_RETURN(&pps->weighted_pred_flag);
    READ_BOOL_OR_RETURN(&pps->weighted_bipred_flag);
    READ_BOOL_OR_RETURN(&pps->transquant_bypass_enabled_flag);
    READ_BOOL_OR_RETURN(&pps->tiles_enabled_flag);
    READ_BOOL_OR_RETURN(&pps->entropy_coding_sync_enabled_flag);
    if (pps->tiles_enabled_flag) {
        READ_UE_OR_RETURN(&pps->num_tile_columns_minus1);
        IN_RANGE_OR_RETURN(pps->num_tile_columns_minus1, 0,
            sps->pic_width_in_ctbs_y - 1);
        TRUE_OR_RETURN(pps->num_tile_columns_minus1 < H265PPS::kMaxNumTileColumnWidth);
        READ_UE_OR_RETURN(&pps->num_tile_rows_minus1);
        IN_RANGE_OR_RETURN(pps->num_tile_rows_minus1, 0,
            sps->pic_height_in_ctbs_y - 1);
        TRUE_OR_RETURN((pps->num_tile_columns_minus1 != 0) || (pps->num_tile_rows_minus1 != 0));
        TRUE_OR_RETURN(pps->num_tile_rows_minus1 < H265PPS::kMaxNumTileRowHeight);
        READ_BOOL_OR_RETURN(&pps->uniform_spacing_flag);
        if (!pps->uniform_spacing_flag) {
            pps->column_width_minus1[pps->num_tile_columns_minus1] = sps->pic_width_in_ctbs_y - 1;
            for (int i = 0; i < pps->num_tile_columns_minus1; ++i) {
                READ_UE_OR_RETURN(&pps->column_width_minus1[i]);
                IN_RANGE_OR_RETURN(
                    pps->column_width_minus1[i], 0,
                    pps->column_width_minus1[pps->num_tile_columns_minus1] - 1);
                pps->column_width_minus1[pps->num_tile_columns_minus1] -= pps->column_width_minus1[i] + 1;
            }
            pps->row_height_minus1[pps->num_tile_rows_minus1] = sps->pic_height_in_ctbs_y - 1;
            for (int i = 0; i < pps->num_tile_rows_minus1; ++i) {
                READ_UE_OR_RETURN(&pps->row_height_minus1[i]);
                IN_RANGE_OR_RETURN(
                    pps->row_height_minus1[i], 0,
                    pps->row_height_minus1[pps->num_tile_rows_minus1] - 1);
                pps->row_height_minus1[pps->num_tile_rows_minus1] -= pps->row_height_minus1[i] + 1;
            }
        }
        READ_BOOL_OR_RETURN(&pps->loop_filter_across_tiles_enabled_flag);
    }
    READ_BOOL_OR_RETURN(&pps->pps_loop_filter_across_slices_enabled_flag);
    READ_BOOL_OR_RETURN(&pps->deblocking_filter_control_present_flag);
    if (pps->deblocking_filter_control_present_flag) {
        READ_BOOL_OR_RETURN(&pps->deblocking_filter_override_enabled_flag);
        READ_BOOL_OR_RETURN(&pps->pps_deblocking_filter_disabled_flag);
        if (!pps->pps_deblocking_filter_disabled_flag) {
            READ_SE_OR_RETURN(&pps->pps_beta_offset_div2);
            IN_RANGE_OR_RETURN(pps->pps_beta_offset_div2, -6, 6);
            READ_SE_OR_RETURN(&pps->pps_tc_offset_div2);
            IN_RANGE_OR_RETURN(pps->pps_tc_offset_div2, -6, 6);
        }
    }
    READ_BOOL_OR_RETURN(&pps->pps_scaling_list_data_present_flag);
#if 0
  if (pps->pps_scaling_list_data_present_flag) {
    res = ParseScalingListData(&pps->scaling_list_data);
    if (res != kOk)
      return res;
  }
#endif
    READ_BOOL_OR_RETURN(&pps->lists_modification_present_flag);
    READ_UE_OR_RETURN(&pps->log2_parallel_merge_level_minus2);
    IN_RANGE_OR_RETURN(pps->log2_parallel_merge_level_minus2, 0,
        sps->ctb_log2_size_y - 2);
    READ_BOOL_OR_RETURN(&pps->slice_segment_header_extension_present_flag);
    READ_BOOL_OR_RETURN(&pps->pps_extension_present_flag);
    if (pps->pps_extension_present_flag) {
        READ_BOOL_OR_RETURN(&pps->pps_range_extension_flag);
        READ_BOOL_OR_RETURN(&pps->pps_multilayer_extension_flag);
        READ_BOOL_OR_RETURN(&pps->pps_3d_extension_flag);
        READ_BOOL_OR_RETURN(&pps->pps_scc_extension_flag);
        SKIP_BITS_OR_RETURN(4); // pps_extension_4bits
    }

    if (pps->pps_range_extension_flag) {
        if (pps->transform_skip_enabled_flag) {
            READ_UE_OR_RETURN(&pps->log2_max_transform_skip_block_size_minus2);
            IN_RANGE_OR_RETURN(pps->log2_max_transform_skip_block_size_minus2, 0, 3);
        }
        READ_BOOL_OR_RETURN(&pps->cross_component_prediction_enabled_flag);
        READ_BOOL_OR_RETURN(&pps->chroma_qp_offset_list_enabled_flag);
        if (pps->chroma_qp_offset_list_enabled_flag) {
            READ_UE_OR_RETURN(&pps->diff_cu_chroma_qp_offset_depth);
            IN_RANGE_OR_RETURN(pps->diff_cu_chroma_qp_offset_depth, 0,
                sps->log2_diff_max_min_luma_coding_block_size);
            READ_UE_OR_RETURN(&pps->chroma_qp_offset_list_len_minus1);
            IN_RANGE_OR_RETURN(pps->chroma_qp_offset_list_len_minus1, 0, 5);
            for (int i = 0; i <= pps->chroma_qp_offset_list_len_minus1; i++) {
                READ_SE_OR_RETURN(&pps->cb_qp_offset_list[i]);
                IN_RANGE_OR_RETURN(pps->cb_qp_offset_list[i], -12, 12);
                READ_SE_OR_RETURN(&pps->cr_qp_offset_list[i]);
                IN_RANGE_OR_RETURN(pps->cr_qp_offset_list[i], -12, 12);
            }
        }
        READ_UE_OR_RETURN(&pps->log2_sao_offset_scale_luma);
        IN_RANGE_OR_RETURN(pps->log2_sao_offset_scale_luma, 0,
            std::max(sps->bit_depth_luma_minus8 - 2, 0));
        READ_UE_OR_RETURN(&pps->log2_sao_offset_scale_chroma);
        IN_RANGE_OR_RETURN(pps->log2_sao_offset_scale_chroma, 0,
            std::max(sps->bit_depth_chroma_minus8 - 2, 0));
    }
    if (pps->pps_multilayer_extension_flag) {
        DVLOG(1) << "HEVC multilayer extension not supported";
        return kInvalidStream;
    }
    if (pps->pps_3d_extension_flag) {
        DVLOG(1) << "HEVC 3D extension not supported";
        return kInvalidStream;
    }
    if (pps->pps_scc_extension_flag) {
        DVLOG(1) << "HEVC SCC extension not supported";
        return kInvalidStream;
    }

    // If a PPS with the same id already exists, replace it.
    //*pps_id = pps->pps_pic_parameter_set_id;
    // active_pps_[*pps_id] = std::move(pps);

    return res;
}


HEVCParser::Result HEVCParser::ParseSliceHeaderForPictureParameterSets(
    const H265NALU &nalu,
    int *pps_id) {
    // 7.4.7 Slice segment header
    //DVLOG(4) << "Parsing slice header for pps";

    H265SliceHeader shdr;
    READ_BOOL_OR_RETURN(&shdr.first_slice_segment_in_pic_flag);
    shdr.irap_pic = (nalu.nal_unit_type >= H265NALU::BLA_W_LP && nalu.nal_unit_type <= H265NALU::RSV_IRAP_VCL23);
    if (shdr.irap_pic) {
        READ_BOOL_OR_RETURN(&shdr.no_output_of_prior_pics_flag);
    }
    READ_UE_OR_RETURN(&shdr.slice_pic_parameter_set_id);
    IN_RANGE_OR_RETURN(shdr.slice_pic_parameter_set_id, 0, 63);
    if (pps_id) {
        *pps_id = shdr.slice_pic_parameter_set_id;
    }

    return kOk;
}

HEVCParser::Result HEVCParser::ParseSliceHeader(const H265NALU &nalu, H265SliceHeader *shdr,
    H265SliceHeader *prior_shdr) {
    // 7.4.7 Slice segment header
    //DVLOG(4) << "Parsing slice header\n";
    Result res = kOk;

    DCHECK(shdr);
    memset(shdr, 0, sizeof(*shdr));
    shdr->nal_unit_type = nalu.nal_unit_type;
    shdr->nalu_data = nalu.data;
    shdr->nalu_size = nalu.size;

    READ_BOOL_OR_RETURN(&shdr->first_slice_segment_in_pic_flag);
    shdr->irap_pic = (shdr->nal_unit_type >= H265NALU::BLA_W_LP && shdr->nal_unit_type <= H265NALU::RSV_IRAP_VCL23);
    if (shdr->irap_pic) {
        READ_BOOL_OR_RETURN(&shdr->no_output_of_prior_pics_flag);
    }
    READ_UE_OR_RETURN(&shdr->slice_pic_parameter_set_id);
    IN_RANGE_OR_RETURN(shdr->slice_pic_parameter_set_id, 0, 63);
    // pps = GetPPS(shdr->slice_pic_parameter_set_id);
    if (!pps) {
        return kMissingParameterSet;
    }
    // sps = GetSPS(pps->pps_seq_parameter_set_id);
    DCHECK(sps); // We already validated this when we parsed the PPS.

    if (!shdr->first_slice_segment_in_pic_flag) {
        if (pps->dependent_slice_segments_enabled_flag) {
            READ_BOOL_OR_RETURN(&shdr->dependent_slice_segment_flag);
        }
        READ_BITS_OR_RETURN(Log2Ceiling(sps->pic_size_in_ctbs_y),
            &shdr->slice_segment_address);
        IN_RANGE_OR_RETURN(shdr->slice_segment_address, 0,
            sps->pic_size_in_ctbs_y - 1);
    }
    if (shdr->dependent_slice_segment_flag) {
        if (!prior_shdr) {
            DVLOG(1) << "Cannot parse dependent slice w/out prior slice data";
            return kInvalidStream;
        }
        // Copy everything in the structure starting at |slice_type| going forward.
        // This is copying the dependent slice data that we do not parse below.
        size_t skip_amount = offsetof(H265SliceHeader, slice_type);
        memcpy(reinterpret_cast<uint8_t *>(shdr) + skip_amount,
            reinterpret_cast<uint8_t *>(prior_shdr) + skip_amount,
            sizeof(H265SliceHeader) - skip_amount);

        // We also need to validate the fields that have conditions that depend on
        // anything unique in this slice (i.e. anything already parsed).
        if ((shdr->irap_pic || sps->sps_max_dec_pic_buffering_minus1[pps->temporal_id] == 0) && nalu.nuh_layer_id == 0) {
            TRUE_OR_RETURN(shdr->slice_type == 2);
        }
    } else {
        // Set these defaults if they are not present here.
        shdr->pic_output_flag = 1;
        shdr->num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
        shdr->num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;
        shdr->collocated_from_l0_flag = 1;
        shdr->slice_deblocking_filter_disabled_flag = pps->pps_deblocking_filter_disabled_flag;
        shdr->slice_beta_offset_div2 = pps->pps_beta_offset_div2;
        shdr->slice_tc_offset_div2 = pps->pps_tc_offset_div2;
        shdr->slice_loop_filter_across_slices_enabled_flag = pps->pps_loop_filter_across_slices_enabled_flag;
        shdr->curr_rps_idx = sps->num_short_term_ref_pic_sets;

        // slice_reserved_flag
        SKIP_BITS_OR_RETURN(pps->num_extra_slice_header_bits);
        READ_UE_OR_RETURN(&shdr->slice_type);
        if ((shdr->irap_pic || sps->sps_max_dec_pic_buffering_minus1[pps->temporal_id] == 0) && nalu.nuh_layer_id == 0) {
            TRUE_OR_RETURN(shdr->slice_type == 2);
        }
        if (pps->output_flag_present_flag) {
            READ_BOOL_OR_RETURN(&shdr->pic_output_flag);
        }
        if (sps->separate_colour_plane_flag) {
            READ_BITS_OR_RETURN(2, &shdr->colour_plane_id);
            IN_RANGE_OR_RETURN(shdr->colour_plane_id, 0, 2);
        }
        if (shdr->nal_unit_type != H265NALU::IDR_W_RADL && shdr->nal_unit_type != H265NALU::IDR_N_LP) {
            READ_BITS_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                &shdr->slice_pic_order_cnt_lsb);
            IN_RANGE_OR_RETURN(shdr->slice_pic_order_cnt_lsb, 0,
                sps->max_pic_order_cnt_lsb - 1);
            //printf("slice_pic_order_cnt_lsb=%u\n", shdr->slice_pic_order_cnt_lsb);
            READ_BOOL_OR_RETURN(&shdr->short_term_ref_pic_set_sps_flag);
            if (!shdr->short_term_ref_pic_set_sps_flag) {
                off_t bits_left_prior = br_.NumBitsLeft();
                size_t num_epb_prior = br_.NumEmulationPreventionBytesRead();
                //printf("vs short term pic set\n");
                res = ParseStRefPicSet(sps->num_short_term_ref_pic_sets, *sps,
                    &shdr->st_ref_pic_set, true);
                if (res != kOk) {
                    return res;
                }
                shdr->st_rps_bits = (bits_left_prior - br_.NumBitsLeft()) - 8 * (br_.NumEmulationPreventionBytesRead() - num_epb_prior);
            } else if (sps->num_short_term_ref_pic_sets > 1) {
                READ_BITS_OR_RETURN(
                    Log2Ceiling(sps->num_short_term_ref_pic_sets),
                    &shdr->short_term_ref_pic_set_idx);
                IN_RANGE_OR_RETURN(shdr->short_term_ref_pic_set_idx, 0,
                    sps->num_short_term_ref_pic_sets - 1);
            }

            if (shdr->short_term_ref_pic_set_sps_flag) {
                shdr->curr_rps_idx = shdr->short_term_ref_pic_set_idx;
            }

            if (sps->long_term_ref_pics_present_flag) {
                off_t bits_left_prior = br_.NumBitsLeft();
                size_t num_epb_prior = br_.NumEmulationPreventionBytesRead();
                if (sps->num_long_term_ref_pics_sps > 0) {
                    READ_UE_OR_RETURN(&shdr->num_long_term_sps);
                    IN_RANGE_OR_RETURN(shdr->num_long_term_sps, 0,
                        sps->num_long_term_ref_pics_sps);
                }
                READ_UE_OR_RETURN(&shdr->num_long_term_pics);
                if (nalu.nuh_layer_id == 0) {
                    TRUE_OR_RETURN(
                        shdr->num_long_term_pics <= (sps->sps_max_dec_pic_buffering_minus1[pps->temporal_id] - shdr->GetStRefPicSet(sps).num_negative_pics - shdr->GetStRefPicSet(sps).num_positive_pics - shdr->num_long_term_sps));
                }
                IN_RANGE_OR_RETURN(shdr->num_long_term_pics, 0,
                    kMaxLongTermRefPicSets - shdr->num_long_term_sps);
                for (int i = 0; i < shdr->num_long_term_sps + shdr->num_long_term_pics;
                     ++i) {
                    if (i < shdr->num_long_term_sps) {
                        int lt_idx_sps = 0;
                        if (sps->num_long_term_ref_pics_sps > 1) {
                            READ_BITS_OR_RETURN(
                                Log2Ceiling(sps->num_long_term_ref_pics_sps),
                                &lt_idx_sps);
                            IN_RANGE_OR_RETURN(lt_idx_sps, 0,
                                sps->num_long_term_ref_pics_sps - 1);
                        }
                        shdr->poc_lsb_lt[i] = sps->lt_ref_pic_poc_lsb_sps[lt_idx_sps];
                        shdr->used_by_curr_pic_lt[i] = sps->used_by_curr_pic_lt_sps_flag[lt_idx_sps];
                    } else {
                        READ_BITS_OR_RETURN(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                            &shdr->poc_lsb_lt[i]);
                        READ_BOOL_OR_RETURN(&shdr->used_by_curr_pic_lt[i]);
                    }
                    READ_BOOL_OR_RETURN(&shdr->delta_poc_msb_present_flag[i]);
                    if (shdr->delta_poc_msb_present_flag[i]) {
                        READ_UE_OR_RETURN(&shdr->delta_poc_msb_cycle_lt[i]);
                        IN_RANGE_OR_RETURN(
                            shdr->delta_poc_msb_cycle_lt[i], 0,
                            std::pow(2, 32 - sps->log2_max_pic_order_cnt_lsb_minus4 - 4));
                        // Equation 7-52.
                        if (i != 0 && i != shdr->num_long_term_sps) {
                            shdr->delta_poc_msb_cycle_lt[i] = shdr->delta_poc_msb_cycle_lt[i] + shdr->delta_poc_msb_cycle_lt[i - 1];
                        }
                    }
                }
                shdr->lt_rps_bits = (bits_left_prior - br_.NumBitsLeft()) - 8 * (br_.NumEmulationPreventionBytesRead() - num_epb_prior);
            }
            if (sps->sps_temporal_mvp_enabled_flag) {
                READ_BOOL_OR_RETURN(&shdr->slice_temporal_mvp_enabled_flag);
            }
        }
        if (sps->sample_adaptive_offset_enabled_flag) {
            READ_BOOL_OR_RETURN(&shdr->slice_sao_luma_flag);
            if (sps->chroma_array_type != 0) {
                READ_BOOL_OR_RETURN(&shdr->slice_sao_chroma_flag);
            }
        }
        if (shdr->IsPSlice() || shdr->IsBSlice()) {
            READ_BOOL_OR_RETURN(&shdr->num_ref_idx_active_override_flag);
            if (shdr->num_ref_idx_active_override_flag) {
                READ_UE_OR_RETURN(&shdr->num_ref_idx_l0_active_minus1);
                IN_RANGE_OR_RETURN(shdr->num_ref_idx_l0_active_minus1, 0,
                    kMaxRefIdxActive - 1);
                if (shdr->IsBSlice()) {
                    READ_UE_OR_RETURN(&shdr->num_ref_idx_l1_active_minus1);
                    IN_RANGE_OR_RETURN(shdr->num_ref_idx_l1_active_minus1, 0,
                        kMaxRefIdxActive - 1);
                }
            }

            shdr->num_pic_total_curr = 0;
            const H265StRefPicSet &st_ref_pic = shdr->GetStRefPicSet(sps);
            for (int i = 0; i < st_ref_pic.num_negative_pics; ++i) {
                if (st_ref_pic.used_by_curr_pic_s0[i]) {
                    shdr->num_pic_total_curr++;
                }
            }
            for (int i = 0; i < st_ref_pic.num_positive_pics; ++i) {
                if (st_ref_pic.used_by_curr_pic_s1[i]) {
                    shdr->num_pic_total_curr++;
                }
            }
            for (int i = 0; i < shdr->num_long_term_sps + shdr->num_long_term_pics;
                 ++i) {
                if (shdr->used_by_curr_pic_lt[i]) {
                    shdr->num_pic_total_curr++;
                }
            }

            TRUE_OR_RETURN(shdr->num_pic_total_curr);
            if (pps->lists_modification_present_flag && shdr->num_pic_total_curr > 1) {
                res = ParseRefPicListsModifications(*shdr,
                    &shdr->ref_pic_lists_modification);
                if (res != kOk) {
                    return res;
                }
            }
            if (shdr->IsBSlice()) {
                READ_BOOL_OR_RETURN(&shdr->mvd_l1_zero_flag);
            }
            if (pps->cabac_init_present_flag) {
                READ_BOOL_OR_RETURN(&shdr->cabac_init_flag);
            }
            if (shdr->slice_temporal_mvp_enabled_flag) {
                if (shdr->IsBSlice()) {
                    READ_BOOL_OR_RETURN(&shdr->collocated_from_l0_flag);
                }
                if ((shdr->collocated_from_l0_flag && shdr->num_ref_idx_l0_active_minus1 > 0) || (!shdr->collocated_from_l0_flag && shdr->num_ref_idx_l1_active_minus1 > 0)) {
                    READ_UE_OR_RETURN(&shdr->collocated_ref_idx);
                    if ((shdr->IsPSlice() || shdr->IsBSlice()) && shdr->collocated_from_l0_flag) {
                        IN_RANGE_OR_RETURN(shdr->collocated_ref_idx, 0,
                            shdr->num_ref_idx_l0_active_minus1);
                    }
                    if (shdr->IsBSlice() && !shdr->collocated_from_l0_flag) {
                        IN_RANGE_OR_RETURN(shdr->collocated_ref_idx, 0,
                            shdr->num_ref_idx_l1_active_minus1);
                    }
                }
            }

            if ((pps->weighted_pred_flag && shdr->IsPSlice()) || (pps->weighted_bipred_flag && shdr->IsBSlice())) {
                res = ParsePredWeightTable(*sps, *shdr, &shdr->pred_weight_table);
                if (res != kOk) {
                    return res;
                }
            }
            READ_UE_OR_RETURN(&shdr->five_minus_max_num_merge_cand);
            IN_RANGE_OR_RETURN(5 - shdr->five_minus_max_num_merge_cand, 1, 5);
        }
        READ_SE_OR_RETURN(&shdr->slice_qp_delta);
        IN_RANGE_OR_RETURN(26 + pps->init_qp_minus26 + shdr->slice_qp_delta,
            -pps->qp_bd_offset_y, 51);

        if (pps->pps_slice_chroma_qp_offsets_present_flag) {
            READ_SE_OR_RETURN(&shdr->slice_cb_qp_offset);
            IN_RANGE_OR_RETURN(shdr->slice_cb_qp_offset, -12, 12);
            IN_RANGE_OR_RETURN(pps->pps_cb_qp_offset + shdr->slice_cb_qp_offset, -12,
                12);
            READ_SE_OR_RETURN(&shdr->slice_cr_qp_offset);
            IN_RANGE_OR_RETURN(shdr->slice_cr_qp_offset, -12, 12);
            IN_RANGE_OR_RETURN(pps->pps_cr_qp_offset + shdr->slice_cr_qp_offset, -12,
                12);
        }

        // pps_slice_act_qp_offsets_present_flag is zero, we don't support SCC ext.

        if (pps->chroma_qp_offset_list_enabled_flag) {
            SKIP_BITS_OR_RETURN(1); // cu_chroma_qp_offset_enabled_flag
        }
        bool deblocking_filter_override_flag = false;
        if (pps->deblocking_filter_override_enabled_flag) {
            READ_BOOL_OR_RETURN(&deblocking_filter_override_flag);
        }
        if (deblocking_filter_override_flag) {
            READ_BOOL_OR_RETURN(&shdr->slice_deblocking_filter_disabled_flag);
            if (!shdr->slice_deblocking_filter_disabled_flag) {
                READ_SE_OR_RETURN(&shdr->slice_beta_offset_div2);
                IN_RANGE_OR_RETURN(shdr->slice_beta_offset_div2, -6, 6);
                READ_SE_OR_RETURN(&shdr->slice_tc_offset_div2);
                IN_RANGE_OR_RETURN(shdr->slice_tc_offset_div2, -6, 6);
            }
        }
        if (pps->pps_loop_filter_across_slices_enabled_flag && (shdr->slice_sao_luma_flag || shdr->slice_sao_chroma_flag || !shdr->slice_deblocking_filter_disabled_flag)) {
            READ_BOOL_OR_RETURN(&shdr->slice_loop_filter_across_slices_enabled_flag);
        }
    }

    if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag) {
        int num_entry_point_offsets;
        READ_UE_OR_RETURN(&num_entry_point_offsets);
        if (!pps->tiles_enabled_flag) {
            IN_RANGE_OR_RETURN(num_entry_point_offsets, 0,
                sps->pic_height_in_ctbs_y - 1);
        } else if (!pps->entropy_coding_sync_enabled_flag) {
            IN_RANGE_OR_RETURN(
                num_entry_point_offsets, 0,
                (pps->num_tile_columns_minus1 + 1) * (pps->num_tile_rows_minus1 + 1) - 1);
        } else { // both are true
            IN_RANGE_OR_RETURN(
                num_entry_point_offsets, 0,
                (pps->num_tile_columns_minus1 + 1) * sps->pic_height_in_ctbs_y - 1);
        }
        if (num_entry_point_offsets > 0) {
            int offset_len_minus1;
            READ_UE_OR_RETURN(&offset_len_minus1);
            IN_RANGE_OR_RETURN(offset_len_minus1, 0, 31);
            SKIP_BITS_OR_RETURN(num_entry_point_offsets * (offset_len_minus1 + 1));
        }
    }

    if (pps->slice_segment_header_extension_present_flag) {
        int slice_segment_header_extension_length;
        READ_UE_OR_RETURN(&slice_segment_header_extension_length);
        IN_RANGE_OR_RETURN(slice_segment_header_extension_length, 0, 256);
        SKIP_BITS_OR_RETURN(slice_segment_header_extension_length * 8);
    }

    if (prior_shdr && !shdr->first_slice_segment_in_pic_flag) {
        // Validate the fields that must match between slice headers for the same
        // picture.
        EQ_OR_RETURN(shdr, prior_shdr, slice_pic_parameter_set_id);
        EQ_OR_RETURN(shdr, prior_shdr, pic_output_flag);
        EQ_OR_RETURN(shdr, prior_shdr, no_output_of_prior_pics_flag);
        EQ_OR_RETURN(shdr, prior_shdr, slice_pic_order_cnt_lsb);
        EQ_OR_RETURN(shdr, prior_shdr, short_term_ref_pic_set_sps_flag);

        // All the other fields we need to compare are contiguous, so compare them
        // as one memory range.
        size_t block_start = offsetof(H265SliceHeader, short_term_ref_pic_set_idx);
        size_t block_end = offsetof(H265SliceHeader, slice_sao_luma_flag);
        TRUE_OR_RETURN(!memcmp(reinterpret_cast<uint8_t *>(shdr) + block_start,
            reinterpret_cast<uint8_t *>(prior_shdr) + block_start,
            block_end - block_start));
    }

    // byte_alignment()
    SKIP_BITS_OR_RETURN(1); // alignment bit
    int bits_left_to_align = br_.NumBitsLeft() % 8;
    if (bits_left_to_align) {
        SKIP_BITS_OR_RETURN(bits_left_to_align);
    }

    shdr->header_emulation_prevention_bytes = br_.NumEmulationPreventionBytesRead();
    shdr->header_size = shdr->nalu_size - shdr->header_emulation_prevention_bytes - br_.NumBitsLeft() / 8;
    return res;
}

void HEVCParser::FillInDefaultScalingListData(H265ScalingListData *scaling_list_data,
    int size_id,
    int matrix_id) {
    if (size_id == 0) {
        std::fill_n(scaling_list_data->scaling_list_4x4[matrix_id],
            H265ScalingListData::kScalingListSizeId0Count,
            H265ScalingListData::kDefaultScalingListSize0Values);
        return;
    }

    uint8_t *dst;
    switch (size_id) {
    case 1:
        dst = scaling_list_data->scaling_list_8x8[matrix_id];
        break;
    case 2:
        dst = scaling_list_data->scaling_list_16x16[matrix_id];
        break;
    case 3:
        dst = scaling_list_data->scaling_list_32x32[matrix_id];
        break;
    }
    const uint8_t *src;
    if (matrix_id < 3) {
        src = kDefaultScalingListSize1To3Matrix0To2;
    } else {
        src = kDefaultScalingListSize1To3Matrix3To5;
    }
    memcpy(dst, src,
        H265ScalingListData::kScalingListSizeId1To3Count * sizeof(*src));

    // These are sixteen because the default for the minus8 values is 8.
    if (size_id == 2) {
        scaling_list_data->scaling_list_dc_coef_16x16[matrix_id] = 16;
    } else if (size_id == 3) {
        scaling_list_data->scaling_list_dc_coef_32x32[matrix_id] = 16;
    }
}

HEVCParser::Result HEVCParser::ParseScalingListData(
    H265ScalingListData *scaling_list_data) {
    for (int size_id = 0; size_id < 4; ++size_id) {
        for (int matrix_id = 0; matrix_id < 6;
             matrix_id += (size_id == 3) ? 3 : 1) {
            bool scaling_list_pred_mode_flag;
            READ_BOOL_OR_RETURN(&scaling_list_pred_mode_flag);
            if (!scaling_list_pred_mode_flag) {
                int scaling_list_pred_matrix_id_delta;
                READ_UE_OR_RETURN(&scaling_list_pred_matrix_id_delta);
                if (size_id <= 2) {
                    IN_RANGE_OR_RETURN(scaling_list_pred_matrix_id_delta, 0, matrix_id);
                } else { // size_id == 3
                    IN_RANGE_OR_RETURN(scaling_list_pred_matrix_id_delta, 0,
                        matrix_id / 3);
                }
                if (scaling_list_pred_matrix_id_delta == 0) {
                    FillInDefaultScalingListData(scaling_list_data, size_id, matrix_id);
                } else {
                    int ref_matrix_id = matrix_id - scaling_list_pred_matrix_id_delta * (size_id == 3 ? 3 : 1);
                    uint8_t *dst;
                    uint8_t *src;
                    int count = H265ScalingListData::kScalingListSizeId1To3Count;
                    switch (size_id) {
                    case 0:
                        src = scaling_list_data->scaling_list_4x4[ref_matrix_id];
                        dst = scaling_list_data->scaling_list_4x4[matrix_id];
                        count = H265ScalingListData::kScalingListSizeId0Count;
                        break;
                    case 1:
                        src = scaling_list_data->scaling_list_8x8[ref_matrix_id];
                        dst = scaling_list_data->scaling_list_8x8[matrix_id];
                        break;
                    case 2:
                        src = scaling_list_data->scaling_list_16x16[ref_matrix_id];
                        dst = scaling_list_data->scaling_list_16x16[matrix_id];
                        break;
                    case 3:
                        src = scaling_list_data->scaling_list_32x32[ref_matrix_id];
                        dst = scaling_list_data->scaling_list_32x32[matrix_id];
                        break;
                    }
                    memcpy(dst, src, count * sizeof(*src));

                    if (size_id == 2) {
                        scaling_list_data->scaling_list_dc_coef_16x16[matrix_id] = scaling_list_data->scaling_list_dc_coef_16x16[ref_matrix_id];
                    } else if (size_id == 3) {
                        scaling_list_data->scaling_list_dc_coef_32x32[matrix_id] = scaling_list_data->scaling_list_dc_coef_32x32[ref_matrix_id];
                    }
                }
            } else {
                int next_coef = 8;
                int coef_num = std::min(64, (1 << (4 + (size_id << 1))));
                if (size_id > 1) {
                    if (size_id == 2) {
                        int scaling_list_dc_coef_16x16_minus_8;
                        READ_SE_OR_RETURN(&scaling_list_dc_coef_16x16_minus_8);
                        IN_RANGE_OR_RETURN(scaling_list_dc_coef_16x16_minus_8, -7, 247);
                        // This is parsed as minus8;
                        scaling_list_data->scaling_list_dc_coef_16x16[matrix_id] = scaling_list_dc_coef_16x16_minus_8 + 8;
                        next_coef = scaling_list_data->scaling_list_dc_coef_16x16[matrix_id];
                    } else { // size_id == 3
                        int scaling_list_dc_coef_32x32_minus_8;
                        READ_SE_OR_RETURN(&scaling_list_dc_coef_32x32_minus_8);
                        IN_RANGE_OR_RETURN(scaling_list_dc_coef_32x32_minus_8, -7, 247);
                        // This is parsed as minus8;
                        scaling_list_data->scaling_list_dc_coef_32x32[matrix_id] = scaling_list_dc_coef_32x32_minus_8 + 8;
                        next_coef = scaling_list_data->scaling_list_dc_coef_32x32[matrix_id];
                    }
                }
                for (int i = 0; i < coef_num; ++i) {
                    int scaling_list_delta_coef;
                    READ_SE_OR_RETURN(&scaling_list_delta_coef);
                    IN_RANGE_OR_RETURN(scaling_list_delta_coef, -128, 127);
                    next_coef = (next_coef + scaling_list_delta_coef + 256) % 256;
                    switch (size_id) {
                    case 0:
                        scaling_list_data->scaling_list_4x4[matrix_id][i] = next_coef;
                        break;
                    case 1:
                        scaling_list_data->scaling_list_8x8[matrix_id][i] = next_coef;
                        break;
                    case 2:
                        scaling_list_data->scaling_list_16x16[matrix_id][i] = next_coef;
                        break;
                    case 3:
                        scaling_list_data->scaling_list_32x32[matrix_id][i] = next_coef;
                        break;
                    }
                }
            }
        }
    }
    return kOk;
}

HEVCParser::Result HEVCParser::ParseProfileTierLevel(
    bool profile_present,
    int max_num_sub_layers_minus1,
    H265ProfileTierLevel *profile_tier_level) {
    // 7.4.4
    DVLOG(4) << "Parsing profile_tier_level\n";
    if (profile_present) {
        int general_profile_space;
        READ_BITS_OR_RETURN(2, &general_profile_space);
        TRUE_OR_RETURN(general_profile_space == 0);
        SKIP_BITS_OR_RETURN(1); // general_tier_flag
        READ_BITS_OR_RETURN(5, &profile_tier_level->general_profile_idc);
        IN_RANGE_OR_RETURN(profile_tier_level->general_profile_idc, 0, 11);
        uint16_t general_profile_compatibility_flag_high16;
        uint16_t general_profile_compatibility_flag_low16;
        READ_BITS_OR_RETURN(16, &general_profile_compatibility_flag_high16);
        READ_BITS_OR_RETURN(16, &general_profile_compatibility_flag_low16);
        profile_tier_level->general_profile_compatibility_flags = (general_profile_compatibility_flag_high16 << 16) + general_profile_compatibility_flag_low16;
        READ_BOOL_OR_RETURN(&profile_tier_level->general_progressive_source_flag);
        READ_BOOL_OR_RETURN(&profile_tier_level->general_interlaced_source_flag);
        if (!profile_tier_level->general_progressive_source_flag && profile_tier_level->general_interlaced_source_flag) {
            DVLOG(1) << "Interlaced streams not supported";
            return kUnsupportedStream;
        }
        READ_BOOL_OR_RETURN(
            &profile_tier_level->general_non_packed_constraint_flag);
        READ_BOOL_OR_RETURN(
            &profile_tier_level->general_frame_only_constraint_flag);
        SKIP_BITS_OR_RETURN(7); // general_reserved_zero_7bits
        READ_BOOL_OR_RETURN(
            &profile_tier_level->general_one_picture_only_constraint_flag);
        SKIP_BITS_OR_RETURN(35); // general_reserved_zero_35bits
        SKIP_BITS_OR_RETURN(1); // general_inbld_flag
    }
    READ_BITS_OR_RETURN(8, &profile_tier_level->general_level_idc);
    bool sub_layer_profile_present_flag[8];
    bool sub_layer_level_present_flag[8];
    for (int i = 0; i < max_num_sub_layers_minus1; ++i) {
        READ_BOOL_OR_RETURN(&sub_layer_profile_present_flag[i]);
        READ_BOOL_OR_RETURN(&sub_layer_level_present_flag[i]);
    }
    if (max_num_sub_layers_minus1 > 0) {
        for (int i = max_num_sub_layers_minus1; i < 8; i++) {
            SKIP_BITS_OR_RETURN(2);
        }
    }
    for (int i = 0; i < max_num_sub_layers_minus1; i++) {
        if (sub_layer_profile_present_flag[i]) {
            SKIP_BITS_OR_RETURN(2); // sub_layer_profile_space
            SKIP_BITS_OR_RETURN(1); // sub_layer_tier_flag
            SKIP_BITS_OR_RETURN(5); // sub_layer_profile_idc
            SKIP_BITS_OR_RETURN(32); // sub_layer_profile_compatibility_flag
            SKIP_BITS_OR_RETURN(2); // sub_layer_{progressive,interlaced}_source_flag
            // Ignore sub_layer_non_packed_constraint_flag and
            // sub_layer_frame_only_constraint_flag.
            SKIP_BITS_OR_RETURN(2);
            // Skip the compatibility flags, they are always 43 bits.
            SKIP_BITS_OR_RETURN(43);
            SKIP_BITS_OR_RETURN(1); // sub_layer_inbld_flag
        }
        if (sub_layer_level_present_flag[i]) {
            SKIP_BITS_OR_RETURN(8); // sub_layer_level_idc
        }
    }
    return kOk;
}

HEVCParser::Result HEVCParser::ParseAndIgnoreSubLayerHrdParameters(
    int cpb_cnt,
    bool sub_pic_hrd_params_present_flag) {
    int data;
    for (int i = 0; i < cpb_cnt; ++i) {
        READ_UE_OR_RETURN(&data); // bit_rate_value_minus1[i]
        READ_UE_OR_RETURN(&data); // cpb_size_value_minus1[i]
        if (sub_pic_hrd_params_present_flag) {
            READ_UE_OR_RETURN(&data); // cpb_size_du_value_minus1[i]
            READ_UE_OR_RETURN(&data); // bit_rate_du_value_minus1[i]
        }
        SKIP_BITS_OR_RETURN(1); // cbr_flag[i]
    }
    return kOk;
}

HEVCParser::Result HEVCParser::ParseRefPicListsModifications(
    const H265SliceHeader &shdr,
    H265RefPicListsModifications *rpl_mod) {
    READ_BOOL_OR_RETURN(&rpl_mod->ref_pic_list_modification_flag_l0);
    if (rpl_mod->ref_pic_list_modification_flag_l0) {
        for (int i = 0; i <= shdr.num_ref_idx_l0_active_minus1; ++i) {
            READ_BITS_OR_RETURN(Log2Ceiling(shdr.num_pic_total_curr),
                &rpl_mod->list_entry_l0[i]);
            IN_RANGE_OR_RETURN(rpl_mod->list_entry_l0[i], 0,
                shdr.num_pic_total_curr - 1);
        }
    }
    if (shdr.IsBSlice()) {
        READ_BOOL_OR_RETURN(&rpl_mod->ref_pic_list_modification_flag_l1);
        if (rpl_mod->ref_pic_list_modification_flag_l1) {
            for (int i = 0; i <= shdr.num_ref_idx_l1_active_minus1; ++i) {
                READ_BITS_OR_RETURN(Log2Ceiling(shdr.num_pic_total_curr),
                    &rpl_mod->list_entry_l1[i]);
                IN_RANGE_OR_RETURN(rpl_mod->list_entry_l1[i], 0,
                    shdr.num_pic_total_curr - 1);
            }
        }
    }
    return kOk;
}

HEVCParser::Result HEVCParser::ParsePredWeightTable(
    const H265SPS &sps,
    const H265SliceHeader &shdr,
    H265PredWeightTable *pred_weight_table) {
    // 7.4.6.3 Weighted prediction parameters semantics
    READ_UE_OR_RETURN(&pred_weight_table->luma_log2_weight_denom);
    IN_RANGE_OR_RETURN(pred_weight_table->luma_log2_weight_denom, 0, 7);
    if (sps.chroma_array_type) {
        READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_log2_weight_denom);
        pred_weight_table->chroma_log2_weight_denom = pred_weight_table->delta_chroma_log2_weight_denom + pred_weight_table->luma_log2_weight_denom;
        IN_RANGE_OR_RETURN(pred_weight_table->chroma_log2_weight_denom, 0, 7);
    }
    bool luma_weight_flag[kMaxRefIdxActive];
    bool chroma_weight_flag[kMaxRefIdxActive];
    memset(chroma_weight_flag, 0, sizeof(chroma_weight_flag));
    for (int i = 0; i <= shdr.num_ref_idx_l0_active_minus1; ++i) {
        READ_BOOL_OR_RETURN(&luma_weight_flag[i]);
    }
    if (sps.chroma_array_type) {
        for (int i = 0; i <= shdr.num_ref_idx_l0_active_minus1; ++i) {
            READ_BOOL_OR_RETURN(&chroma_weight_flag[i]);
        }
    }
    int sum_weight_l0_flags = 0;
    for (int i = 0; i <= shdr.num_ref_idx_l0_active_minus1; ++i) {
        if (luma_weight_flag[i]) {
            sum_weight_l0_flags++;
            READ_SE_OR_RETURN(&pred_weight_table->delta_luma_weight_l0[i]);
            IN_RANGE_OR_RETURN(pred_weight_table->delta_luma_weight_l0[i], -128, 127);
            READ_SE_OR_RETURN(&pred_weight_table->luma_offset_l0[i]);
            IN_RANGE_OR_RETURN(pred_weight_table->luma_offset_l0[i],
                -sps.wp_offset_half_range_y,
                sps.wp_offset_half_range_y - 1);
        }
        if (chroma_weight_flag[i]) {
            sum_weight_l0_flags += 2;
            for (int j = 0; j < 2; ++j) {
                READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_weight_l0[i][j]);
                IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_weight_l0[i][j],
                    -128, 127);
                READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_offset_l0[i][j]);
                IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_offset_l0[i][j],
                    -4 * sps.wp_offset_half_range_c,
                    4 * sps.wp_offset_half_range_c - 1);
            }
        }
    }
    if (shdr.IsPSlice()) {
        TRUE_OR_RETURN(sum_weight_l0_flags <= 24);
    }
    if (shdr.IsBSlice()) {
        memset(chroma_weight_flag, 0, sizeof(chroma_weight_flag));
        int sum_weight_l1_flags = 0;
        for (int i = 0; i <= shdr.num_ref_idx_l1_active_minus1; ++i) {
            READ_BOOL_OR_RETURN(&luma_weight_flag[i]);
        }
        if (sps.chroma_array_type) {
            for (int i = 0; i <= shdr.num_ref_idx_l1_active_minus1; ++i) {
                READ_BOOL_OR_RETURN(&chroma_weight_flag[i]);
            }
        }
        for (int i = 0; i <= shdr.num_ref_idx_l1_active_minus1; ++i) {
            if (luma_weight_flag[i]) {
                sum_weight_l1_flags++;
                READ_SE_OR_RETURN(&pred_weight_table->delta_luma_weight_l1[i]);
                IN_RANGE_OR_RETURN(pred_weight_table->delta_luma_weight_l1[i], -128,
                    127);
                READ_SE_OR_RETURN(&pred_weight_table->luma_offset_l1[i]);
                IN_RANGE_OR_RETURN(pred_weight_table->luma_offset_l1[i],
                    -sps.wp_offset_half_range_y,
                    sps.wp_offset_half_range_y - 1);
            }
            if (chroma_weight_flag[i]) {
                sum_weight_l1_flags += 2;
                for (int j = 0; j < 2; ++j) {
                    READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_weight_l1[i][j]);
                    IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_weight_l1[i][j],
                        -128, 127);
                    READ_SE_OR_RETURN(&pred_weight_table->delta_chroma_offset_l1[i][j]);
                    IN_RANGE_OR_RETURN(pred_weight_table->delta_chroma_offset_l1[i][j],
                        -4 * sps.wp_offset_half_range_c,
                        4 * sps.wp_offset_half_range_c - 1);
                }
            }
        }
        TRUE_OR_RETURN(sum_weight_l0_flags + sum_weight_l1_flags <= 24);
    }

    return kOk;
}


HEVCParser::Result HEVCParser::ParseAndIgnoreHrdParameters(
    bool common_inf_present_flag,
    int max_num_sub_layers_minus1) {
    Result res = kOk;
    int data;
    READ_BOOL_OR_RETURN(&data); // present_flag
    if (!data) {
        return res;
    }

    bool nal_hrd_parameters_present_flag = false;
    bool vcl_hrd_parameters_present_flag = false;
    bool sub_pic_hrd_params_present_flag = false;
    if (common_inf_present_flag) {
        READ_BOOL_OR_RETURN(&nal_hrd_parameters_present_flag);
        READ_BOOL_OR_RETURN(&vcl_hrd_parameters_present_flag);
        if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
            READ_BOOL_OR_RETURN(&sub_pic_hrd_params_present_flag);
            if (sub_pic_hrd_params_present_flag) {
                SKIP_BITS_OR_RETURN(8); // tick_divisor_minus2
                SKIP_BITS_OR_RETURN(5); // du_cpb_removal_delay_increment_length_minus1
                SKIP_BITS_OR_RETURN(1); // sub_pic_cpb_params_in_pic_timing_sei_flag
                SKIP_BITS_OR_RETURN(5); // dpb_output_delay_du_length_minus1
            }
            SKIP_BITS_OR_RETURN(4); // bit_rate_scale;
            SKIP_BITS_OR_RETURN(4); // cpb_size_scale;
            if (sub_pic_hrd_params_present_flag) {
                SKIP_BITS_OR_RETURN(4); // cpb_size_du_scale
            }
            SKIP_BITS_OR_RETURN(5); // initial_cpb_removal_delay_length_minus1
            SKIP_BITS_OR_RETURN(5); // au_cpb_removal_delay_length_minus1
            SKIP_BITS_OR_RETURN(5); // dpb_output_delay_length_minus1
        }
    }
    for (int i = 0; i <= max_num_sub_layers_minus1; ++i) {
        bool fixed_pic_rate_flag;
        READ_BOOL_OR_RETURN(&fixed_pic_rate_flag); // general
        if (!fixed_pic_rate_flag) {
            READ_BOOL_OR_RETURN(&fixed_pic_rate_flag); // within_cvs
        }
        bool low_delay_hrd_flag = false;
        if (fixed_pic_rate_flag) {
            READ_UE_OR_RETURN(&data); // elemental_duration_in_tc_minus1
        } else {
            READ_BOOL_OR_RETURN(&low_delay_hrd_flag);
        }
        int cpb_cnt = 1;
        if (!low_delay_hrd_flag) {
            READ_UE_OR_RETURN(&cpb_cnt);
            IN_RANGE_OR_RETURN(cpb_cnt, 0, 31);
            cpb_cnt += 1; // parsed as minus1
        }
        if (nal_hrd_parameters_present_flag) {
            res = ParseAndIgnoreSubLayerHrdParameters(
                cpb_cnt, sub_pic_hrd_params_present_flag);
            if (res != kOk) {
                return res;
            }
        }
        if (vcl_hrd_parameters_present_flag) {
            res = ParseAndIgnoreSubLayerHrdParameters(
                cpb_cnt, sub_pic_hrd_params_present_flag);
            if (res != kOk) {
                return res;
            }
        }
    }
    return res;
}

HEVCParser::Result HEVCParser::ParseVuiParameters(const H265SPS &sps, H265VUIParameters *vui) {
    Result res = kOk;
    bool aspect_ratio_info_present_flag;
    READ_BOOL_OR_RETURN(&aspect_ratio_info_present_flag);
    if (aspect_ratio_info_present_flag) {
        int aspect_ratio_idc;
        READ_BITS_OR_RETURN(8, &aspect_ratio_idc);
        constexpr int kExtendedSar = 255;
        if (aspect_ratio_idc == kExtendedSar) {
            READ_BITS_OR_RETURN(16, &vui->sar_width);
            READ_BITS_OR_RETURN(16, &vui->sar_height);
        } else {
            const int max_aspect_ratio_idc = sizeof(kTableSarWidth) / sizeof(kTableSarWidth[0]) - 1; // std::size(kTableSarWidth) - 1;
            IN_RANGE_OR_RETURN(aspect_ratio_idc, 0, max_aspect_ratio_idc);
            vui->sar_width = kTableSarWidth[aspect_ratio_idc];
            vui->sar_height = kTableSarHeight[aspect_ratio_idc];
        }
    }

    int data;
    // Read and ignore overscan info.
    READ_BOOL_OR_RETURN(&data); // overscan_info_present_flag
    if (data) {
        SKIP_BITS_OR_RETURN(1); // overscan_appropriate_flag
    }

    bool video_signal_type_present_flag;
    READ_BOOL_OR_RETURN(&video_signal_type_present_flag);
    if (video_signal_type_present_flag) {
        SKIP_BITS_OR_RETURN(3); // video_format
        READ_BOOL_OR_RETURN(&vui->video_full_range_flag);
        READ_BOOL_OR_RETURN(&vui->colour_description_present_flag);
        if (vui->colour_description_present_flag) {
            // color description syntax elements
            READ_BITS_OR_RETURN(8, &vui->colour_primaries);
            READ_BITS_OR_RETURN(8, &vui->transfer_characteristics);
            READ_BITS_OR_RETURN(8, &vui->matrix_coeffs);
        }
    }

    READ_BOOL_OR_RETURN(&data); // chroma_loc_info_present_flag
    if (data) {
        READ_UE_OR_RETURN(&data); // chroma_sample_loc_type_top_field
        READ_UE_OR_RETURN(&data); // chroma_sample_loc_type_bottom_field
    }

    // Ignore neutral_chroma_indication_flag, field_seq_flag and
    // frame_field_info_present_flag.
    SKIP_BITS_OR_RETURN(3);

    bool default_display_window_flag;
    READ_BOOL_OR_RETURN(&default_display_window_flag);
    if (default_display_window_flag) {
        READ_UE_OR_RETURN(&vui->def_disp_win_left_offset);
        READ_UE_OR_RETURN(&vui->def_disp_win_right_offset);
        READ_UE_OR_RETURN(&vui->def_disp_win_top_offset);
        READ_UE_OR_RETURN(&vui->def_disp_win_bottom_offset);
    }

    // Read and ignore timing info.
    READ_BOOL_OR_RETURN(&data); // timing_info_present_flag
    if (data) {
        SKIP_BITS_OR_RETURN(32); // vui_num_units_in_tick
        SKIP_BITS_OR_RETURN(32); // vui_time_scale
        READ_BOOL_OR_RETURN(&data); // vui_poc_proportional_to_timing_flag
        if (data) {
            READ_UE_OR_RETURN(&data); // vui_num_ticks_poc_diff_one_minus1
        }
        res = ParseAndIgnoreHrdParameters(true, sps.sps_max_sub_layers_minus1);
        if (res != kOk) {
            return res;
        }
    }

    READ_BOOL_OR_RETURN(&vui->bitstream_restriction_flag);
    if (vui->bitstream_restriction_flag) {
        // Skip tiles_fixed_structure_flag, motion_vectors_over_pic_boundaries_flag
        // and restricted_ref_pic_lists_flag.
        SKIP_BITS_OR_RETURN(3);
        READ_UE_OR_RETURN(&vui->min_spatial_segmentation_idc);
        READ_UE_OR_RETURN(&vui->max_bytes_per_pic_denom);
        READ_UE_OR_RETURN(&vui->max_bits_per_min_cu_denom);
        READ_UE_OR_RETURN(&vui->log2_max_mv_length_horizontal);
        READ_UE_OR_RETURN(&vui->log2_max_mv_length_vertical);
    }

    return kOk;
}


HEVCParser::Result HEVCParser::ParseVPS(H265VPS **pvps) {
    DVLOG(4) << "Parsing VPS";
    Result res = kOk;

    auto vps = new H265VPS();
    *pvps = vps;

    READ_BITS_OR_RETURN(4, &vps->vps_video_parameter_set_id);
    IN_RANGE_OR_RETURN(vps->vps_video_parameter_set_id, 0, 16);
    READ_BOOL_OR_RETURN(&vps->vps_base_layer_internal_flag);
    READ_BOOL_OR_RETURN(&vps->vps_base_layer_available_flag);
    READ_BITS_OR_RETURN(6, &vps->vps_max_layers_minus1);
    IN_RANGE_OR_RETURN(vps->vps_max_layers_minus1, 0, 62);
    READ_BITS_OR_RETURN(3, &vps->vps_max_sub_layers_minus1);
    IN_RANGE_OR_RETURN(vps->vps_max_sub_layers_minus1, 0, 7);
    READ_BOOL_OR_RETURN(&vps->vps_temporal_id_nesting_flag);
    SKIP_BITS_OR_RETURN(16); // vps_reserved_0xffff_16bits
    res = ParseProfileTierLevel(true, vps->vps_max_sub_layers_minus1,
        &vps->profile_tier_level);
    if (res != kOk) {
        return res;
    }

    bool vps_sub_layer_ordering_info_present_flag;
    READ_BOOL_OR_RETURN(&vps_sub_layer_ordering_info_present_flag);

    for (int i = vps_sub_layer_ordering_info_present_flag
             ? 0
             : vps->vps_max_sub_layers_minus1;
         i <= vps->vps_max_sub_layers_minus1; ++i) {
        READ_UE_OR_RETURN(&vps->vps_max_dec_pic_buffering_minus1[i]);
        IN_RANGE_OR_RETURN(vps->vps_max_dec_pic_buffering_minus1[i], 0, 15);
        READ_UE_OR_RETURN(&vps->vps_max_num_reorder_pics[i]);
        IN_RANGE_OR_RETURN(vps->vps_max_num_reorder_pics[i], 0,
            vps->vps_max_dec_pic_buffering_minus1[i]);
        if (i > 0) {
            TRUE_OR_RETURN(vps->vps_max_dec_pic_buffering_minus1[i] >= vps->vps_max_dec_pic_buffering_minus1[i - 1]);
            TRUE_OR_RETURN(vps->vps_max_num_reorder_pics[i] >= vps->vps_max_num_reorder_pics[i - 1]);
        }
        READ_UE_OR_RETURN(&vps->vps_max_latency_increase_plus1[i]);
    }
    if (!vps_sub_layer_ordering_info_present_flag) {
        for (int i = 0; i < vps->vps_max_sub_layers_minus1; ++i) {
            vps->vps_max_dec_pic_buffering_minus1[i] = vps->vps_max_dec_pic_buffering_minus1[vps->vps_max_sub_layers_minus1];
            vps->vps_max_num_reorder_pics[i] = vps->vps_max_num_reorder_pics[vps->vps_max_sub_layers_minus1];
            vps->vps_max_latency_increase_plus1[i] = vps->vps_max_latency_increase_plus1[vps->vps_max_sub_layers_minus1];
        }
    }

    READ_BITS_OR_RETURN(6, &vps->vps_max_layer_id);
    IN_RANGE_OR_RETURN(vps->vps_max_layer_id, 0, 62);
    READ_UE_OR_RETURN(&vps->vps_num_layer_sets_minus1);
    IN_RANGE_OR_RETURN(vps->vps_num_layer_sets_minus1, 0, 1023);

    // If an VPS with the same id already exists, replace it.
    //*vps_id = vps->vps_video_parameter_set_id;
    // active_vps_[*vps_id] = std::move(vps);

    return res;
}

// Code below is Copyright 2023 Jamscape ApS. All rights reserved.

bool HEVCParser::Parse(const uint8_t *bytes, size_t compressed_size, decode_callback_t cb, void *opaque) {

#if 0
    for (size_t i = 0; i < compressed_size; ++i) {
        printf("%02x,", bytes[i]);
    }
    printf("\n");
#endif

    const size_t max_buffer = 0x200000;
    auto buffer = new uint8_t[max_buffer]();
    if (compressed_size > max_buffer) {
        printf("%s buffer full, dropping %zu bytes of input\n",
            __PRETTY_FUNCTION__, compressed_size);
        return false;
    }

    /* we no longer support streaming in NALUs, so keep the state
     * variable here instead of in the class definition. */
    static size_t buffer_size = 0;
    static size_t nalu_start_len = 0;
    static size_t nalu_zeros = 0;

    auto p = buffer;
    // uint8_t *enc_buffer = new uint8_t[max_buffer]();
    // size_t enc_size = 0;
    bool have_frame = false;

    for (size_t i = 0; i < compressed_size + 5; ++i) {
        int c;
        if (i < compressed_size) {
            c = bytes[i];
        } else {
            /* add fake NALU header at end to trigger decode of last unit */
            if (i == compressed_size + 3) {
                c = 1;
            } else {
                c = 0;
            }
        }

        if (nalu_start_len) {
            if (buffer_size > nalu_start_len) {
                size_t size = buffer_size - nalu_start_len;

                // Initialize bit reader at the start of found NALU.
                br_.Initialize(p, size);

                // Read NALU header, skip the forbidden_zero_bit, but check for it.
                int data;
                READ_BITS_OR_RETURN(1, &data);
                TRUE_OR_RETURN(data == 0);

                READ_BITS_OR_RETURN(6, &nalu.nal_unit_type);
                READ_BITS_OR_RETURN(6, &nalu.nuh_layer_id);
                READ_BITS_OR_RETURN(3, &nalu.nuh_temporal_id_plus1);

                unsigned hevc_type = nalu.nal_unit_type;
                unsigned avc_type = p[0] & 0x1f;

                if (hevc_type == NAL_UNIT_H265_VPS) {
                    is_hevc = true;
                    if (ParseVPS(&vps)) {
                        errx(1, "ParseVPS failed");
                    }
                } else if (hevc_type == NAL_UNIT_H265_SPS) {
                    is_hevc = true;
                    if (ParseSPS(&sps)) {
                        errx(1, "ParseSPS failed");
                    }
                } else if (hevc_type == NAL_UNIT_H265_PPS) {
                    is_hevc = true;
                    if (ParsePPS(sps, &pps)) {
                        errx(1, "ParsePPS failed");
                    }
                } else if (hevc_type == H265NALU::Type::IDR_N_LP ||
                    hevc_type == H265NALU::Type::TRAIL_N ||
                    hevc_type == H265NALU::Type::TRAIL_R ||
                    hevc_type == H265NALU::Type::IDR_W_RADL ||
                    hevc_type == H265NALU::Type::CRA_NUT //||
                    //hevc_type == H265NALU::Type::PREFIX_SEI_NUT
                        ) {
                    if (vps && sps && pps) {
                        printf("coded slice type=%u sz=%zu nalu_start_len=%zu\n", hevc_type, size, nalu_start_len);
                        is_hevc = true;
                        if (ParseSliceHeader(nalu, &shdr1, nullptr)) {
                            errx(1, "ParsePPS failed");
                        }
                        have_frame = true;
                        cb(p, size, opaque);
                    }
                } else if (hevc_type == H265NALU::Type::PREFIX_SEI_NUT) {
                    printf("%s: PREFIX_SEI_NUT not handled!\n", __PRETTY_FUNCTION__);
                } else if (!is_hevc && avc_type == NAL_UNIT_H264_SPS) {
                    errx(1, "%s: unhandled line %d\n", __PRETTY_FUNCTION__, __LINE__);
                } else if (!is_hevc && avc_type == NAL_UNIT_H264_PPS) {
                    errx(1, "%s: unhandled line %d\n", __PRETTY_FUNCTION__, __LINE__);
                } else {
                    printf("IGNORING nal %u (0x%x) sz=%zu\n", hevc_type, hevc_type, size);
                    errx(1, "%s: unhandled line %d\n", __PRETTY_FUNCTION__, __LINE__);
#if 0
                    uint32_t size_network = std::HToNL(size);
                    memcpy(buffer, &size_network, sizeof(uint32_t));
                    memcpy(enc_buffer + enc_size, buffer, sizeof(uint32_t) + size);
                    enc_size += sizeof(uint32_t) + size;
#endif
                }
            }
            buffer_size = 0;
        }
        p[buffer_size++] = c;
        nalu_start_len = (nalu_zeros >= 2 && c == 1) ? nalu_zeros + 1 : 0;
        nalu_zeros = c ? 0 : nalu_zeros + 1;
    }

    // if (enc_size) {
    //  have_frame = impl->HandleEncoding(enc_buffer, enc_size);
    //}
    // delete[] enc_buffer;
    delete[] buffer;
    return have_frame;
}
