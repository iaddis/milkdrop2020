
#include "script-kernel-generated.h"




#if 1

#include "../generated/generated-code.cpp"

#else

namespace Script { namespace mdpx {
    
    namespace Generated {
        const KernelFuncEntry g_PrecompiledKernels[] = {
            {0, nullptr}
        };
    }
}}
#endif



namespace Script { namespace mdpx {
namespace Generated {
    extern const KernelFuncEntry g_PrecompiledKernels[];
    const std::map<uint64_t, KernelFunc> &GetRegisteredPrecompilerKernels()
    {
        static std::map<uint64_t, KernelFunc> map;
        if (map.empty()) {
            for (int i=0; g_PrecompiledKernels[i].func_ptr; i++)
            {
                map[ g_PrecompiledKernels[i].hash ] = g_PrecompiledKernels[i].func_ptr;
            }
        }
        return map;
        
    }
}
}}



