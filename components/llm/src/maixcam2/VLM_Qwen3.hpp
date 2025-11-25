#pragma once
#include <string>
#include <algorithm>
#include <cmath>
#include <numeric>
#include "bfloat16.hpp"
#include "Tokenizer/Tokenizer.hpp"
#include "LLMEmbedSelector.hpp"
#include "ax_model_runner/ax_model_runner_ax650.hpp"
#include "ax_cmm_utils.hpp"
#include "cqdm.h"
#include "timer.hpp"

#include <ax_sys_api.h>
#include "LLMPostprocess.hpp"
#include "maix_vlm_internvl.hpp"
#include <float.h>

namespace maix::nn::VLM_Qwen3
{
#ifdef ALIGN_DOWN
#undef ALIGN_DOWN
#endif
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

typedef void (*LLMRuningCallback)(int *p_token, int n_token, const char *p_str, float token_per_sec, void *reserve);

struct LLMAttrType
{
    std::string system_prompt;
    std::string template_filename_axmodel = "tinyllama-int8/tinyllama_l%d.axmodel";
    int axmodel_num = 22;

    // std::string template_prefill_filename_axmodel = "minicpmv/prefill_axmodel/minicpm_p96_l%d.axmodel";
    // int prefill_axmodel_num = 40;
    int prefill_token_num = 96; // auto calc
    int prefill_max_token_num = 512;

    std::string filename_post_axmodel = "tinyllama-int8/tinyllama_post.axmodel";

    std::string filename_vpm_encoder_axmodedl = "minicpmv/vpm_resampler_version0_fp16.axmodel";
    std::string filename_vpm_resampler_axmodedl = "minicpmv/vpm_resampler_version0_fp16.axmodel";
    int vpm_width = 280;
    int vpm_height = 280;
    bool b_vpm_two_stage = false;

    TokenizerType tokenizer_type = TKT_LLaMa;
    std::string url_tokenizer_model = "http://127.0.0.1:12345";
    std::string url_llm_service = "http://127.0.0.1:12346";
    std::string llm_service = "qwen3_vl.service";
    bool b_bos = true, b_eos = false;
    std::string filename_tokens_embed = "tinyllama.model.embed_tokens.weight.bfloat16.bin";
    int tokens_embed_num = 32000;
    int tokens_embed_size = 2048;

    int max_token_len = 127; // auto calc

    int kv_cache_num = 1024; // auto calc
    int kv_cache_size = 256; // auto calc

    int precompute_len = 1202;
    std::vector<int> prefill_max_kv_cache_num_grp;

    int prefill_grpid = -1;

    bool b_use_mmap_load_embed = false;

    int vpm_len;

    // bool b_use_mmap_load_layer = true;

    // bool b_live_print = true;
    LLMRuningCallback runing_callback = nullptr;
    void *reserve = nullptr;
};

}; // namespace VLM_Qwen3

