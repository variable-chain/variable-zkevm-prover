#ifndef CHELPERS_HPP
#define CHELPERS_HPP

#include <string>
#include <map>
#include "binfile_utils.hpp"
#include "zhInv.hpp"
#include "polinomial.hpp"
#include "goldilocks_base_field.hpp"
#include "goldilocks_base_field_avx.hpp"
#include "goldilocks_base_field_operations.hpp"
#include "goldilocks_cubic_extension.hpp"
#include "goldilocks_cubic_extension_operations_avx.hpp"
#include "constant_pols_starks.hpp"
#include "steps.hpp"
#include "stark_info.hpp"
#include <immintrin.h>
#include <cassert>

const int CHELPERS_STAGES_SECTION = 2;
struct ParserParams 
{
    uint32_t stage;
    uint32_t executeBefore;
    uint32_t nTemp1;
    uint32_t nTemp3;
    uint32_t nOps;
    uint32_t *ops;
    uint32_t nArgs;
    uint32_t *args;
    uint32_t nNumbers;
    uint64_t *numbers;
};

class CHelpers
{
public:
    std::map<std::string, ParserParams> stagesInfo;
    
    ~CHelpers() {
        for (auto it = stagesInfo.begin(); it != stagesInfo.end(); ++it) {  
            delete[] it->second.ops;
            delete[] it->second.args;
            delete[] it->second.numbers;
        }

        stagesInfo.clear();
    };

    void loadCHelpers(BinFileUtils::BinFile *cHelpersBin);
};


#endif
