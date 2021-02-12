
#pragma once

#include "../script.h"

namespace Script { namespace mdpx {
    

    class KernelCodeGenerator
    {
    public:
        
        void AddKernel(Script::IKernelPtr kernel)
        {
            if (kernel && !kernel->IsEmpty()) {
                m_kernels[kernel->GetHash()]  = kernel;
            }
        }
        
        void GenerateCode(std::string path)
        {
            std::stringstream ss;
            
            ss << "// Auto-generated file\n";
            
            ss << "#include \"../src/script/script.h\"\n";
            ss << "#include \"../src/script/mdp-eel2/script-functions.h\"\n";
            
            ss << "namespace Script {\n";
            ss << "namespace mdpx {\n";
            ss << "namespace Generated {\n";
            
            
            ss << "using namespace Functions;\n";
            ss << "\n";
            
            
            std::map<uint64_t, std::string> registered;
            
            ///int i = 0;
            for (auto it : m_kernels)
            {
                ss << "//------------------\n";
                ss << "#if 0\n";
                ss << it.second->GetSource();
                ss << "#endif\n";
                
                uint64_t hash = it.first;
                std::string hash_str = std::to_string(hash);
                std::string name= "generated_kernel_" + hash_str;
                it.second->GenerateCPP(name, ss);
                registered[hash] = name;
            }
            
            ss << "//------------------\n";
            ss << "const KernelFuncEntry g_PrecompiledKernels[] = \n";
            
            ss << "{\n";
            for (auto it : registered)
            {
                ss << "\t" << "{" << it.first << "ULL, &" << it.second << "}, ";
                ss << "\n";
            }
            ss << "\t" << "{ 0, nullptr }";
            ss << "\n";
            
            ss << "};\n";
            
            ss << "\n";
            
            
            ss << "} // namespace Generated\n";
            ss << "} // namespace mdpx\n";
            ss << "} // namespace Script\n";
            
            
            ss << "// EOF\n\n";
            
            
            FileWriteAllText(path, ss.str());
        }
        
    protected:
        
        std::map<uint64_t, Script::IKernelPtr> m_kernels;
        
    };

    
}} // namespace

