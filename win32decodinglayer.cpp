#include <d3d12video.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <dxva.h>

#include <stdio.h>
#include <err.h>
#include <inttypes.h>
#include <stdlib.h>

#include "device.h"
//#include "hash.h"
#include "hevcbitstream.h"
#include "hevcparser.h"
#include "win32decodinglayer.h"

#define CHECK(_hr) \
    if (FAILED(_hr)) { \
        const char *msg = nullptr; \
        switch (_hr) { \
            case E_FAIL: \
                msg = "E_FAIL"; \
                break; \
            case E_OUTOFMEMORY: \
                msg = "E_OUTOFMEMORY"; \
                break; \
            case E_INVALIDARG: \
                msg = "E_INVALIDARG"; \
                break; \
            case DXGI_ERROR_DEVICE_REMOVED: \
                msg = "DXGI_ERROR_DEVICE_REMOVED"; \
                warnx("device removed %s line %d, hr=%x : %s\n", __FUNCTION__, __LINE__, (uint32_t) hr, msg); \
                break; \
            case DXGI_ERROR_INVALID_CALL: \
                msg = "DXGI_ERROR_INVALID_CALL"; \
                break; \
            default: \
                msg = "unknown error"; \
                break; \
        } \
        errx(1, "failed %s line %d, hr=%x : %s\n", __FUNCTION__, __LINE__, (uint32_t) hr, msg); \
    }



#define NAL_UNIT_H265_VPS 32
#define NAL_UNIT_H265_SPS 33
#define NAL_UNIT_H265_PPS 34

#define NAL_UNIT_H264_SEI 6
#define NAL_UNIT_H264_SPS 7
#define NAL_UNIT_H264_PPS 8

#if 0
#define IS_IDR(s) ((s)->nal_unit_type == HEVC_NAL_IDR_W_RADL || (s)->nal_unit_type == HEVC_NAL_IDR_N_LP)
#define IS_BLA(s) ((s)->nal_unit_type == HEVC_NAL_BLA_W_RADL || (s)->nal_unit_type == HEVC_NAL_BLA_W_LP || \
                   (s)->nal_unit_type == HEVC_NAL_BLA_N_LP)
#define IS_IRAP(s) ((s)->nal_unit_type >= HEVC_NAL_BLA_W_LP && (s)->nal_unit_type <= HEVC_NAL_RSV_IRAP_VCL23)
#endif

constexpr uint16_t DXVA_HEVC_INVALID_PICTURE_ENTRY_VALUE = 0xFF;

static void
d3d12_video_decoder_log_pic_entry_hevc(DXVA_PicEntry_HEVC &picEntry)
{
   printf("\t\tIndex7Bits: %d\n"
                 "\t\tAssociatedFlag: %d\n"
                 "\t\tbPicEntry: %d\n",
                 picEntry.Index7Bits,
                 picEntry.AssociatedFlag,
                 picEntry.bPicEntry);
}
void
d3d12_video_decoder_log_pic_params_hevc(DXVA_PicParams_HEVC *pPicParams)
{
   printf("\n=============================================\n");
#if 0
   printf("PicWidthInMinCbsY = %d\n", pPicParams->PicWidthInMinCbsY);
   printf("PicHeightInMinCbsY = %d\n", pPicParams->PicHeightInMinCbsY);
   printf("chroma_format_idc = %d\n", pPicParams->chroma_format_idc);
   printf("separate_colour_plane_flag = %d\n", pPicParams->separate_colour_plane_flag);
   printf("bit_depth_luma_minus8 = %d\n", pPicParams->bit_depth_luma_minus8);
   printf("bit_depth_chroma_minus8 = %d\n", pPicParams->bit_depth_chroma_minus8);
   printf("log2_max_pic_order_cnt_lsb_minus4 = %d\n", pPicParams->log2_max_pic_order_cnt_lsb_minus4);
   printf("NoPicReorderingFlag = %d\n", pPicParams->NoPicReorderingFlag);
   printf("NoBiPredFlag = %d\n", pPicParams->NoBiPredFlag);
   printf("ReservedBits1 = %d\n", pPicParams->ReservedBits1);
   printf("wFormatAndSequenceInfoFlags = %d\n", pPicParams->wFormatAndSequenceInfoFlags);
   printf("CurrPic.Index7Bits = %d\n", pPicParams->CurrPic.Index7Bits);
   printf("CurrPic.AssociatedFlag = %d\n", pPicParams->CurrPic.AssociatedFlag);
   printf("sps_max_dec_pic_buffering_minus1 = %d\n", pPicParams->sps_max_dec_pic_buffering_minus1);
   printf("log2_min_luma_coding_block_size_minus3 = %d\n", pPicParams->log2_min_luma_coding_block_size_minus3);
   printf("log2_diff_max_min_luma_coding_block_size = %d\n", pPicParams->log2_diff_max_min_luma_coding_block_size);
   printf("log2_min_transform_block_size_minus2 = %d\n", pPicParams->log2_min_transform_block_size_minus2);
   printf("log2_diff_max_min_transform_block_size = %d\n", pPicParams->log2_diff_max_min_transform_block_size);
   printf("max_transform_hierarchy_depth_inter = %d\n", pPicParams->max_transform_hierarchy_depth_inter);
   printf("max_transform_hierarchy_depth_intra = %d\n", pPicParams->max_transform_hierarchy_depth_intra);
   printf("num_short_term_ref_pic_sets = %d\n", pPicParams->num_short_term_ref_pic_sets);
   printf("num_long_term_ref_pics_sps = %d\n", pPicParams->num_long_term_ref_pics_sps);
   printf("num_ref_idx_l0_default_active_minus1 = %d\n", pPicParams->num_ref_idx_l0_default_active_minus1);
   printf("num_ref_idx_l1_default_active_minus1 = %d\n", pPicParams->num_ref_idx_l1_default_active_minus1);
   printf("init_qp_minus26 = %d\n", pPicParams->init_qp_minus26);
   printf("ucNumDeltaPocsOfRefRpsIdx = %d\n", pPicParams->ucNumDeltaPocsOfRefRpsIdx);
   printf("wNumBitsForShortTermRPSInSlice = %d\n", pPicParams->wNumBitsForShortTermRPSInSlice);
   printf("ReservedBits2 = %d\n", pPicParams->ReservedBits2);
   printf("scaling_list_enabled_flag = %d\n", pPicParams->scaling_list_enabled_flag);
   printf("amp_enabled_flag = %d\n", pPicParams->amp_enabled_flag);
   printf("sample_adaptive_offset_enabled_flag = %d\n", pPicParams->sample_adaptive_offset_enabled_flag);
   printf("pcm_enabled_flag = %d\n", pPicParams->pcm_enabled_flag);
   printf("pcm_sample_bit_depth_luma_minus1 = %d\n", pPicParams->pcm_sample_bit_depth_luma_minus1);
   printf("pcm_sample_bit_depth_chroma_minus1 = %d\n", pPicParams->pcm_sample_bit_depth_chroma_minus1);
   printf("log2_min_pcm_luma_coding_block_size_minus3 = %d\n", pPicParams->log2_min_pcm_luma_coding_block_size_minus3);
   printf("log2_diff_max_min_pcm_luma_coding_block_size = %d\n", pPicParams->log2_diff_max_min_pcm_luma_coding_block_size);
   printf("pcm_loop_filter_disabled_flag = %d\n", pPicParams->pcm_loop_filter_disabled_flag);
   printf("long_term_ref_pics_present_flag = %d\n", pPicParams->long_term_ref_pics_present_flag);
   printf("sps_temporal_mvp_enabled_flag = %d\n", pPicParams->sps_temporal_mvp_enabled_flag);
   printf("strong_intra_smoothing_enabled_flag = %d\n", pPicParams->strong_intra_smoothing_enabled_flag);
   printf("dependent_slice_segments_enabled_flag = %d\n", pPicParams->dependent_slice_segments_enabled_flag);
   printf("output_flag_present_flag = %d\n", pPicParams->output_flag_present_flag);
   printf("num_extra_slice_header_bits = %d\n", pPicParams->num_extra_slice_header_bits);
   printf("sign_data_hiding_enabled_flag = %d\n", pPicParams->sign_data_hiding_enabled_flag);
   printf("cabac_init_present_flag = %d\n", pPicParams->cabac_init_present_flag);
   printf("ReservedBits3 = %d\n", pPicParams->ReservedBits3);
   printf("dwCodingParamToolFlags = %d\n", pPicParams->dwCodingParamToolFlags);
   printf("constrained_intra_pred_flag = %d\n", pPicParams->constrained_intra_pred_flag);
   printf("transform_skip_enabled_flag = %d\n", pPicParams->transform_skip_enabled_flag);
   printf("cu_qp_delta_enabled_flag = %d\n", pPicParams->cu_qp_delta_enabled_flag);
   printf("pps_slice_chroma_qp_offsets_present_flag = %d\n", pPicParams->pps_slice_chroma_qp_offsets_present_flag);
   printf("weighted_pred_flag = %d\n", pPicParams->weighted_pred_flag);
   printf("weighted_bipred_flag = %d\n", pPicParams->weighted_bipred_flag);
   printf("transquant_bypass_enabled_flag = %d\n", pPicParams->transquant_bypass_enabled_flag);
   printf("tiles_enabled_flag = %d\n", pPicParams->tiles_enabled_flag);
   printf("entropy_coding_sync_enabled_flag = %d\n", pPicParams->entropy_coding_sync_enabled_flag);
   printf("uniform_spacing_flag = %d\n", pPicParams->uniform_spacing_flag);
   printf("loop_filter_across_tiles_enabled_flag = %d\n", pPicParams->loop_filter_across_tiles_enabled_flag);
   printf("pps_loop_filter_across_slices_enabled_flag = %d\n", pPicParams->pps_loop_filter_across_slices_enabled_flag);
   printf("deblocking_filter_override_enabled_flag = %d\n", pPicParams->deblocking_filter_override_enabled_flag);
   printf("pps_deblocking_filter_disabled_flag = %d\n", pPicParams->pps_deblocking_filter_disabled_flag);
   printf("lists_modification_present_flag = %d\n", pPicParams->lists_modification_present_flag);
   printf("slice_segment_header_extension_present_flag = %d\n", pPicParams->slice_segment_header_extension_present_flag);
   printf("IrapPicFlag = %d\n", pPicParams->IrapPicFlag);
   printf("IdrPicFlag = %d\n", pPicParams->IdrPicFlag);
   printf("IntraPicFlag = %d\n", pPicParams->IntraPicFlag);
   printf("ReservedBits4 = %d\n", pPicParams->ReservedBits4);
   printf("dwCodingSettingPicturePropertyFlags = %d\n", pPicParams->dwCodingSettingPicturePropertyFlags);
   printf("pps_cb_qp_offset = %d\n", pPicParams->pps_cb_qp_offset);
   printf("pps_cr_qp_offset = %d\n", pPicParams->pps_cr_qp_offset);
   printf("num_tile_columns_minus1 = %d\n", pPicParams->num_tile_columns_minus1);
   printf("num_tile_rows_minus1 = %d\n", pPicParams->num_tile_rows_minus1);
   for (uint32_t i = 0; i < std::min((unsigned) pPicParams->num_tile_columns_minus1 + 1u, (unsigned) _countof(DXVA_PicParams_HEVC::column_width_minus1)); i++) {
      printf("column_width_minus1[%d]; = %d\n", i, pPicParams->column_width_minus1[i]);
   }
   for (uint32_t i = 0; i < std::min((unsigned) pPicParams->num_tile_rows_minus1 + 1u, (unsigned) _countof(DXVA_PicParams_HEVC::row_height_minus1)); i++) {
      printf("row_height_minus1[%d]; = %d\n", i, pPicParams->row_height_minus1[i]);
   }
   printf("diff_cu_qp_delta_depth = %d\n", pPicParams->diff_cu_qp_delta_depth);
   printf("pps_beta_offset_div2 = %d\n", pPicParams->pps_beta_offset_div2);
   printf("pps_tc_offset_div2 = %d\n", pPicParams->pps_tc_offset_div2);
   printf("log2_parallel_merge_level_minus2 = %d\n", pPicParams->log2_parallel_merge_level_minus2);
   printf("CurrPicOrderCntVal = %d\n", pPicParams->CurrPicOrderCntVal);
   printf("ReservedBits5 = %d\n", pPicParams->ReservedBits5);
   printf("ReservedBits6 = %d\n", pPicParams->ReservedBits6);
   printf("ReservedBits7 = %d\n", pPicParams->ReservedBits7);
#endif
   printf("StatusReportFeedbackNumber = %d\n", pPicParams->StatusReportFeedbackNumber);

   printf("[D3D12 Video Decoder HEVC DXVA PicParams info]\n"
                 "\t[Current Picture Entry]\n");
   d3d12_video_decoder_log_pic_entry_hevc(pPicParams->CurrPic);

   printf("[D3D12 Video Decoder HEVC DXVA PicParams info]\n"
                 "\t[Current Picture Reference sets, hiding entries with bPicEntry 0xFF]\n");

   for (uint32_t refIdx = 0; refIdx < _countof(DXVA_PicParams_HEVC::RefPicSetStCurrBefore); refIdx++) {
      if(pPicParams->RefPicSetStCurrBefore[refIdx] != DXVA_HEVC_INVALID_PICTURE_ENTRY_VALUE) {
         printf("\tRefPicSetStCurrBefore[%d] = %d \n PicEntry RefPicList[%d]\n", refIdx, pPicParams->RefPicSetStCurrBefore[refIdx], pPicParams->RefPicSetStCurrBefore[refIdx]);
         d3d12_video_decoder_log_pic_entry_hevc(pPicParams->RefPicList[pPicParams->RefPicSetStCurrBefore[refIdx]]);
         printf("\t\tPicOrderCntValList: %d\n",
                     pPicParams->PicOrderCntValList[pPicParams->RefPicSetStCurrBefore[refIdx]]);
      }
   }
   for (uint32_t refIdx = 0; refIdx < _countof(DXVA_PicParams_HEVC::RefPicSetStCurrAfter); refIdx++) {
      if(pPicParams->RefPicSetStCurrAfter[refIdx] != DXVA_HEVC_INVALID_PICTURE_ENTRY_VALUE) {
         printf("\tRefPicSetStCurrAfter[%d] = %d \n PicEntry RefPicList[%d]\n", refIdx, pPicParams->RefPicSetStCurrAfter[refIdx], pPicParams->RefPicSetStCurrAfter[refIdx]);
         d3d12_video_decoder_log_pic_entry_hevc(pPicParams->RefPicList[pPicParams->RefPicSetStCurrAfter[refIdx]]);
         printf("\t\tPicOrderCntValList: %d\n",
                     pPicParams->PicOrderCntValList[pPicParams->RefPicSetStCurrAfter[refIdx]]);
      }
   }
   for (uint32_t refIdx = 0; refIdx < _countof(DXVA_PicParams_HEVC::RefPicSetLtCurr); refIdx++) {
      if(pPicParams->RefPicSetLtCurr[refIdx] != DXVA_HEVC_INVALID_PICTURE_ENTRY_VALUE) {
         printf("\tRefPicSetLtCurr[%d] = %d \n PicEntry RefPicList[%d]\n", refIdx, pPicParams->RefPicSetLtCurr[refIdx], pPicParams->RefPicSetLtCurr[refIdx]);
         d3d12_video_decoder_log_pic_entry_hevc(pPicParams->RefPicList[pPicParams->RefPicSetLtCurr[refIdx]]);
         printf("\t\tPicOrderCntValList: %d\n",
                     pPicParams->PicOrderCntValList[pPicParams->RefPicSetLtCurr[refIdx]]);
      }
   }
}

class Win32DecoderImpl {

    friend class Win32DecodingLayer;
    HEVCParser hevc_parser;

protected:
    Win32DecodingLayer *dl;
    ID3D12Device *device;
    ID3D12Device4 *device4;

    ID3D12CommandAllocator *direct_command_allocator;
    ID3D12CommandQueue *direct_command_queue;
    ID3D12GraphicsCommandList *direct_command_list;

    HANDLE fenceEvent;
    ID3D12Fence *fence;
    uint32_t direct_fencevalue = 0;

    ID3D12VideoProcessor1 *video_processor;
    ID3D12CommandAllocator *process_command_allocator;
    ID3D12VideoProcessCommandList1 *process_command_list;
    ID3D12CommandQueue *process_command_queue;
    uint32_t process_signal_value = 0;
    ID3D12Fence *process_fence;
    HANDLE process_fence_event;

    static const int num_reference_textures = 25;
    ID3D12Resource *reference_texture = nullptr;
    ID3D12Resource *nv12_texture = nullptr;

    HEVCBitStream *hevc_bitstream = nullptr;

    // d3d12 decoder state
    ID3D12VideoDevice3 *video_device = nullptr;
    int heap_width = 0;
    int heap_height = 0;
    ID3D12VideoDecoderHeap *decoder_heap = nullptr;
    ID3D12VideoDecoder *video_decoder = nullptr;

    ID3D12CommandAllocator *video_command_allocator = nullptr;
    ID3D12VideoDecodeCommandList2 *video_command_list = nullptr;
    ID3D12CommandQueue *video_command_queue = nullptr;

    HANDLE video_event;
    ID3D12Fence *video_fence = nullptr;
    UINT64 video_fencevalue = 0;

    D3D12_VIDEO_DECODE_CONFIGURATION decode_config = {
        D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN,
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    Win32DecoderImpl(Win32DecodingLayer *dl)
        : dl(dl) {

        HRESULT hr;
        extern ID3D12Device *GetD3DDevice();
        device = GetD3DDevice();

        UINT width = 1280;
        UINT height = 720;

        /************* video decoder *******************************/

        hr = device->QueryInterface(IID_PPV_ARGS(&video_device));
        CHECK(hr);

        hr = device->QueryInterface(IID_PPV_ARGS(&device4));
        CHECK(hr);

        D3D12_VIDEO_DECODER_DESC decoder_desc = { 0, decode_config };

        hr = video_device->CreateVideoDecoder(&decoder_desc, IID_PPV_ARGS(&video_decoder));
        CHECK(hr);

        D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT decode_support = {};
        decode_support.Configuration = decode_config;
        decode_support.Width = width;
        decode_support.Height = height;
        decode_support.DecodeFormat = DXGI_FORMAT_NV12;
        decode_support.FrameRate.Numerator = 30;
        decode_support.FrameRate.Denominator = 1;
        decode_support.BitRate = 0;
        hr = video_device->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &decode_support, sizeof(decode_support));
        CHECK(hr);

        if (decode_support.SupportFlags != D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED || decode_support.DecodeTier < D3D12_VIDEO_DECODE_TIER_1) {
            errx(1, "%s: not supported", __PRETTY_FUNCTION__);
        }

        auto cf = decode_support.ConfigurationFlags;
        printf("flags %x\n", cf);
        switch (cf) {
            case D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_NONE:
                break;
            case D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_HEIGHT_ALIGNMENT_MULTIPLE_32_REQUIRED:
                printf("32 mult\n");
                break;
            case D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_POST_PROCESSING_SUPPORTED:
                printf("post proc\n");
                break;
            case D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_REFERENCE_ONLY_ALLOCATIONS_REQUIRED:
                printf("reference only\n");
                break;
            case D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_ALLOW_RESOLUTION_CHANGE_ON_NON_KEY_FRAME:
                printf("non key\n");
                break;
        }

        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE, IID_PPV_ARGS(&video_command_allocator));
        CHECK(hr);

        hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE, video_command_allocator, nullptr, IID_PPV_ARGS(&video_command_list));
        CHECK(hr);

        hr = video_command_list->Close();
        CHECK(hr);

        D3D12_COMMAND_QUEUE_DESC video_queue_desc = { D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE };
        hr = device->CreateCommandQueue(&video_queue_desc, IID_PPV_ARGS(&video_command_queue));
        CHECK(hr);

        video_command_queue->SetName(L"video_command_queue");

        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&video_fence));
        CHECK(hr);
        video_fencevalue = 1;

        // Create an event handle to use for frame synchronization.
        video_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (video_event == nullptr) {
            hr = HRESULT_FROM_WIN32(GetLastError());
            CHECK(hr);
        }

        /********** video processor *********************************/

        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS, IID_PPV_ARGS(&process_command_allocator));
        CHECK(hr);

        hr = device4->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&process_command_list));
        CHECK(hr);

        D3D12_COMMAND_QUEUE_DESC process_queue_desc = { D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS, 0, D3D12_COMMAND_QUEUE_FLAG_NONE };
        hr = device->CreateCommandQueue(&process_queue_desc, IID_PPV_ARGS(&process_command_queue));
        CHECK(hr);

        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&process_fence));
        CHECK(hr);

        process_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (process_fence_event == NULL) {
            errx(1, "err %s on line %d\n", __FUNCTION__, __LINE__);
        }

        D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT dx12ProcCaps = {
            0, // NodeIndex
            {
                width,
                height,
                { DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 },
            },
            D3D12_VIDEO_FIELD_TYPE_NONE,
            D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
            { 30, 1 },
            { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
            D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
            { 30, 1 },
        };

        hr = video_device->CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_SUPPORT, &dx12ProcCaps, sizeof(dx12ProcCaps));
        if ((dx12ProcCaps.SupportFlags & D3D12_VIDEO_PROCESS_SUPPORT_FLAG_SUPPORTED) == 0) {
            errx(1, "VideoProc not supported for conversion DXGI_FORMAT_R8G8B8A8_UNORM to DXGI_FORMAT_NV12.");
        }

        DXGI_RATIONAL FrameRate = { 30, 1 };
        DXGI_RATIONAL AspectRatio = { 1, 1 };

        D3D12_VIDEO_SIZE_RANGE sr = dx12ProcCaps.ScaleSupport.OutputSizeRange;
        printf("size range %u %u %u %u\n", sr.MaxWidth, sr.MaxHeight, sr.MinWidth, sr.MinHeight);

        D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC inputStreamDesc = {
            DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
            AspectRatio,
            AspectRatio,
            FrameRate,
         dx12ProcCaps.ScaleSupport.OutputSizeRange,
         dx12ProcCaps.ScaleSupport.OutputSizeRange,
            //size_range, // SourceSizeRange
            //size_range, // DestinationSizeRange
            false, //enableOrientation,
            D3D12_VIDEO_PROCESS_FILTER_FLAG_NONE,
            D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
            D3D12_VIDEO_FIELD_TYPE_NONE,
            D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE,
            false, // EnableAlphaBlending
            {}, // LumaKey
            0, // NumPastFrames
            0, // NumFutureFrames
            false // EnableAutoProcessing
        };

        D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC outputStreamDesc = {
            DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
            D3D12_VIDEO_PROCESS_ALPHA_FILL_MODE_OPAQUE, // AlphaFillMode
            0u, // AlphaFillModeSourceStreamIndex
            { 0, 0, 0, 0 }, // BackgroundColor
            FrameRate, // FrameRate
            false // EnableStereo
        };

        hr = video_device->CreateVideoProcessor(0,
            &outputStreamDesc,
            1, &inputStreamDesc,
            IID_PPV_ARGS(&video_processor));
        CHECK(hr);


        /************* EOF video decoder *******************************/
        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&direct_command_allocator));
        CHECK(hr);

        hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, direct_command_allocator, nullptr, IID_PPV_ARGS(&direct_command_list));
        direct_command_list->SetName(L"direct_command_list");
        direct_command_list->Close();

        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        CHECK(hr);
        //fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (fenceEvent == nullptr) {
            hr = HRESULT_FROM_WIN32(GetLastError());
            CHECK(hr);
        }

        // Describe and create the command queue.
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&direct_command_queue));
        CHECK(hr);
    }

    void dump(const char *label, const uint8_t *bytes, size_t size) {
#if 0
        SHA1Hash h(bytes, size);
        char tmp[64];
        unsigned type = dl->is_hevc ? (bytes[0] >> 1) & 0x3f : bytes[0] & 0x1f;
        printf("RECV %s sz=%u: type=%d hash=%s\n", label, (uint32_t) size, type, h.AsText(tmp));

        size_t i;
        for (i = 0; i < size && i < 20; ++i) {
            printf("0x%02x, ", bytes[i]);
        }
        if (i < size) {
            printf(" (...)");
        }
        printf("\n");
#endif
    }

    inline void barrier(ID3D12Resource *resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(resource, from, to);
        direct_command_list->ResourceBarrier(1, &b);
    }

    inline void video_barrier(ID3D12Resource *resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(resource, from, to);
        video_command_list->ResourceBarrier(1, &b);
    }

    void direct_wait() {
        HRESULT hr;
        auto val = ++direct_fencevalue;
        hr = fence->SetEventOnCompletion(val, fenceEvent);
        CHECK(hr);
        hr = direct_command_queue->Signal(fence, val);
        CHECK(hr);
        if (WaitForSingleObject(fenceEvent, INFINITE) != WAIT_OBJECT_0) {
            errx(1, "WaitForSingleObject failed");
        }
    }

    void video_wait() {
        HRESULT hr;
        auto val = ++video_fencevalue;
        hr = video_fence->SetEventOnCompletion(val, video_event);
        CHECK(hr);
        hr = video_command_queue->Signal(video_fence, val);
        CHECK(hr);
        if (WaitForSingleObject(video_event, INFINITE) != WAIT_OBJECT_0) {
            errx(1, "WaitForSingleObject failed");
        }
    }

    void copy_to_host(void *dst, ID3D12Resource *resource, size_t size) {
        HRESULT hr;

        auto cl = direct_command_list;
        hr = direct_command_allocator->Reset();
        CHECK(hr);
        hr = cl->Reset(direct_command_allocator, NULL);
        CHECK(hr);

        ID3D12Resource *read_back_buffer;

        D3D12_RESOURCE_ALLOCATION_INFO info = {
            .SizeInBytes = size,
        };
        CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_READBACK);
        const D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(info);
        hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
                D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&read_back_buffer));
        CHECK(hr);

        barrier(resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
        cl->CopyResource(read_back_buffer, resource);
        barrier(resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
        hr = direct_command_list->Close();
        CHECK(hr);

        ID3D12CommandList *pcl[] = { direct_command_list };
        direct_command_queue->ExecuteCommandLists(1, pcl);
        direct_wait();

        void *rptr = nullptr;
        hr = read_back_buffer->Map(0, NULL, &rptr);
        CHECK(hr);
        memcpy(dst, rptr, size);
        read_back_buffer->Unmap(0, nullptr);
        read_back_buffer->Release();
    }

    void copy_texture(ID3D12Resource *dst, ID3D12Resource *src) {
        HRESULT hr;

        auto cl = direct_command_list;
        hr = direct_command_allocator->Reset();
        CHECK(hr);
        hr = cl->Reset(direct_command_allocator, NULL);
        CHECK(hr);

        barrier(src, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
        barrier(dst, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        cl->CopyResource(dst, src);
        barrier(src, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
        barrier(dst, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        hr = direct_command_list->Close();
        CHECK(hr);

        ID3D12CommandList *pcl[] = { direct_command_list };
        direct_command_queue->ExecuteCommandLists(1, pcl);
        direct_wait();
    }

    inline void process_barrier(ID3D12Resource *resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
        assert(resource);
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(resource, from, to);
        process_command_list->ResourceBarrier(1, &b);
    }

    void wait_process() {
        auto val = ++process_signal_value;

        if (process_fence->SetEventOnCompletion(val, process_fence_event) < 0) {
            errx(1, "err %s on line %d\n", __FUNCTION__, __LINE__);
        }

        if (process_command_queue->Signal(process_fence, val) < 0) {
            errx(1, "err %s on line %d\n", __FUNCTION__, __LINE__);
        }

        while (process_fence->GetCompletedValue() < val) {
            if (WaitForSingleObject(process_fence_event, INFINITE) != WAIT_OBJECT_0) {
                errx(1, "WaitForSingleObject failed");
            }
        }
    }

    void convert_nv12_to_rgba(ID3D12Resource *input, ID3D12Resource *output) {
        HRESULT hr;

        hr = process_command_allocator->Reset();
        CHECK(hr);
        hr = process_command_list->Reset(process_command_allocator);
        CHECK(hr);

        process_barrier(input, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ);
        process_barrier(output, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE);

        D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS1 input_args = {
            {
                {
                    input,
                },
                { },
            },
            {
                { 0, 0, dl->width, dl->height },
                { 0, 0, dl->width, dl->height },
                D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT,
            },
            D3D12_VIDEO_PROCESS_INPUT_STREAM_FLAG_NONE,
        };

        D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS output_args = {
            {
                {
                    output,
                },
            },
            { 0, 0, dl->width, dl->height }
        };

        process_command_list->ProcessFrames1(video_processor, &output_args, 1, &input_args);
        process_barrier(output, D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE, D3D12_RESOURCE_STATE_COMMON);
        process_barrier(input, D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ, D3D12_RESOURCE_STATE_COMMON);

        hr = process_command_list->Close();
        CHECK(hr);

        ID3D12CommandList *pcl[] = { process_command_list };
        process_command_queue->ExecuteCommandLists(1, pcl);
        wait_process();
    }

    void Decode(const uint8_t *bytes, size_t compressed_size) {

        HEVCBitStream::NALUType type = (HEVCBitStream::NALUType) ((bytes[0] >> 1) & 0x3f);
        bool is_irap = type >= HEVCBitStream::NALU_BLA_W_LP && type <= HEVCBitStream::NALU_RSV_IRAP_VCL23;
        bool is_idr = type == HEVCBitStream::NALU_IDR_W_DLP || type == HEVCBitStream::NALU_IDR_N_LP;
        bool is_key = is_irap || is_idr;
        printf("is_key %d\n", is_key);

        HRESULT hr;
        const size_t header_size = 3;

        //dump("payload", bytes, compressed_size);

        /* upload compressed data */
        CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_ALLOCATION_INFO alloc_info = {
            .SizeInBytes = header_size + compressed_size,
        };
        const D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(alloc_info);
        ID3D12Resource *resource;
        hr = device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&resource));
        CHECK(hr);

        void *ptr = nullptr;
        hr = resource->Map(0, NULL, &ptr);
        CHECK(hr);
        uint8_t *d = (uint8_t *) ptr;
        if (header_size) {
            /* fill in 0,0,1 NAL header */
            d[0] = 0;
            d[1] = 0;
            d[2] = 1;
        }
        memcpy(d + header_size, bytes, compressed_size);
        resource->Unmap(0, nullptr);

        /* copy from upload buffer to compressed resource */
        CD3DX12_HEAP_PROPERTIES heap_properties2(D3D12_HEAP_TYPE_DEFAULT);
        const D3D12_RESOURCE_DESC resource_desc2 = CD3DX12_RESOURCE_DESC::Buffer(alloc_info);
        ID3D12Resource *resource2;
        hr = device->CreateCommittedResource(
            &heap_properties2,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc2,
            D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&resource2));
        CHECK(hr);

        hr = direct_command_allocator->Reset();
        CHECK(hr);
        hr = direct_command_list->Reset(direct_command_allocator, nullptr);
        CHECK(hr);

        resource2->SetName(L"compressed data");
        barrier(resource2, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        direct_command_list->CopyResource(resource2, resource);
        barrier(resource2, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

        direct_command_list->Close();
        ID3D12CommandList *ppCommandLists[] = { direct_command_list };
        direct_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
        direct_wait();

        resource->Release();

        //printf("copied...\n");

        DXVA_PicParams_HEVC p = {};
        DXVA_Qmatrix_HEVC im = {};
        hevc_parser.FillDXVA(&p, &im);

        static uint32_t frame_counter = 0;
        p.StatusReportFeedbackNumber = ++frame_counter;

        DXVA_Slice_HEVC_Short slice = {
            0,
            (UINT) (header_size + compressed_size),
        };

        //d3d12_video_decoder_log_pic_params_hevc(&p);
        //printf("Index7Bits 0x%x\n", p.CurrPic.Index7Bits);

        int w, h;
        hevc_parser.GetDimensions(&w, &h);

        if (w != heap_width || h != heap_height) {
            /* change of image size detected, can no lounger user existing heap or nv12_texture */
            if (decoder_heap) {
                decoder_heap->Release();
                decoder_heap = nullptr;
            }
            if (nv12_texture) {
                nv12_texture->Release();
                nv12_texture = nullptr;
            }
            if (reference_texture) {
                reference_texture->Release();
            }
        }
        hevc_parser.GetUnpaddedDimensions(&dl->width, &dl->height);
        assert(dl->width);
        assert(dl->height);

        if (!decoder_heap) {
            D3D12_VIDEO_DECODER_HEAP_DESC heap_desc = {};
            heap_desc.NodeMask = 0;
            heap_desc.Configuration = decode_config;
            heap_desc.DecodeWidth = w;
            heap_desc.DecodeHeight = h;
            heap_desc.Format = DXGI_FORMAT_NV12;

            printf("create %dx%d decoder heap\n", w, h);
            hr = video_device->CreateVideoDecoderHeap(&heap_desc, IID_PPV_ARGS(&decoder_heap));
            CHECK(hr);

            heap_width = w;
            heap_height = h;

            D3D12_RESOURCE_DESC nv12_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_NV12, w, h, 1, 1);
            CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
            hr = device->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &nv12_resource_desc,
                D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&nv12_texture));
            CHECK(hr);
            nv12_texture->SetName(L"nv12_texture");

            D3D12_RESOURCE_DESC reference_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_NV12, w, h, num_reference_textures, 1);
            reference_resource_desc.Flags = D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

            hr = device->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &reference_resource_desc,
                D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&reference_texture));
            CHECK(hr);
            reference_texture->SetName(L"reference_texture");
        }


        D3D12_VIDEO_DECODE_INPUT_STREAM_ARGUMENTS input_arguments = {};
        assert(decoder_heap);
        input_arguments.pHeap = decoder_heap;
        input_arguments.NumFrameArguments = 3;

        input_arguments.FrameArguments[0].Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_PICTURE_PARAMETERS;
        input_arguments.FrameArguments[0].Size = sizeof(p);
        input_arguments.FrameArguments[0].pData = &p;

        input_arguments.FrameArguments[1].Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_INVERSE_QUANTIZATION_MATRIX;
        input_arguments.FrameArguments[1].Size = sizeof(im);
        input_arguments.FrameArguments[1].pData = &im;

        input_arguments.FrameArguments[2].Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_SLICE_CONTROL;
        input_arguments.FrameArguments[2].Size = sizeof(slice);
        input_arguments.FrameArguments[2].pData = &slice;

        input_arguments.CompressedBitstream.pBuffer = resource2;
        input_arguments.CompressedBitstream.Offset = 0;
        input_arguments.CompressedBitstream.Size = header_size + compressed_size;

        input_arguments.ReferenceFrames.NumTexture2Ds = num_reference_textures;

        /* AMD seems to only support TIER1 decoding, which means we have to pass the
         * references textures as a texture array, and provide a list of identical
         * resources here. */
        ID3D12Resource *references[num_reference_textures];
        for (int i = 0; i < num_reference_textures; ++i) {
            references[i] = reference_texture;
        }
        input_arguments.ReferenceFrames.ppTexture2Ds = references;

        /* Intel driver will choke on pSubresources being NULL, so pass pointer
         * to a zero-filled array. This is clearly a bug in Intel's driver. */
        UINT none[num_reference_textures] = {};
        input_arguments.ReferenceFrames.pSubresources = none;

        /* docs are unclear as to whether or not we need to pass in an array of heaps, so here we go */
        ID3D12VideoDecoderHeap *heaps[num_reference_textures];
        for (int i = 0; i < num_reference_textures; ++i) {
            heaps[i] = decoder_heap;
        }
        input_arguments.ReferenceFrames.ppHeaps = heaps;

        D3D12_VIDEO_DECODE_OUTPUT_STREAM_ARGUMENTS output_arguments = {};
        output_arguments.pOutputTexture2D = nv12_texture;
        output_arguments.ConversionArguments.pReferenceTexture2D = nv12_texture;

        hr = video_command_allocator->Reset();
        CHECK(hr);

        hr = video_command_list->Reset(video_command_allocator);
        CHECK(hr);

        video_barrier(resource2, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_DECODE_READ);

        video_barrier(reference_texture, D3D12_RESOURCE_STATE_COMMON, is_key ? D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE : D3D12_RESOURCE_STATE_VIDEO_DECODE_READ);

        video_barrier(nv12_texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE);
        printf("call DecodeFrame\n");
        video_command_list->DecodeFrame(video_decoder, &output_arguments, &input_arguments);
        video_barrier(resource2, D3D12_RESOURCE_STATE_VIDEO_DECODE_READ, D3D12_RESOURCE_STATE_COMMON);
        video_barrier(nv12_texture, D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE, D3D12_RESOURCE_STATE_COMMON);

        video_barrier(reference_texture, is_key ? D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE : D3D12_RESOURCE_STATE_VIDEO_DECODE_READ, D3D12_RESOURCE_STATE_COMMON);

        hr = video_command_list->Close();
        CHECK(hr);
        ID3D12CommandList *pcl2[] = { video_command_list };
        assert(video_command_queue);
        video_command_queue->ExecuteCommandLists(1, pcl2);
        video_wait();

        resource2->Release();

        printf("video frame decoded! exiting cleanly! (change code to decode more frames)\n");
        exit(0);
#if 0
        // XXX this is where we pass the decoding frame texture to the surrounding code,
        // disabled for AMD test

        auto buffer = new Buffer<uint8_t>;
        buffer->ReserveTexture(ColorSpace::RGBA, dl->width, dl->height);
        buffer->ToDevice(dl->device);
        auto gpu_buffer = (GPUBuffer *) buffer->GetDevicePointer();

        convert_nv12_to_rgba(nv12_texture, gpu_buffer->resource);

        dl->PutFrame(buffer);
#endif
    }

    static void Decode(const uint8_t *bytes, size_t compressed_size, void *opaque) {
        ((Win32DecoderImpl *) opaque)->Decode(bytes, compressed_size);
    }

    bool ReceiveBytes(const uint8_t *bytes, size_t compressed_size) {
        return hevc_parser.Parse(bytes, compressed_size, Decode, this);
    }

};

Win32DecodingLayer::Win32DecodingLayer(Device *device)
    : DecodingLayer(device) {
    impl = new Win32DecoderImpl(this);
}

Win32DecodingLayer::~Win32DecodingLayer() {
    delete impl;
}

bool Win32DecodingLayer::ReceiveBytes(const uint8_t *bytes,
    size_t compressed_size) {
    return impl->ReceiveBytes(bytes, compressed_size);
}
