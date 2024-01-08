#ifndef CHELPERS_STEPS_HPP
#define CHELPERS_STEPS_HPP
#include "chelpers.hpp"

class CHelpersSteps {
    public:
        virtual void calculateExpressions(StarkInfo &starkInfo, StepsParams &params, ParserParams &parserParams, bool useGeneric) {};
    private:
        virtual void parser_avx(StarkInfo &starkInfo, StepsParams &params, ParserParams &parserParams, uint32_t nrowsBatch, bool domainExtended) {};
};

#endif