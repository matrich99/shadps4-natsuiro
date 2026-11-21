// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ult.h"

#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/orbis_error.h"
#include "core/libraries/kernel/threads/pthread.h"
#include "core/libraries/libs.h"

#include <cstring>
#include <mutex>
#include <unordered_map>

namespace Libraries::Kernel {
    int PS4_SYSV_ABI posix_pthread_create_name_np(PthreadT* thread, const PthreadAttrT* attr,
                                                  PthreadEntryFunc start_routine, void* arg,
                                                  const char* name);
    int PS4_SYSV_ABI posix_pthread_join(PthreadT pthread, void** thread_return);
} // namespace Libraries::Kernel


namespace Libraries::Ult {

struct UlthreadWrapper {
    Libraries::Kernel::PthreadT pthread = nullptr;
    SceUltUlthreadEntry entry = nullptr;
    u64 arg = 0;
};

static std::mutex g_ult_mutex;
static std::unordered_map<SceUltUlthread*, UlthreadWrapper*> g_ulthreads;

static void* PS4_SYSV_ABI UlthreadTrampoline(void* raw_wrapper) {
    auto* wrapper = static_cast<UlthreadWrapper*>(raw_wrapper);
    const s32 ret = wrapper->entry(wrapper->arg);
    return reinterpret_cast<void*>(static_cast<intptr_t>(ret));
}

s32 PS4_SYSV_ABI sceUltInitialize() {
    LOG_INFO(Lib_Kernel, "sceUltInitialize called");
    return ORBIS_OK;
}

u64 PS4_SYSV_ABI sceUltUlthreadRuntimeGetWorkAreaSize(u32 max_num_ulthread,
                                                     u32 max_num_worker_thread) {
    LOG_INFO(Lib_Kernel, "sceUltUlthreadRuntimeGetWorkAreaSize(ulthreads={}, workers={})",
             max_num_ulthread, max_num_worker_thread);
    return static_cast<u64>(max_num_ulthread) * 0x1000 + 0x10000;
}

s32 PS4_SYSV_ABI _sceUltUlthreadRuntimeOptParamInitialize(SceUltUlthreadRuntimeOptParam* opt_param,
                                                           u32 build_version) {
    LOG_INFO(Lib_Kernel, "_sceUltUlthreadRuntimeOptParamInitialize(version=0x{:x})", build_version);
    if (opt_param) {
        std::memset(opt_param, 0, sizeof(SceUltUlthreadRuntimeOptParam));
        opt_param->size = sizeof(SceUltUlthreadRuntimeOptParam);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI _sceUltUlthreadRuntimeCreate(SceUltUlthreadRuntime* runtime, const char* name,
                                              u32 max_num_ulthread, u32 max_num_worker_thread,
                                              void* work_area,
                                              const SceUltUlthreadRuntimeOptParam* opt_param,
                                              u32 build_version) {
    LOG_INFO(Lib_Kernel, "_sceUltUlthreadRuntimeCreate(name={}, ulthreads={}, workers={})",
             name ? name : "null", max_num_ulthread, max_num_worker_thread);
    if (runtime) {
        std::memset(runtime, 0, sizeof(SceUltUlthreadRuntime));
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUltUlthreadRuntimeDestroy(SceUltUlthreadRuntime* runtime) {
    LOG_INFO(Lib_Kernel, "sceUltUlthreadRuntimeDestroy called");
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI _sceUltUlthreadOptParamInitialize(SceUltUlthreadOptParam* opt_param,
                                                   u32 build_version) {
    LOG_INFO(Lib_Kernel, "_sceUltUlthreadOptParamInitialize(version=0x{:x})", build_version);
    if (opt_param) {
        std::memset(opt_param, 0, sizeof(SceUltUlthreadOptParam));
        opt_param->size = sizeof(SceUltUlthreadOptParam);
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI _sceUltUlthreadCreate(SceUltUlthread* ulthread, const char* name,
                                       SceUltUlthreadEntry entry, u64 arg, void* context,
                                       u64 context_size, SceUltUlthreadRuntime* runtime,
                                       const SceUltUlthreadOptParam* opt_param, u32 build_version) {
    LOG_INFO(Lib_Kernel, "_sceUltUlthreadCreate(name={}, entry={:p}, arg=0x{:x})",
             name ? name : "null", reinterpret_cast<void*>(entry), arg);

    if (!ulthread || !entry) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }

    std::memset(ulthread, 0, sizeof(SceUltUlthread));

    auto* wrapper = new UlthreadWrapper{
        .pthread = nullptr,
        .entry = entry,
        .arg = arg,
    };

    const int ret = Libraries::Kernel::posix_pthread_create_name_np(
        &wrapper->pthread, nullptr, UlthreadTrampoline, wrapper, name);

    if (ret != 0) {
        delete wrapper;
        return ORBIS_KERNEL_ERROR_EAGAIN;
    }

    {
        std::scoped_lock lock(g_ult_mutex);
        g_ulthreads[ulthread] = wrapper;
    }

    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceUltUlthreadJoin(SceUltUlthread* ulthread, s32* status) {
    LOG_INFO(Lib_Kernel, "sceUltUlthreadJoin(ulthread={:p})", static_cast<void*>(ulthread));

    UlthreadWrapper* wrapper = nullptr;
    {
        std::scoped_lock lock(g_ult_mutex);
        auto it = g_ulthreads.find(ulthread);
        if (it != g_ulthreads.end()) {
            wrapper = it->second;
            g_ulthreads.erase(it);
        }
    }

    if (wrapper) {
        void* thread_ret = nullptr;
        Libraries::Kernel::posix_pthread_join(wrapper->pthread, &thread_ret);
        if (status) {
            *status = static_cast<s32>(reinterpret_cast<intptr_t>(thread_ret));
        }
        delete wrapper;
    } else if (status) {
        *status = 0;
    }

    return ORBIS_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("hZIg1EWGsHM", "libSceUlt", 1, "libSceUlt", sceUltInitialize);
    LIB_FUNCTION("grs2pbc2awM", "libSceUlt", 1, "libSceUlt",
                 sceUltUlthreadRuntimeGetWorkAreaSize);
    LIB_FUNCTION("V2u3WLrwh64", "libSceUlt", 1, "libSceUlt",
                 _sceUltUlthreadRuntimeOptParamInitialize);
    LIB_FUNCTION("jw9FkZBXo-g", "libSceUlt", 1, "libSceUlt", _sceUltUlthreadRuntimeCreate);
    LIB_FUNCTION("-gxcs521SvA", "libSceUlt", 1, "libSceUlt", sceUltUlthreadRuntimeDestroy);
    LIB_FUNCTION("vIuen2qlRmo", "libSceUlt", 1, "libSceUlt", _sceUltUlthreadOptParamInitialize);
    LIB_FUNCTION("znI3q8S7KQ4", "libSceUlt", 1, "libSceUlt", _sceUltUlthreadCreate);
    LIB_FUNCTION("gCeAI57LGgI", "libSceUlt", 1, "libSceUlt", sceUltUlthreadJoin);
}

} // namespace Libraries::Ult
