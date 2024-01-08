#include "Recursive2Steps.hpp"

void Recursive2Steps::calculateExpressions(StarkInfo &starkInfo, StepsParams &params, ParserParams &parserParams, bool useGeneric) {
    uint32_t nrowsBatch = 4;
        bool domainExtended = parserParams.stage > 3 ? true : false;
        Recursive2Steps::parser_avx(starkInfo, params, parserParams, nrowsBatch, domainExtended);
}