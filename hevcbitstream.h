#undef NDEBUG

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#endif

class HEVCBitStream {

    int frame_width;
    int frame_height;

public:

    enum FrameType {
        FRAME_I = 1,
        FRAME_P = 2,
        FRAME_B = 3,
        FRAME_IDR = 7
    };

    enum SliceType {
        SLICE_B = 0,
        SLICE_P = 1,
        SLICE_I = 2,
    };

    enum NALUType {
        NALU_TRAIL_N = 0x00, // Coded slice segment of a non-TSA, non-STSA trailing picture - slice_segment_layer_rbsp, VLC
        NALU_TRAIL_R = 0x01, // Coded slice segment of a non-TSA, non-STSA trailing picture - slice_segment_layer_rbsp, VLC
        NALU_TSA_N = 0x02, // Coded slice segment of a TSA picture - slice_segment_layer_rbsp, VLC
        NALU_TSA_R = 0x03, // Coded slice segment of a TSA picture - slice_segment_layer_rbsp, VLC
        NALU_STSA_N = 0x04, // Coded slice of an STSA picture - slice_layer_rbsp, VLC
        NALU_STSA_R = 0x05, // Coded slice of an STSA picture - slice_layer_rbsp, VLC
        NALU_RADL_N = 0x06, // Coded slice of an RADL picture - slice_layer_rbsp, VLC
        NALU_RADL_R = 0x07, // Coded slice of an RADL picture - slice_layer_rbsp, VLC
        NALU_RASL_N = 0x08, // Coded slice of an RASL picture - slice_layer_rbsp, VLC
        NALU_RASL_R = 0x09, // Coded slice of an RASL picture - slice_layer_rbsp, VLC
        /* 0x0a..0x0f - Reserved */
        NALU_BLA_W_LP = 0x10, // Coded slice segment of an BLA picture - slice_segment_layer_rbsp, VLC
        NALU_BLA_W_DLP = 0x11, // Coded slice segment of an BLA picture - slice_segment_layer_rbsp, VLC
        NALU_BLA_N_LP = 0x12, // Coded slice segment of an BLA picture - slice_segment_layer_rbsp, VLC
        NALU_IDR_W_DLP = 0x13, // Coded slice segment of an IDR picture - slice_segment_layer_rbsp, VLC
        NALU_IDR_N_LP = 0x14, // Coded slice segment of an IDR picture - slice_segment_layer_rbsp, VLC
        NALU_CRA = 0x15, // Coded slice segment of an CRA picture - slice_segment_layer_rbsp, VLC
        NALU_RSV_IRAP_VCL23 = 0x17,
        /* 0x16..0x1f - Reserved */
        NALU_VPS = 0x20, // Video parameter set - video_parameter_set_rbsp, non-VLC
        NALU_SPS = 0x21, // Sequence parameter set - seq_parameter_set_rbsp, non-VLC
        NALU_PPS = 0x22, // Picture parameter set - pic_parameter_set_rbsp, non-VLC
        NALU_AUD = 0x23, // Access unit delimiter - access_unit_delimiter_rbsp, non-VLC
        NALU_EOS = 0x24, // End of sequence - end_of_seq_rbsp, non-VLC
        NALU_EOB = 0x25, // End of bitsteam - end_of_bitsteam_rbsp, non-VLC
        NALU_FD = 0x26, // Filler data - filler_data_rbsp, non-VLC
        NALU_PREFIX_SEI = 0x27, // Supplemental enhancement information (SEI) - sei_rbsp, non_VLC
        NALU_SUFFIX_SEI = 0x28, // Supplemental enhancement information (SEI) - sei_rbsp, non_VLC
        /* 0x29..0x2f - Reserved */
        /* 0x30..0x3f - Unspecified */
        // this should be the last element of this enum
        // chagne this value if NAL unit type increased
        MAX_HEVC_NAL_TYPE = 0x3f,

    };

    static const uint32_t MAX_TEMPORAL_SUBLAYERS = 8;
    static const uint32_t MAX_LAYER_ID = 64;
    static const uint32_t MAX_LONGTERM_REF_PIC = 32;
    static const uint32_t NUM_OF_EXTRA_SLICEHEADER_BITS = 3;

    struct ProfileTierParamSet {
        uint8_t general_profile_space; // u(2)
        int general_tier_flag; // u(1)
        uint8_t general_profile_idc; // u(5)
        int general_profile_compatibility_flag[32]; // u(1)
        int general_progressive_source_flag; // u(1)
        int general_interlaced_source_flag; // u(1)
        int general_non_packed_constraint_flag; // u(1)
        int general_frame_only_constraint_flag; // u(1)
        int general_reserved_zero_43bits[43]; // u(1)
        int general_reserved_zero_bit; // u(1)
        uint8_t general_level_idc; // u(8)
    };
    // Video parameter set structure
    struct VideoParamSet {
        uint8_t vps_video_parameter_set_id; // u(4)
        int vps_base_layer_internal_flag; // u(1)
        int vps_base_layer_available_flag; // u(1)
        uint8_t vps_max_layers_minus1; // u(6)
        uint8_t vps_max_sub_layers_minus1; // u(3)
        int vps_temporal_id_nesting_flag; // u(1)
        uint16_t vps_reserved_0xffff_16bits; // u(16)

        struct ProfileTierParamSet ptps;
        uint8_t vps_max_nuh_reserved_zero_layer_id;
        uint32_t vps_max_op_sets;
        uint32_t vps_num_op_sets_minus1;

        int vps_sub_layer_ordering_info_present_flag; // u(1)
        uint32_t vps_max_dec_pic_buffering_minus1[MAX_TEMPORAL_SUBLAYERS]; // ue(v)
        uint32_t vps_max_num_reorder_pics[MAX_TEMPORAL_SUBLAYERS]; // ue(v)
        uint32_t vps_max_latency_increase_plus1[MAX_TEMPORAL_SUBLAYERS]; // ue(v)
        uint8_t vps_max_layer_id; // u(6)
        uint32_t vps_num_layer_sets_minus1; // ue(v)
        int layer_id_included_flag[MAX_TEMPORAL_SUBLAYERS][MAX_LAYER_ID]; // u(1)
        int vps_timing_info_present_flag; // u(1)
        uint32_t vps_num_units_in_tick; // u(32)
        uint32_t vps_time_scale; // u(32
        int vps_poc_proportional_to_timing_flag; // u(1)
        uint32_t vps_num_ticks_poc_diff_one_minus1; // ue(v)
        uint32_t vps_num_hrd_parameters; // ue(v)
        uint32_t hrd_layer_set_idx[MAX_TEMPORAL_SUBLAYERS]; // ue(v)
        int cprms_present_flag[MAX_TEMPORAL_SUBLAYERS]; // u(1)
        int vps_extension_flag; // u(1)
        int vps_extension_data_flag; // u(1)
    };

    struct ShortTermRefPicParamSet {
        int inter_ref_pic_set_prediction_flag; // u(1)
        uint32_t delta_idx_minus1; // ue(v)
        uint8_t delta_rps_sign; // u(1)
        uint32_t abs_delta_rps_minus1; // ue(v)
        uint8_t used_by_curr_pic_flag[32]; // u(1)
        uint8_t use_delta_flag[32]; // u(1)
        uint32_t num_negative_pics; // ue(v)
        uint32_t num_positive_pics; // ue(v)
        uint32_t delta_poc_s0_minus1[32]; // ue(v)
        uint8_t used_by_curr_pic_s0_flag[32]; // u(1)
        uint32_t delta_poc_s1_minus1[32]; // ue(v)
        uint8_t used_by_curr_pic_s1_flag[32]; // u(1)
    };
    struct SeqParamSet {
        uint8_t sps_video_parameter_set_id; // u(4)
        uint8_t sps_max_sub_layers_minus1; // u(3)
        int sps_temporal_id_nesting_flag; // u(1)

        struct ProfileTierParamSet ptps;
        uint32_t sps_seq_parameter_set_id; // ue(v)
        uint32_t chroma_format_idc; // ue(v)
        int separate_colour_plane_flag; // u(1)
        uint32_t pic_width_in_luma_samples; // ue(v)
        uint32_t pic_height_in_luma_samples; // ue(v)
        int conformance_window_flag; // u(1)
        uint32_t conf_win_left_offset; // ue(v)
        uint32_t conf_win_right_offset; // ue(v)
        uint32_t conf_win_top_offset; // ue(v)
        uint32_t conf_win_bottom_offset; // ue(v)
        uint32_t bit_depth_luma_minus8; // ue(v)
        uint32_t bit_depth_chroma_minus8; // ue(v)
        uint32_t log2_max_pic_order_cnt_lsb_minus4; // ue(v)
        int sps_sub_layer_ordering_info_present_flag; // u(1)
        uint32_t sps_max_dec_pic_buffering_minus1[MAX_TEMPORAL_SUBLAYERS]; // ue(v)
        uint32_t sps_max_num_reorder_pics[MAX_TEMPORAL_SUBLAYERS]; // ue(v)
        uint32_t sps_max_latency_increase_plus1[MAX_TEMPORAL_SUBLAYERS]; // ue(v)
        uint32_t log2_min_luma_coding_block_size_minus3; // ue(v)
        uint32_t log2_diff_max_min_luma_coding_block_size;
        uint32_t log2_max_coding_block_size_minus3; // ue(v)
        uint32_t log2_min_luma_transform_block_size_minus2; // ue(v)
        uint32_t log2_diff_max_min_luma_transform_block_size; // ue(v)
        uint32_t max_transform_hierarchy_depth_inter; // ue(v)
        uint32_t max_transform_hierarchy_depth_intra; // ue(v)
        uint8_t scaling_list_enabled_flag; // u(1)
        uint8_t sps_scaling_list_data_present_flag; // u(1)
        uint8_t amp_enabled_flag; // u(1)
        uint8_t sample_adaptive_offset_enabled_flag; // u(1)
        uint8_t pcm_enabled_flag; // u(1)
        uint8_t pcm_sample_bit_depth_luma_minus1; // u(4)
        uint8_t pcm_sample_bit_depth_chroma_minus1; // u(4)
        uint32_t log2_min_pcm_luma_coding_block_size_minus3;
        uint32_t log2_max_pcm_luma_coding_block_size_minus3; // ue(v)
        uint32_t log2_diff_max_min_pcm_luma_coding_block_size; // ue(v)
        uint8_t pcm_loop_filter_disabled_flag; // u(1)
        uint32_t num_short_term_ref_pic_sets; // ue(v)

        struct ShortTermRefPicParamSet strp[66];
        uint8_t long_term_ref_pics_present_flag; // u(1)
        uint32_t num_long_term_ref_pics_sps; // ue(v)
        uint32_t lt_ref_pic_poc_lsb_sps[MAX_LONGTERM_REF_PIC]; // u(v)
        uint8_t used_by_curr_pic_lt_sps_flag[MAX_LONGTERM_REF_PIC]; // u(1)
        uint8_t sps_temporal_mvp_enabled_flag; // u(1)
        uint8_t strong_intra_smoothing_enabled_flag; // u(1)
        uint8_t vui_parameters_present_flag; // u(1)
        // VuiParameters   vui_parameters;
        int sps_extension_present_flag; // u(1)
        int sps_range_extension_flag; // u(1)
        int sps_multilayer_extension_flag; // u(1)
        int sps_3d_extension_flag; // u(1)
        uint8_t sps_extension_5bits; // u(5)
        int sps_extension_data_flag; // u(1)
    };
    struct PicParamSet {
        uint32_t pps_pic_parameter_set_id; // ue(v)
        uint32_t pps_seq_parameter_set_id; // ue(v)
        int dependent_slice_segments_enabled_flag; // u(1)
        int output_flag_present_flag; // u(1)
        uint8_t num_extra_slice_header_bits; // u(3)
        int sign_data_hiding_enabled_flag; // u(1)
        int cabac_init_present_flag; // u(1)
        uint32_t num_ref_idx_l0_default_active_minus1; // ue(v)
        uint32_t num_ref_idx_l1_default_active_minus1; // ue(v)
        int32_t init_qp_minus26; // se(v)
        int constrained_intra_pred_flag; // u(1)
        int transform_skip_enabled_flag; // u(1)
        int cu_qp_delta_enabled_flag; // u(1)
        uint32_t diff_cu_qp_delta_depth; // ue(v)
        uint32_t pps_cb_qp_offset; // se(v)
        uint32_t pps_cr_qp_offset; // se(v)
        int pps_slice_chroma_qp_offsets_present_flag; // u(1)
        int weighted_pred_flag; // u(1)
        int weighted_bipred_flag; // u(1)
        int transquant_bypass_enabled_flag; // u(1)
        int tiles_enabled_flag; // u(1)
        int entropy_coding_sync_enabled_flag; // u(1)
        uint32_t num_tile_columns_minus1; // ue(v)
        uint32_t num_tile_rows_minus1; // ue(v)
        int uniform_spacing_flag; // u(1)
        uint32_t *column_width_minus1; // ue(v)
        uint32_t *row_height_minus1; // ue(v)
        int loop_filter_across_tiles_enabled_flag; // u(1)
        int pps_loop_filter_across_slices_enabled_flag; // u(1)
        int deblocking_filter_control_present_flag; // u(1)
        int deblocking_filter_override_enabled_flag; // u(1)
        int pps_deblocking_filter_disabled_flag; // u(1)
        int32_t pps_beta_offset_div2; // se(v)
        int32_t pps_tc_offset_div2; // se(v)
        int pps_scaling_list_data_present_flag; // u(1)
        int lists_modification_present_flag; // u(1)
        uint32_t log2_parallel_merge_level_minus2; // ue(v)
        int slice_segment_header_extension_present_flag; // u(1)
        int pps_extension_present_flag; // u(1)
        int pps_range_extension_flag; // u(1)
        int pps_multilayer_extension_flag; // u(1)
        int pps_3d_extension_flag; // u(1)
        uint8_t pps_extension_5bits; // u(5)
        uint8_t pps_extension_data_flag; // u(1)
        uint32_t log2_max_transform_skip_block_size_minus2; // ue(v)
        uint8_t cross_component_prediction_enabled_flag; // ue(1)
        uint8_t chroma_qp_offset_list_enabled_flag; // ue(1)
        uint32_t diff_cu_chroma_qp_offset_depth; // ue(v)
        uint32_t chroma_qp_offset_list_len_minus1; // ue(v)
        uint32_t cb_qp_offset_list[6]; // se(v)
        uint32_t cr_qp_offset_list[6]; // se(v)
        uint32_t log2_sao_offset_scale_luma; // ue(v)
        uint32_t log2_sao_offset_scale_chroma; // ue(v)
    };
    struct SliceHeader {
        int first_slice_segment_in_pic_flag; // u(1)
        int no_output_of_prior_pics_flag; // u(1)
        uint32_t slice_pic_parameter_set_id; // ue(v)
        int dependent_slice_segment_flag; // u(1)
        uint32_t picture_width_in_ctus;
        uint32_t picture_height_in_ctus;
        uint32_t slice_segment_address; // u(v)
        int slice_reserved_undetermined_flag[NUM_OF_EXTRA_SLICEHEADER_BITS]; // u(1)
        SliceType slice_type; // ue(v)
        int pic_output_flag; // u(1)
        uint8_t colour_plane_id; // u(2)
        uint32_t pic_order_cnt_lsb;
        uint32_t num_negative_pics;
        uint32_t num_positive_pics;
        uint32_t delta_poc_s0_minus1;

        struct ShortTermRefPicParamSet strp;
        int short_term_ref_pic_set_sps_flag; // u(1)
        uint32_t short_term_ref_pic_set_idx; // u(v)
        uint32_t num_long_term_sps; // ue(v)
        uint32_t num_long_term_pics; // ue(v)
        uint32_t *lt_idx_sps; // u(v)
        uint32_t *poc_lsb_lt; // u(v)
        int *used_by_curr_pic_lt_flag; // u(1)
        int *delta_poc_msb_present_flag; // u(1)
        uint32_t *delta_poc_msb_cycle_lt; // ue(v)
        int slice_temporal_mvp_enabled_flag; // u(1)
        int slice_sao_luma_flag; // u(1)
        int slice_sao_chroma_flag; // u(1)
        int num_ref_idx_active_override_flag; // u(1)
        uint32_t num_ref_idx_l0_active_minus1; // ue(v)
        uint32_t num_ref_idx_l1_active_minus1;
        uint32_t num_poc_total_cur;
        int ref_pic_list_modification_flag_l0;
        int ref_pic_list_modification_flag_l1;
        uint32_t *list_entry_l0;
        uint32_t *list_entry_l1;

        int ref_pic_list_combination_flag;

        uint32_t num_ref_idx_lc_active_minus1;
        uint32_t ref_pic_list_modification_flag_lc;
        int pic_from_list_0_flag;
        uint32_t ref_idx_list_curr;
        int mvd_l1_zero_flag; // u(1)
        int cabac_init_present_flag;
        int pic_temporal_mvp_enable_flag;

        int collocated_from_l0_flag; // u(1)
        uint32_t collocated_ref_idx; // ue(v)
        uint32_t five_minus_max_num_merge_cand; // ue(v)
        int32_t delta_pic_order_cnt_bottom; // se(v)
        int32_t slice_qp_delta; // se(v)
        int32_t slice_qp_delta_cb; // se(v)
        int32_t slice_qp_delta_cr; // se(v)
        int cu_chroma_qp_offset_enabled_flag; // u(1)
        int deblocking_filter_override_flag; // u(1)
        int disable_deblocking_filter_flag; // u(1)
        int32_t beta_offset_div2; // se(v)
        int32_t tc_offset_div2; // se(v)
        int slice_loop_filter_across_slices_enabled_flag; // u(1)
        uint32_t num_entry_point_offsets; // ue(v)
        uint32_t offset_len_minus1; // ue(v)
        uint32_t *entry_point_offset; // u(v)
        uint32_t slice_segment_header_extension_length; // ue(v)
        uint8_t *slice_segment_header_extension_data_byte; // u(8)
    };

    VideoParamSet vps = {};
    SeqParamSet sps = {};
    PicParamSet pps = {};
    SliceHeader ssh = {};
    ProfileTierParamSet protier_param = {};

    static const int p2b = 1;
    static const uint32_t MaxPicOrderCntLsb = (2 << 8);
    static const unsigned int num_active_ref_p = 1;
    static const int initial_qp = 26;
    static const int minimal_qp = 0;
    static const int intra_period = 30000;
    static const int intra_idr_period = 30;
    static const int ip_period = 1;

#if 0
    int rc_mode = -1;
    int rc_default_modes[] = {
        VA_RC_VBR,
        VA_RC_CQP,
        VA_RC_VBR_CONSTRAINED,
        VA_RC_CBR,
        VA_RC_VCM,
        VA_RC_NONE,
    };
#endif

    static const int BITSTREAM_ALLOCATE_STEPPING = 4096;
    static const int LCU_SIZE = 64;

private:
    uint32_t *buffer = nullptr;
    int bit_offset = 0;
public:
    int max_size_in_dword = 0;
    uint64_t current_frame_display = 0;
    uint64_t current_IDR_display = 0;

    void
    bitstream_start() {
        max_size_in_dword = BITSTREAM_ALLOCATE_STEPPING;
        buffer = (uint32_t *) calloc(max_size_in_dword * sizeof(int), 1);
        assert(buffer);
        this->bit_offset = 0;
    }

    void
    bitstream_end() {
        int pos = (this->bit_offset >> 5);
        int bit_offset = (this->bit_offset & 0x1f);
        int bit_left = 32 - bit_offset;

        if (bit_offset) {
            buffer[pos] = htonl(buffer[pos] << bit_left);
        }
    }

    void
    put_ui(uint32_t val, int size_in_bits) {
        int pos = (this->bit_offset >> 5);
        int bit_offset = (this->bit_offset & 0x1f);
        int bit_left = 32 - bit_offset;

        if (!size_in_bits) {
            return;
        }

        this->bit_offset += size_in_bits;

        if (bit_left > size_in_bits) {
            buffer[pos] = (buffer[pos] << size_in_bits | val);
        } else {
            size_in_bits -= bit_left;
            buffer[pos] = (buffer[pos] << bit_left) | (val >> size_in_bits);
            buffer[pos] = htonl(buffer[pos]);

            if (pos + 1 == this->max_size_in_dword) {
                errx(1, "grow buffer");
                this->max_size_in_dword += BITSTREAM_ALLOCATE_STEPPING;
                buffer = (uint32_t *) realloc(buffer, max_size_in_dword * sizeof(uint32_t));
                assert(buffer);
            }

            buffer[pos + 1] = val;
        }
    }

    void
    put_ue(uint32_t val) {
        int size_in_bits = 0;
        int tmp_val = ++val;

        while (tmp_val) {
            tmp_val >>= 1;
            size_in_bits++;
        }

        put_ui(0, size_in_bits - 1); // leading zero
        put_ui(val, size_in_bits);
    }

    void
    put_se(int val) {
        uint32_t new_val;

        if (val <= 0) {
            new_val = -2 * val;
        } else {
            new_val = 2 * val - 1;
        }

        put_ue(new_val);
    }

    void
    byte_aligning(int bit) {
        int bit_offset = (this->bit_offset & 0x7);
        int bit_left = 8 - bit_offset;
        int new_val;

        if (!bit_offset) {
            return;
        }

        assert(bit == 0 || bit == 1);

        if (bit) {
            new_val = (1 << bit_left) - 1;
        } else {
            new_val = 0;
        }

        put_ui(new_val, bit_left);
    }

    void
    rbsp_trailing_bits() {
        put_ui(1, 1);
        byte_aligning(0);
    }

    void nal_start_code_prefix(int nal_unit_type) {
        if (nal_unit_type == NALU_VPS || nal_unit_type == NALU_SPS || nal_unit_type == NALU_PPS || nal_unit_type == NALU_AUD) {
            put_ui(0x00000001, 32);
        } else {
            put_ui(0x000001, 24);
        }
    }

    void nal_header(int nal_unit_type) {
        put_ui(0, 1); /* forbidden_zero_bit: 0 */
        put_ui(nal_unit_type, 6);
        put_ui(0, 6);
        put_ui(1, 3);
    }

    static int calc_poc(FrameType frame_type, int pic_order_cnt_lsb) {
        static int picOrderCntMsb_ref = 0, pic_order_cnt_lsb_ref = 0;
        int prevPicOrderCntMsb, prevPicOrderCntLsb;
        int picOrderCntMsb, picOrderCnt;

        if (frame_type == FRAME_IDR) {
            prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
        } else {
            prevPicOrderCntMsb = picOrderCntMsb_ref;
            prevPicOrderCntLsb = pic_order_cnt_lsb_ref;
        }

        if ((pic_order_cnt_lsb < prevPicOrderCntLsb) && ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (int) (MaxPicOrderCntLsb / 2))) {
            picOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
        } else if ((pic_order_cnt_lsb > prevPicOrderCntLsb) && ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (int) (MaxPicOrderCntLsb / 2))) {
            picOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
        } else {
            picOrderCntMsb = prevPicOrderCntMsb;
        }

        picOrderCnt = picOrderCntMsb + pic_order_cnt_lsb;

        if (frame_type != FRAME_B) {
            picOrderCntMsb_ref = picOrderCntMsb;
            pic_order_cnt_lsb_ref = pic_order_cnt_lsb;
        }

        return picOrderCnt;
    }

    void fill_profile_tier_level(
        uint8_t vps_max_layers_minus1,
        struct ProfileTierParamSet *ptps) {

        memset(ptps, 0, sizeof(*ptps));

        ptps->general_profile_space = 0;
        ptps->general_tier_flag = 0;
        ptps->general_profile_idc = 1;//VAProfileHEVCMain;
        memset(ptps->general_profile_compatibility_flag, 0, 32 * sizeof(int));
        ptps->general_profile_compatibility_flag[ptps->general_profile_idc] = 1;
        ptps->general_progressive_source_flag = 1;
        ptps->general_interlaced_source_flag = 0;
        ptps->general_non_packed_constraint_flag = 0;
        ptps->general_frame_only_constraint_flag = 1;

        ptps->general_level_idc = 30;
        ptps->general_level_idc = ptps->general_level_idc * 4;
    }

public:
    void fill_vps_header() {
        memset(&vps, 0, sizeof(vps));

        vps.vps_video_parameter_set_id = 0;
        vps.vps_base_layer_internal_flag = 1;
        vps.vps_base_layer_available_flag = 1;
        vps.vps_max_layers_minus1 = 0;
        vps.vps_max_sub_layers_minus1 = 0; // max temporal layer minus 1
        vps.vps_temporal_id_nesting_flag = 1;
        vps.vps_reserved_0xffff_16bits = 0xFFFF;
        // hevc::ProfileTierParamSet ptps;
        memset(&vps.ptps, 0, sizeof(vps.ptps));
        fill_profile_tier_level(vps.vps_max_layers_minus1, &protier_param);
        vps.vps_sub_layer_ordering_info_present_flag = 0;
        for (int i = 0; i < MAX_TEMPORAL_SUBLAYERS; i++) {
            vps.vps_max_dec_pic_buffering_minus1[i] = intra_period == 1 ? 1 : 6;
            vps.vps_max_num_reorder_pics[i] = ip_period != 0 ? ip_period - 1 : 0;
            vps.vps_max_latency_increase_plus1[i] = 0;
        }
        vps.vps_max_layer_id = 0;
        vps.vps_num_layer_sets_minus1 = 0;
        vps.vps_sub_layer_ordering_info_present_flag = 0;
        vps.vps_max_nuh_reserved_zero_layer_id = 0;
        vps.vps_max_op_sets = 1;
        vps.vps_timing_info_present_flag = 0;
        vps.vps_extension_flag = 0;
    }

    void fill_short_term_ref_pic_header(
        struct ShortTermRefPicParamSet *strp,
        uint8_t strp_index) {
        uint32_t i = 0;
        // inter_ref_pic_set_prediction_flag is always 0 now
        strp->inter_ref_pic_set_prediction_flag = 0;
        /* don't need to set below parameters since inter_ref_pic_set_prediction_flag equal to 0
        strp->delta_idx_minus1 should be set to 0 since strp_index != num_short_term_ref_pic_sets in sps
        strp->delta_rps_sign;
        strp->abs_delta_rps_minus1;
        strp->used_by_curr_pic_flag[j];
        strp->use_delta_flag[j];
        */
        strp->num_negative_pics = num_active_ref_p;
        int num_positive_pics = ip_period > 1 ? 1 : 0;
        strp->num_positive_pics = strp_index == 0 ? 0 : num_positive_pics;

        if (strp_index == 0) {
            for (i = 0; i < strp->num_negative_pics; i++) {
                strp->delta_poc_s0_minus1[i] = ip_period - 1;
                strp->used_by_curr_pic_s0_flag[i] = 1;
            }
        } else {
            for (i = 0; i < strp->num_negative_pics; i++) {
                strp->delta_poc_s0_minus1[i] = (i == 0) ? (strp_index - 1) : (ip_period - 1);
                strp->used_by_curr_pic_s0_flag[i] = 1;
            }
            for (i = 0; i < strp->num_positive_pics; i++) {
                strp->delta_poc_s1_minus1[i] = ip_period - 1 - strp_index;
                strp->used_by_curr_pic_s1_flag[i] = 1;
            }
        }
    }

    void fill_sps_header(int id) {
        memset(&sps, 0, sizeof(sps));

        sps.sps_temporal_id_nesting_flag = 1;
        fill_profile_tier_level(sps.sps_max_sub_layers_minus1, &sps.ptps);
        sps.sps_seq_parameter_set_id = id;
        sps.chroma_format_idc = 1;
#define ALIGN16(x) ((x + 15) & ~15)
        auto frame_width_aligned = ALIGN16(frame_width);
        auto frame_height_aligned = ALIGN16(frame_height);
        sps.pic_width_in_luma_samples = frame_width_aligned;
        sps.pic_height_in_luma_samples = frame_height_aligned;
        if (frame_width_aligned != frame_width || frame_height_aligned != frame_height) {
            printf("w %d != %d || h %d != %d\n", frame_width_aligned, frame_width, frame_height_aligned, frame_height);
            sps.conformance_window_flag = 1;
            // (sps.chroma_format_idc set to 1 => 4:2:0 format
            sps.conf_win_right_offset = (frame_width_aligned - frame_width) >> 1;
            sps.conf_win_bottom_offset = (frame_height_aligned - frame_height) >> 1;
        }

        sps.log2_max_pic_order_cnt_lsb_minus4 = std::max((ceil(log(ip_period - 1 + 4) / log(2.0)) + 3), 4.0) - 4;
        for (int i = 0; i < MAX_TEMPORAL_SUBLAYERS; i++) {
            sps.sps_max_dec_pic_buffering_minus1[i] = intra_period == 1 ? 1 : 6;
            sps.sps_max_num_reorder_pics[i] = ip_period != 0 ? ip_period - 1 : 0;
        }
        int log2_max_luma_coding_block_size = (int) log2(LCU_SIZE);
        int log2_min_luma_coding_block_size = sps.log2_min_luma_coding_block_size_minus3 + 3;
        sps.log2_diff_max_min_luma_coding_block_size = log2_max_luma_coding_block_size - log2_min_luma_coding_block_size;
        sps.log2_diff_max_min_luma_transform_block_size = 3;
        sps.max_transform_hierarchy_depth_inter = 2;
        sps.max_transform_hierarchy_depth_intra = 2;
        sps.amp_enabled_flag = 1;
        sps.sample_adaptive_offset_enabled_flag = 1;
        sps.num_short_term_ref_pic_sets = ip_period;

        memset(&sps.strp[0], 0, sizeof(sps.strp));
        for (uint32_t i = 0; i < std::min(sps.num_short_term_ref_pic_sets, 64U); ++i) {
            fill_short_term_ref_pic_header(&sps.strp[i], i);
        }
        sps.sps_temporal_mvp_enabled_flag = 1;
    }

    uint8_t convert_12cusize_to_pixel_size_hevc(const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE &cuSize) {
        switch (cuSize) {
        case D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_8x8: {
            return 8u;
        } break;
        case D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_16x16: {
            return 16u;
        } break;
        case D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_32x32: {
            return 32u;
        } break;
        case D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_CUSIZE_64x64: {
            return 64u;
        } break;
        default: {
            errx(1, "Not a supported cu size");
        } break;
        }
    }

    uint8_t convert_12tusize_to_pixel_size_hevc(const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE &TUSize) {
        switch (TUSize) {
        case D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_4x4: {
            return 4u;
        } break;
        case D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_8x8: {
            return 8u;
        } break;
        case D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_16x16: {
            return 16u;
        } break;
        case D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_TUSIZE_32x32: {
            return 32u;
        } break;
        default: {
            errx(1, "Not a supported TU size");
        } break;
        }
    }

        void convert_from_d3d12_level_hevc(D3D12_VIDEO_ENCODER_LEVELS_HEVC level12, uint32_t &specLevel) {
        specLevel = 3u;
        switch (level12) {
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_1: {
            specLevel *= 10;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_2: {
            specLevel *= 20;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_21: {
            specLevel *= 21;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_3: {
            specLevel *= 30;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_31: {
            specLevel *= 31;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_4: {
            specLevel *= 40;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_41: {
            specLevel *= 41;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_5: {
            specLevel *= 50;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_51: {
            specLevel *= 51;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_52: {
            specLevel *= 52;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_6: {
            specLevel *= 60;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_61: {
            specLevel *= 61;
        } break;
        case D3D12_VIDEO_ENCODER_LEVELS_HEVC_62: {
            specLevel *= 62;
        } break;
        default: {
            errx(1, "Unsupported D3D12_VIDEO_ENCODER_LEVELS_HEVC value");
        } break;
        }
    }

    void init_profile_tier_level(ProfileTierParamSet *ptl,
        uint8_t HEVCProfileIdc,
        uint8_t HEVCLevelIdc,
        bool isHighTier) {
        memset(ptl, 0, sizeof(*ptl));

        ptl->general_profile_space = 0; // must be 0
        ptl->general_tier_flag = isHighTier ? 1 : 0;
        ptl->general_profile_idc = HEVCProfileIdc;

        memset(ptl->general_profile_compatibility_flag, 0, sizeof(ptl->general_profile_compatibility_flag));
        ptl->general_profile_compatibility_flag[ptl->general_profile_idc] = 1;

        ptl->general_progressive_source_flag = 1; // yes
        ptl->general_interlaced_source_flag = 0; // no
        ptl->general_non_packed_constraint_flag = 1; // no frame packing arrangement SEI messages
        ptl->general_frame_only_constraint_flag = 1;
        ptl->general_level_idc = HEVCLevelIdc;
    }

    void build_vps(uint8_t vps_video_parameter_set_id, const D3D12_VIDEO_ENCODER_LEVEL_TIER_CONSTRAINTS_HEVC &level) {
        uint8_t HEVCProfileIdc = 1;//convert_profile12_to_stdprofile(profile);
        uint32_t HEVCLevelIdc = 0u;
        convert_from_d3d12_level_hevc(level.Level, HEVCLevelIdc);
        bool isHighTier = false;
        uint8_t maxRefFrames = 1;
        bool gopHasBFrames = false;

        memset(&vps, 0, sizeof(vps));
        vps.vps_video_parameter_set_id = vps_video_parameter_set_id,
        //vps.vps_reserved_three_2bits = 3u;
        vps.vps_max_layers_minus1 = 0u;
        vps.vps_max_sub_layers_minus1 = 0u;
        vps.vps_temporal_id_nesting_flag = 1u;
        //vps.vps_reserved_0xffff_16bits = 0xFFFF;
        //init_profile_tier_level(&vps.ptl, HEVCProfileIdc, HEVCLevelIdc, isHighTier);
        init_profile_tier_level(&protier_param, HEVCProfileIdc, HEVCLevelIdc, isHighTier);
        vps.vps_sub_layer_ordering_info_present_flag = 0u;
        for (int i = (vps.vps_sub_layer_ordering_info_present_flag ? 0 : vps.vps_max_sub_layers_minus1); i <= vps.vps_max_sub_layers_minus1; i++) {
            vps.vps_max_dec_pic_buffering_minus1[i] = (maxRefFrames /*previous reference frames*/ + 1 /*additional current frame recon pic*/) - 1 /**minus1 for header*/;
            vps.vps_max_num_reorder_pics[i] = gopHasBFrames ? vps.vps_max_dec_pic_buffering_minus1[i] : 0;
            vps.vps_max_latency_increase_plus1[i] = 0; // When vps_max_latency_increase_plus1[ i ] is equal to 0, no corresponding limit is expressed.
        }
    }

    void build_sps(
        uint8_t seq_parameter_set_id,
        const uint32_t picDimensionMultipleRequirement,
        const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC &codecConfig,
        const D3D12_VIDEO_ENCODER_SEQUENCE_GOP_STRUCTURE_HEVC &hevcGOP) {

        memset(&sps, 0, sizeof(sps));

        uint8_t minCuSize = convert_12cusize_to_pixel_size_hevc(codecConfig.MinLumaCodingUnitSize);
        uint8_t maxCuSize = convert_12cusize_to_pixel_size_hevc(codecConfig.MaxLumaCodingUnitSize);
        uint8_t minTuSize = convert_12tusize_to_pixel_size_hevc(codecConfig.MinLumaTransformUnitSize);
        uint8_t maxTuSize = convert_12tusize_to_pixel_size_hevc(codecConfig.MaxLumaTransformUnitSize);

        sps.sps_seq_parameter_set_id = seq_parameter_set_id;
        sps.sps_max_sub_layers_minus1 = vps.vps_max_sub_layers_minus1;
        sps.sps_temporal_id_nesting_flag = vps.vps_temporal_id_nesting_flag;

        // inherit PTL from parentVPS fully
        sps.ptps = protier_param;

        sps.chroma_format_idc = 1; // 420

        // Codec spec dictates pic_width/height_in_luma_samples must be divisible by minCuSize but HW might have higher req pow 2 multiples
        assert((picDimensionMultipleRequirement % minCuSize) == 0u);

        // upper layer passes the viewport, can calculate the difference between it and pic_width_in_luma_samples
        auto viewport_width = frame_width;//crop_window_upper_layer.front /* passes height */ - ((crop_window_upper_layer.left + crop_window_upper_layer.right) << 1);
        auto viewport_height = frame_height;//crop_window_upper_layer.back /* passes width */ - ((crop_window_upper_layer.top + crop_window_upper_layer.bottom) << 1);

        uint8_t mask = picDimensionMultipleRequirement - 1;
        sps.pic_width_in_luma_samples = (frame_width + mask) & ~mask;
        sps.pic_height_in_luma_samples = (frame_height + mask) & ~mask;
        sps.conf_win_right_offset = (sps.pic_width_in_luma_samples - viewport_width) >> 1;
        sps.conf_win_bottom_offset = (sps.pic_height_in_luma_samples - viewport_height) >> 1;
        sps.conformance_window_flag = sps.conf_win_left_offset || sps.conf_win_right_offset || sps.conf_win_top_offset || sps.conf_win_bottom_offset;

        sps.log2_max_pic_order_cnt_lsb_minus4 = hevcGOP.log2_max_pic_order_cnt_lsb_minus4;

        sps.sps_sub_layer_ordering_info_present_flag = vps.vps_sub_layer_ordering_info_present_flag;
        for (int i = (sps.sps_sub_layer_ordering_info_present_flag ? 0 : sps.sps_max_sub_layers_minus1); i <= sps.sps_max_sub_layers_minus1; i++) {
            sps.sps_max_dec_pic_buffering_minus1[i] = vps.vps_max_dec_pic_buffering_minus1[i];
            sps.sps_max_num_reorder_pics[i] = vps.vps_max_num_reorder_pics[i];
            sps.sps_max_latency_increase_plus1[i] = vps.vps_max_latency_increase_plus1[i];
        }

        sps.log2_min_luma_coding_block_size_minus3 = static_cast<uint8_t>(log2(minCuSize) - 3);
        sps.log2_diff_max_min_luma_coding_block_size = static_cast<uint8_t>(log2(maxCuSize) - log2(minCuSize));
        sps.log2_min_luma_transform_block_size_minus2 = static_cast<uint8_t>(log2(minTuSize) - 2);
        sps.log2_diff_max_min_luma_transform_block_size = static_cast<uint8_t>(log2(maxTuSize) - log2(minTuSize));

        sps.max_transform_hierarchy_depth_inter = codecConfig.max_transform_hierarchy_depth_inter;
        sps.max_transform_hierarchy_depth_intra = codecConfig.max_transform_hierarchy_depth_intra;

        sps.scaling_list_enabled_flag = 0;
        sps.amp_enabled_flag = ((codecConfig.ConfigurationFlags & D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_USE_ASYMETRIC_MOTION_PARTITION) != 0) ? 1u : 0u;
        sps.sample_adaptive_offset_enabled_flag = ((codecConfig.ConfigurationFlags & D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_ENABLE_SAO_FILTER) != 0) ? 1u : 0u;
        sps.pcm_enabled_flag = 0;

        sps.num_short_term_ref_pic_sets = 0;

        sps.long_term_ref_pics_present_flag = ((codecConfig.ConfigurationFlags & D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_ENABLE_LONG_TERM_REFERENCES) != 0) ? 1u : 0u;
        sps.num_long_term_ref_pics_sps = 0; // signal through slice header for now

    }

    void build_pps(uint8_t pic_parameter_set_id,
        const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC &codecConfig,
        const D3D12_VIDEO_ENCODER_PICTURE_CONTROL_CODEC_DATA_HEVC &pictureControl) {
        memset(&pps, 0, sizeof(pps));

        pps.pps_pic_parameter_set_id = pic_parameter_set_id;
        pps.pps_seq_parameter_set_id = sps.sps_seq_parameter_set_id;

        pps.weighted_pred_flag = 0u; // no weighted prediction in D3D12

        //ssh.num_ref_idx_l0_active_minus1 = pps.num_ref_idx_l0_default_active_minus1;
        pps.num_ref_idx_l0_default_active_minus1 = static_cast<uint8_t>(std::max(static_cast<INT>(pictureControl.List0ReferenceFramesCount) - 1, 0));
        pps.num_ref_idx_l1_default_active_minus1 = static_cast<uint8_t>(std::max(static_cast<INT>(pictureControl.List1ReferenceFramesCount) - 1, 0));

        pps.num_tile_columns_minus1 = 0u; // no tiling in D3D12
        pps.num_tile_rows_minus1 = 0u; // no tiling in D3D12
        pps.tiles_enabled_flag = 0u; // no tiling in D3D12
        pps.loop_filter_across_tiles_enabled_flag = 0;

        pps.lists_modification_present_flag = 0;
        pps.log2_parallel_merge_level_minus2 = 0;

        pps.deblocking_filter_control_present_flag = 1;
        pps.deblocking_filter_override_enabled_flag = 0;
        pps.pps_deblocking_filter_disabled_flag = 0;
        pps.pps_scaling_list_data_present_flag = 0;
        pps.pps_beta_offset_div2 = 0;
        pps.pps_tc_offset_div2 = 0;
        pps.pps_loop_filter_across_slices_enabled_flag = ((codecConfig.ConfigurationFlags & D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_DISABLE_LOOP_FILTER_ACROSS_SLICES) != 0) ? 0 : 1;
        pps.transform_skip_enabled_flag = ((codecConfig.ConfigurationFlags & D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_ENABLE_TRANSFORM_SKIPPING) != 0) ? 1 : 0;
        pps.constrained_intra_pred_flag = ((codecConfig.ConfigurationFlags & D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION_HEVC_FLAG_USE_CONSTRAINED_INTRAPREDICTION) != 0) ? 1 : 0;
        pps.cabac_init_present_flag = 1;
        pps.pps_slice_chroma_qp_offsets_present_flag = 1;
        pps.cu_qp_delta_enabled_flag = 1;

    }

    void fill_pps_header(
        uint32_t pps_id,
        uint32_t sps_id) {

        memset(&pps, 0, sizeof(pps));

        pps.pps_pic_parameter_set_id = pps_id;
        pps.pps_seq_parameter_set_id = sps_id;
        pps.cabac_init_present_flag = 1;

        pps.init_qp_minus26 = initial_qp - 26;
#if 0
        pps.cu_qp_delta_enabled_flag = 1;
        if (pps.cu_qp_delta_enabled_flag) {
            pps.diff_cu_qp_delta_depth = 2;
        }
#endif

        pps.deblocking_filter_control_present_flag = 1;
        pps.pps_beta_offset_div2 = 2;
    }

    void fill_slice_header(FrameType frame_type, uint32_t numShortTerm) {
        memset(&ssh, 0, sizeof(ssh));
        ssh.pic_output_flag = 1;
        ssh.pic_order_cnt_lsb = calc_poc(frame_type, (current_frame_display - current_IDR_display) % MaxPicOrderCntLsb);
    //++current_frame_display; //XXX

        // slice_segment_address (u(v))
        ssh.picture_height_in_ctus = (frame_height + LCU_SIZE - 1) / LCU_SIZE;
        ssh.picture_width_in_ctus = (frame_width + LCU_SIZE - 1) / LCU_SIZE;
        ssh.first_slice_segment_in_pic_flag = ((ssh.slice_segment_address == 0) ? 1 : 0);
        ssh.slice_type = frame_type == FRAME_P ? (p2b ? SLICE_B : SLICE_P) : frame_type == FRAME_B ? SLICE_B
                                                                                                   : SLICE_I;

        ssh.short_term_ref_pic_set_sps_flag = 1;
        ssh.short_term_ref_pic_set_idx = ssh.pic_order_cnt_lsb % ip_period;
        ssh.strp.num_negative_pics = numShortTerm;
        ssh.slice_temporal_mvp_enabled_flag = 1;

        ssh.num_ref_idx_l0_active_minus1 = pps.num_ref_idx_l0_default_active_minus1;
        ssh.num_ref_idx_l1_active_minus1 = pps.num_ref_idx_l1_default_active_minus1;

        // for I slice
        if (frame_type == FRAME_I || frame_type == FRAME_IDR) {
        } else {
            ssh.ref_pic_list_modification_flag_l0 = 1;
            ssh.num_poc_total_cur = 2;
        }

        ssh.slice_qp_delta_cb = pps.pps_cb_qp_offset;
        ssh.slice_qp_delta_cr = pps.pps_cr_qp_offset;

        ssh.tc_offset_div2 = pps.pps_tc_offset_div2;
        ssh.beta_offset_div2 = pps.pps_beta_offset_div2;

        ssh.collocated_from_l0_flag = 1;
        ssh.collocated_ref_idx = pps.num_ref_idx_l0_default_active_minus1;
    }

    void protier_rbsp() {
        uint32_t i = 0;
        put_ui(protier_param.general_profile_space, 2);
        put_ui(protier_param.general_tier_flag, 1);
        put_ui(protier_param.general_profile_idc, 5);

        for (i = 0; i < 32; i++) {
            put_ui(protier_param.general_profile_compatibility_flag[i], 1);
        }

        put_ui(protier_param.general_progressive_source_flag, 1);
        put_ui(protier_param.general_interlaced_source_flag, 1);
        put_ui(protier_param.general_non_packed_constraint_flag, 1);
        put_ui(protier_param.general_frame_only_constraint_flag, 1);
        put_ui(0, 16);
        put_ui(0, 16);
        put_ui(0, 12);
        put_ui(protier_param.general_level_idc, 8);
    }

    void pack_short_term_ref_pic_setp(
        struct ShortTermRefPicParamSet *strp,
        int first_strp) {
        uint32_t i = 0;
        if (!first_strp) {
            put_ui(strp->inter_ref_pic_set_prediction_flag, 1);
        }

        // inter_ref_pic_set_prediction_flag is always 0 now
        put_ue(strp->num_negative_pics);
        put_ue(strp->num_positive_pics);

        for (i = 0; i < strp->num_negative_pics; i++) {
            put_ue(strp->delta_poc_s0_minus1[i]);
            put_ui(strp->used_by_curr_pic_s0_flag[i], 1);
        }
        for (i = 0; i < strp->num_positive_pics; i++) {
            put_ue(strp->delta_poc_s1_minus1[i]);
            put_ui(strp->used_by_curr_pic_s1_flag[i], 1);
        }
    }

    void vps_rbsp() {
        uint32_t i = 0;
        put_ui(vps.vps_video_parameter_set_id, 4);
        put_ui(3, 2); // vps_reserved_three_2bits
        put_ui(0, 6); // vps_reserved_zero_6bits

        put_ui(vps.vps_max_sub_layers_minus1, 3);
        put_ui(vps.vps_temporal_id_nesting_flag, 1);
        put_ui(0xFFFF, 16); // vps_reserved_0xffff_16bits
        protier_rbsp();

        put_ui(vps.vps_sub_layer_ordering_info_present_flag, 1);

        for (i = (vps.vps_sub_layer_ordering_info_present_flag ? 0 : vps.vps_max_sub_layers_minus1); i <= vps.vps_max_sub_layers_minus1; i++) {
            // NOTE: In teddi and mv_encoder, the setting is max_dec_pic_buffering.
            // here just follow the spec 7.3.2.1
            put_ue(vps.vps_max_dec_pic_buffering_minus1[i]);
            put_ue(vps.vps_max_num_reorder_pics[i]);
            put_ue(vps.vps_max_latency_increase_plus1[i]);
        }

        put_ui(vps.vps_max_nuh_reserved_zero_layer_id, 6);
        put_ue(vps.vps_num_op_sets_minus1);

        put_ui(vps.vps_timing_info_present_flag, 1);

        if (vps.vps_timing_info_present_flag) {
            put_ue(vps.vps_num_units_in_tick);
            put_ue(vps.vps_time_scale);
            put_ue(vps.vps_poc_proportional_to_timing_flag);
            if (vps.vps_poc_proportional_to_timing_flag) {
                put_ue(vps.vps_num_ticks_poc_diff_one_minus1);
            }
            put_ue(vps.vps_num_hrd_parameters);
            for (i = 0; i < vps.vps_num_hrd_parameters; i++) {
                put_ue(vps.hrd_layer_set_idx[i]);
                if (i > 0) {
                    put_ui(vps.cprms_present_flag[i], 1);
                }
            }
        }

        // no extension flag
        put_ui(0, 1);
    }

    void sps_rbsp() {
        uint32_t i = 0;
        put_ui(sps.sps_video_parameter_set_id, 4);
        put_ui(sps.sps_max_sub_layers_minus1, 3);
        put_ui(sps.sps_temporal_id_nesting_flag, 1);

        protier_rbsp();

        put_ue(sps.sps_seq_parameter_set_id);
        put_ue(sps.chroma_format_idc);

        if (sps.chroma_format_idc == 3) {
            errx(1, "separate_colour_plane_flag");
            put_ui(sps.separate_colour_plane_flag, 1);
        }
        put_ue(sps.pic_width_in_luma_samples);
        put_ue(sps.pic_height_in_luma_samples);

        put_ui(sps.conformance_window_flag, 1);

        if (sps.conformance_window_flag) {
            put_ue(sps.conf_win_left_offset);
            put_ue(sps.conf_win_right_offset);
            put_ue(sps.conf_win_top_offset);
            put_ue(sps.conf_win_bottom_offset);
        }
        put_ue(sps.bit_depth_luma_minus8);
        put_ue(sps.bit_depth_chroma_minus8);
        put_ue(sps.log2_max_pic_order_cnt_lsb_minus4);
        put_ui(sps.sps_sub_layer_ordering_info_present_flag, 1);

        for (i = (sps.sps_sub_layer_ordering_info_present_flag ? 0 : sps.sps_max_sub_layers_minus1); i <= sps.sps_max_sub_layers_minus1; i++) {
            // NOTE: In teddi and mv_encoder, the setting is max_dec_pic_buffering.
            // here just follow the spec 7.3.2.2
            //assert(sps.sps_max_dec_pic_buffering_minus1[i] == 6);
            put_ue(sps.sps_max_dec_pic_buffering_minus1[i]);
            put_ue(sps.sps_max_num_reorder_pics[i]);
            put_ue(sps.sps_max_latency_increase_plus1[i]);
        }

        put_ue(sps.log2_min_luma_coding_block_size_minus3);
        put_ue(sps.log2_diff_max_min_luma_coding_block_size);
        put_ue(sps.log2_min_luma_transform_block_size_minus2);
        put_ue(sps.log2_diff_max_min_luma_transform_block_size);
        put_ue(sps.max_transform_hierarchy_depth_inter);
        put_ue(sps.max_transform_hierarchy_depth_intra);

        // scaling_list_enabled_flag is set as 0 in fill_sps_header() for now
        put_ui(sps.scaling_list_enabled_flag, 1);
        if (sps.scaling_list_enabled_flag) {
            put_ui(sps.sps_scaling_list_data_present_flag, 1);
            if (sps.sps_scaling_list_data_present_flag) {
                // scaling_list_data();
            }
        }

        put_ui(sps.amp_enabled_flag, 1);
        put_ui(sps.sample_adaptive_offset_enabled_flag, 1);

        // pcm_enabled_flag is set as 0 in fill_sps_header() for now
        put_ui(sps.pcm_enabled_flag, 1);
        if (sps.pcm_enabled_flag) {
            put_ui(sps.pcm_sample_bit_depth_luma_minus1, 4);
            put_ui(sps.pcm_sample_bit_depth_chroma_minus1, 4);
            put_ue(sps.log2_min_pcm_luma_coding_block_size_minus3);
            put_ue(sps.log2_diff_max_min_pcm_luma_coding_block_size);
            put_ui(sps.pcm_loop_filter_disabled_flag, 1);
        }

        put_ue(sps.num_short_term_ref_pic_sets);
        for (i = 0; i < sps.num_short_term_ref_pic_sets; i++) {
            pack_short_term_ref_pic_setp(&sps.strp[i], i == 0);
        }

        // long_term_ref_pics_present_flag is set as 0 in fill_sps_header() for now
        put_ui(sps.long_term_ref_pics_present_flag, 1);
        if (sps.long_term_ref_pics_present_flag) {
            put_ue(sps.num_long_term_ref_pics_sps);
            for (i = 0; i < sps.num_long_term_ref_pics_sps; i++) {
                put_ue(sps.lt_ref_pic_poc_lsb_sps[i]);
                put_ui(sps.used_by_curr_pic_lt_sps_flag[i], 1);
            }
        }

        put_ui(sps.sps_temporal_mvp_enabled_flag, 1);
        put_ui(sps.strong_intra_smoothing_enabled_flag, 1);

        // vui_parameters_present_flag is set as 0 in fill_sps_header() for now
        put_ui(sps.vui_parameters_present_flag, 1);

        put_ui(sps.sps_extension_present_flag, 1);
    }

    void pps_rbsp() {
        uint32_t i = 0;
        put_ue(pps.pps_pic_parameter_set_id);
        put_ue(pps.pps_seq_parameter_set_id);
        put_ui(pps.dependent_slice_segments_enabled_flag, 1);
        put_ui(pps.output_flag_present_flag, 1);
        put_ui(pps.num_extra_slice_header_bits, 3);
        put_ui(pps.sign_data_hiding_enabled_flag, 1);
        put_ui(pps.cabac_init_present_flag, 1);

        put_ue(pps.num_ref_idx_l0_default_active_minus1);
        put_ue(pps.num_ref_idx_l1_default_active_minus1);
        put_se(pps.init_qp_minus26);

        put_ui(pps.constrained_intra_pred_flag, 1);
        put_ui(pps.transform_skip_enabled_flag, 1);

        put_ui(pps.cu_qp_delta_enabled_flag, 1);
        if (pps.cu_qp_delta_enabled_flag) {
            put_ue(pps.diff_cu_qp_delta_depth);
        }

        put_se(pps.pps_cb_qp_offset);
        put_se(pps.pps_cr_qp_offset);

        put_ui(pps.pps_slice_chroma_qp_offsets_present_flag, 1);
        put_ui(pps.weighted_pred_flag, 1);
        put_ui(pps.weighted_bipred_flag, 1);
        put_ui(pps.transquant_bypass_enabled_flag, 1);
        put_ui(pps.tiles_enabled_flag, 1);
        put_ui(pps.entropy_coding_sync_enabled_flag, 1);

        if (pps.tiles_enabled_flag) {
            put_ue(pps.num_tile_columns_minus1);
            put_ue(pps.num_tile_rows_minus1);
            put_ui(pps.uniform_spacing_flag, 1);
            if (!pps.uniform_spacing_flag) {
                for (i = 0; i < pps.num_tile_columns_minus1; i++) {
                    put_ue(pps.column_width_minus1[i]);
                }

                for (i = 0; i < pps.num_tile_rows_minus1; i++) {
                    put_ue(pps.row_height_minus1[i]);
                }
            }
            put_ui(pps.loop_filter_across_tiles_enabled_flag, 1);
        }

        put_ui(pps.pps_loop_filter_across_slices_enabled_flag, 1);
        put_ui(pps.deblocking_filter_control_present_flag, 1);
        if (pps.deblocking_filter_control_present_flag) {
            put_ui(pps.deblocking_filter_override_enabled_flag, 1);
            put_ui(pps.pps_deblocking_filter_disabled_flag, 1);
            if (!pps.pps_deblocking_filter_disabled_flag) {
                put_se(pps.pps_beta_offset_div2);
                put_se(pps.pps_tc_offset_div2);
            }
        }

        // pps_scaling_list_data_present_flag is set as 0 in fill_pps_header() for now
        put_ui(pps.pps_scaling_list_data_present_flag, 1);
        if (pps.pps_scaling_list_data_present_flag) {
            // scaling_list_data();
        }

        put_ui(pps.lists_modification_present_flag, 1);
        put_ue(pps.log2_parallel_merge_level_minus2);
        put_ui(pps.slice_segment_header_extension_present_flag, 1);

        put_ui(pps.pps_extension_present_flag, 1);
        if (pps.pps_extension_present_flag) {
            put_ui(pps.pps_range_extension_flag, 1);
            put_ui(pps.pps_multilayer_extension_flag, 1);
            put_ui(pps.pps_3d_extension_flag, 1);
            put_ui(pps.pps_extension_5bits, 1);
        }

        if (pps.pps_range_extension_flag) {
            if (pps.transform_skip_enabled_flag) {
                put_ue(pps.log2_max_transform_skip_block_size_minus2);
            }
            put_ui(pps.cross_component_prediction_enabled_flag, 1);
            put_ui(pps.chroma_qp_offset_list_enabled_flag, 1);

            if (pps.chroma_qp_offset_list_enabled_flag) {
                put_ue(pps.diff_cu_chroma_qp_offset_depth);
                put_ue(pps.chroma_qp_offset_list_len_minus1);
                for (i = 0; i <= pps.chroma_qp_offset_list_len_minus1; i++) {
                    put_ue(pps.cb_qp_offset_list[i]);
                    put_ue(pps.cr_qp_offset_list[i]);
                }
            }

            put_ue(pps.log2_sao_offset_scale_luma);
            put_ue(pps.log2_sao_offset_scale_chroma);
        }
    }
    void sliceHeader_rbsp(
        struct SliceHeader *slice_header,
        struct SeqParamSet *sps,
        struct PicParamSet *pps,
        int isidr) {
        uint8_t nal_unit_type = NALU_TRAIL_R;
        int gop_ref_distance = ip_period;
        int incomplete_mini_gop = 0;
        int p_slice_flag = 1;
        int i = 0;

        put_ui(slice_header->first_slice_segment_in_pic_flag, 1);
        if (slice_header->pic_order_cnt_lsb == 0) {
            nal_unit_type = NALU_IDR_W_DLP;
        }

        if (nal_unit_type >= 16 && nal_unit_type <= 23) {
            put_ui(slice_header->no_output_of_prior_pics_flag, 1);
        }

        put_ue(slice_header->slice_pic_parameter_set_id);

        if (!slice_header->first_slice_segment_in_pic_flag) {
            if (slice_header->dependent_slice_segment_flag) {
                put_ui(slice_header->dependent_slice_segment_flag, 1);
            }

            put_ui(slice_header->slice_segment_address,
                (uint8_t) (ceil(log(slice_header->picture_height_in_ctus * slice_header->picture_width_in_ctus) / log(2.0))));
        }
        if (!slice_header->dependent_slice_segment_flag) {
            for (i = 0; i < pps->num_extra_slice_header_bits; i++) {
                put_ui(slice_header->slice_reserved_undetermined_flag[i], 1);
            }
            put_ue(slice_header->slice_type);
            if (pps->output_flag_present_flag) {
                put_ui(slice_header->pic_output_flag, 1);
            }
            if (sps->separate_colour_plane_flag == 1) {
                put_ui(slice_header->colour_plane_id, 2);
            }

            if (!(nal_unit_type == NALU_IDR_W_DLP || nal_unit_type == NALU_IDR_N_LP)) {
                put_ui(slice_header->pic_order_cnt_lsb, (sps->log2_max_pic_order_cnt_lsb_minus4 + 4));
                put_ui(slice_header->short_term_ref_pic_set_sps_flag, 1);

                if (!slice_header->short_term_ref_pic_set_sps_flag) {
                    // refer to Teddi
                    if (sps->num_short_term_ref_pic_sets > 0) {
                        put_ui(0, 1); // inter_ref_pic_set_prediction_flag, always 0 for now
                    }

                    put_ue(slice_header->strp.num_negative_pics);
                    put_ue(slice_header->strp.num_positive_pics);

                    // below chunks of codes (majorly two big 'for' blocks) are refering both
                    // Teddi and mv_encoder, they look kind of ugly, however, keep them as these
                    // since it will be pretty easy to update if change/update in Teddi side.
                    // According to Teddi, these are CModel Implementation.
                    int prev = 0;
                    int frame_cnt_in_gop = slice_header->pic_order_cnt_lsb / 2;
                    // this is the first big 'for' block
                    for (i = 0; i < slice_header->strp.num_negative_pics; i++) {
                        // Low Delay B case
                        if (1 == gop_ref_distance) {
                            put_ue(0 /*delta_poc_s0_minus1*/);
                        } else {
                            if (incomplete_mini_gop) {
                                if (frame_cnt_in_gop % gop_ref_distance > i) {
                                    put_ue(0 /*delta_poc_s0_minus1*/);
                                } else {
                                    int DeltaPoc = -(int) (gop_ref_distance);
                                    put_ue(prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                                }
                            } else {
                                // For Non-BPyramid GOP i.e B0 type
                                if (num_active_ref_p > 1) {
                                    // MultiRef Case
                                    if (p_slice_flag) {
                                        // DeltaPOC Equals NumB
                                        int DeltaPoc = -(int) (gop_ref_distance);
                                        put_ue(prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                                    } else {
                                        // for normal B
                                        if (frame_cnt_in_gop < gop_ref_distance) {
                                            if (0 == i) {
                                                int DeltaPoc = -(int) (frame_cnt_in_gop);
                                                put_ue(prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                                            }
                                        } else if (frame_cnt_in_gop > gop_ref_distance) {
                                            if (0 == i) {
                                                // Need % to wraparound the delta poc, to avoid corruption caused on POC=5 with GOP (29,2) and 4 refs
                                                int DeltaPoc = -(int) ((frame_cnt_in_gop - gop_ref_distance) % gop_ref_distance);
                                                put_ue(prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                                            } else if (1 <= i) {
                                                int DeltaPoc = -(int) (gop_ref_distance);
                                                put_ue(prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                                            }
                                        }
                                    }
                                } else {
                                    //  the big 'if' wraps here is -
                                    //     if (!slice_header->short_term_ref_pic_set_sps_flag)
                                    // From the Teddi logic, the short_term_ref_pic_set_sps_flag only can be '0'
                                    // either for B-Prymid or first several frames in a GOP in multi-ref cases
                                    // when there are not enough backward refs.
                                    // So though there are really some codes under this 'else'in Teddi, don't
                                    // want to introduce them in MEA to avoid confusion, and put an assert
                                    // here to guard that there is new case we need handle in the future.
                                    assert(0);
                                }
                            }
                        }
                        put_ui(1 /*used_by_curr_pic_s0_flag*/, 1);
                    }

                    prev = 0;
                    // this is the second big 'for' block
                    for (i = 0; i < slice_header->strp.num_positive_pics; i++) {
                        // Non-BPyramid GOP
                        if (num_active_ref_p > 1) {
                            // MultiRef Case
                            if (frame_cnt_in_gop < gop_ref_distance) {
                                int DeltaPoc = (int) (gop_ref_distance - frame_cnt_in_gop);
                                put_ue(DeltaPoc - prev - 1 /*delta_poc_s1_minus1*/);
                            } else if (frame_cnt_in_gop > gop_ref_distance) {
                                int DeltaPoc = (int) (gop_ref_distance * slice_header->strp.num_negative_pics - frame_cnt_in_gop);
                                put_ue(DeltaPoc - prev - 1 /*delta_poc_s1_minus1*/);
                            }
                        } else {
                            //  the big 'if' wraps here is -
                            //     if (!slice_header->short_term_ref_pic_set_sps_flag)
                            // From the Teddi logic, the short_term_ref_pic_set_sps_flag only can be '0'
                            // either for B-Prymid or first several frames in a GOP in multi-ref cases
                            // when there are not enough backward refs.
                            // So though there are really some codes under this 'else'in Teddi, don't
                            // want to introduce them in MEA to avoid confusion, and put an assert
                            // here to guard that there is new case we need handle in the future.
                            assert(0);
                        }
                        put_ui(1 /*used_by_curr_pic_s1_flag*/, 1);
                    }
                } else if (sps->num_short_term_ref_pic_sets > 1) {
                    put_ui(slice_header->short_term_ref_pic_set_idx,
                        (uint8_t) (ceil(log(sps->num_short_term_ref_pic_sets) / log(2.0))));
                }

                if (sps->long_term_ref_pics_present_flag) {
                    if (sps->num_long_term_ref_pics_sps > 0) {
                        put_ue(slice_header->num_long_term_sps);
                    }

                    put_ue(slice_header->num_long_term_pics);
                }

                if (slice_header->slice_temporal_mvp_enabled_flag) {
                    put_ui(slice_header->slice_temporal_mvp_enabled_flag, 1);
                }
            }

            if (sps->sample_adaptive_offset_enabled_flag) {
                put_ui(slice_header->slice_sao_luma_flag, 1);
                put_ui(slice_header->slice_sao_chroma_flag, 1);
            }

            if (slice_header->slice_type != SLICE_I) {
                put_ui(slice_header->num_ref_idx_active_override_flag, 1);

                if (slice_header->num_ref_idx_active_override_flag) {
                    put_ue(slice_header->num_ref_idx_l0_active_minus1);
                    if (slice_header->slice_type == SLICE_B) {
                        put_ue(slice_header->num_ref_idx_l1_active_minus1);
                    }
                }

                if (pps->lists_modification_present_flag && slice_header->num_poc_total_cur > 1) {
                    /* ref_pic_list_modification */
                    put_ui(slice_header->ref_pic_list_modification_flag_l0, 1);

                    if (slice_header->ref_pic_list_modification_flag_l0) {
                        for (i = 0; i <= slice_header->num_ref_idx_l0_active_minus1; i++) {
                            put_ui(slice_header->list_entry_l0[i],
                                (uint8_t) (ceil(log(slice_header->num_poc_total_cur) / log(2.0))));
                        }
                    }

                    put_ui(slice_header->ref_pic_list_modification_flag_l1, 1);

                    if (slice_header->ref_pic_list_modification_flag_l1) {
                        for (i = 0; i <= slice_header->num_ref_idx_l1_active_minus1; i++) {
                            put_ui(slice_header->list_entry_l1[i],
                                (uint8_t) (ceil(log(slice_header->num_poc_total_cur) / log(2.0))));
                        }
                    }
                }

                if (slice_header->slice_type == SLICE_B) {
                    put_ui(slice_header->mvd_l1_zero_flag, 1);
                }

                if (pps->cabac_init_present_flag) {
                    put_ui(slice_header->cabac_init_present_flag, 1);
                }

                if (slice_header->slice_temporal_mvp_enabled_flag) {
                    int collocated_from_l0_flag = 1;

                    if (slice_header->slice_type == SLICE_B) {
                        collocated_from_l0_flag = slice_header->collocated_from_l0_flag;
                        put_ui(slice_header->collocated_from_l0_flag, 1);
                    }

                    if (((collocated_from_l0_flag && (slice_header->num_ref_idx_l0_active_minus1 > 0)) || (!collocated_from_l0_flag && (slice_header->num_ref_idx_l1_active_minus1 > 0)))) {
                        put_ue(slice_header->collocated_ref_idx);
                    }
                }

                put_ue(slice_header->five_minus_max_num_merge_cand);
            }

            put_se(slice_header->slice_qp_delta);

            if (pps->chroma_qp_offset_list_enabled_flag) {
                put_se(slice_header->slice_qp_delta_cb);
                put_se(slice_header->slice_qp_delta_cr);
            }

            if (pps->deblocking_filter_override_enabled_flag) {
                put_ui(slice_header->deblocking_filter_override_flag, 1);
            }
            if (slice_header->deblocking_filter_override_flag) {
                put_ui(slice_header->disable_deblocking_filter_flag, 1);

                if (!slice_header->disable_deblocking_filter_flag) {
                    put_se(slice_header->beta_offset_div2);
                    put_se(slice_header->tc_offset_div2);
                }
            }

            if (pps->pps_loop_filter_across_slices_enabled_flag && (slice_header->slice_sao_luma_flag || slice_header->slice_sao_chroma_flag || !slice_header->disable_deblocking_filter_flag)) {
                put_ui(slice_header->slice_loop_filter_across_slices_enabled_flag, 1);
            }
        }

        if ((pps->tiles_enabled_flag) || (pps->entropy_coding_sync_enabled_flag)) {
            put_ue(slice_header->num_entry_point_offsets);

            if (slice_header->num_entry_point_offsets > 0) {
                put_ue(slice_header->offset_len_minus1);
            }
        }

        if (pps->slice_segment_header_extension_present_flag) {
            int slice_header_extension_length = 0;

            put_ue(slice_header_extension_length);

            for (i = 0; i < slice_header_extension_length; i++) {
                int slice_header_extension_data_byte = 0;
                put_ui(slice_header_extension_data_byte, 8);
            }
        }
    }

    uint32_t build_packed_pic_buffer(uint8_t **pp) {
        bitstream_start();
        nal_start_code_prefix(NALU_PPS);
        nal_header(NALU_PPS);
        pps_rbsp();
        rbsp_trailing_bits();
        bitstream_end();
        *pp = (uint8_t *) buffer;
        buffer = nullptr;
        return bit_offset;
    }

    uint32_t build_packed_video_buffer(uint8_t **pp) {
        bitstream_start();
        nal_start_code_prefix(NALU_VPS);
        nal_header(NALU_VPS);
        vps_rbsp();
        rbsp_trailing_bits();
        bitstream_end();
        *pp = (uint8_t *) buffer;
        buffer = nullptr;
        return bit_offset;
    }

    uint32_t build_packed_seq_buffer(uint8_t **pp) {
        bitstream_start();
        nal_start_code_prefix(NALU_SPS);
        nal_header(NALU_SPS);
        sps_rbsp();
        rbsp_trailing_bits();
        bitstream_end();
        *pp = (uint8_t *) buffer;
        buffer = nullptr;
        return bit_offset;
    }

    uint32_t build_packed_slice_buffer(uint8_t **pp, bool is_idr) {
        int naluType = is_idr ? NALU_IDR_W_DLP : NALU_TRAIL_R;

        bitstream_start();
        nal_start_code_prefix(NALU_TRAIL_R);
        nal_header(naluType);
        sliceHeader_rbsp(&ssh, &sps, &pps, 0);
        rbsp_trailing_bits();
        bitstream_end();
        *pp = (uint8_t *) buffer;
        buffer = nullptr;
        return bit_offset;
    }

public:
    HEVCBitStream(int width, int height)
        : frame_width(width)
        , frame_height(height) {
        bitstream_start();
    }
};
