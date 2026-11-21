// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"

namespace Core::Loader {
class SymbolsResolver;
}

namespace Libraries::Ult {

using SceUltUlthreadEntry = s32(PS4_SYSV_ABI*)(u64 arg);

struct SceUltUlthreadOptParam {
    u64 size;
    u64 reserved[7];
};

struct SceUltUlthreadRuntimeOptParam {
    u64 size;
    u64 reserved[7];
};

struct SceUltUlthreadRuntime {
    u64 internal[32];
};

struct SceUltUlthread {
    u64 internal[32];
};

s32 PS4_SYSV_ABI sceUltInitialize();
u64 PS4_SYSV_ABI sceUltUlthreadRuntimeGetWorkAreaSize(u32 max_num_ulthread,
                                                     u32 max_num_worker_thread);
s32 PS4_SYSV_ABI _sceUltUlthreadRuntimeOptParamInitialize(SceUltUlthreadRuntimeOptParam* opt_param,
                                                           u32 build_version);
s32 PS4_SYSV_ABI _sceUltUlthreadRuntimeCreate(SceUltUlthreadRuntime* runtime, const char* name,
                                              u32 max_num_ulthread, u32 max_num_worker_thread,
                                              void* work_area,
                                              const SceUltUlthreadRuntimeOptParam* opt_param,
                                              u32 build_version);
s32 PS4_SYSV_ABI sceUltUlthreadRuntimeDestroy(SceUltUlthreadRuntime* runtime);

s32 PS4_SYSV_ABI _sceUltUlthreadOptParamInitialize(SceUltUlthreadOptParam* opt_param,
                                                   u32 build_version);
s32 PS4_SYSV_ABI _sceUltUlthreadCreate(SceUltUlthread* ulthread, const char* name,
                                       SceUltUlthreadEntry entry, u64 arg, void* context,
                                       u64 context_size, SceUltUlthreadRuntime* runtime,
                                       const SceUltUlthreadOptParam* opt_param, u32 build_version);
s32 PS4_SYSV_ABI sceUltUlthreadJoin(SceUltUlthread* ulthread, s32* status);

void RegisterLib(Core::Loader::SymbolsResolver* sym);

} // namespace Libraries::Ult
