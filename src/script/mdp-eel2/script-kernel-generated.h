

#pragma once

#include "platform.h"
#include "script-kernel.h"
#include "script-functype.h"
#include "script-functions.h"

namespace Script { namespace mdpx {
    
    namespace Generated {

        extern const KernelFuncEntry g_PrecompiledKernels[];
        const std::map<uint64_t, KernelFunc> &GetRegisteredPrecompilerKernels();
    }

}} // 
