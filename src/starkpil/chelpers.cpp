#include "chelpers.hpp"

void CHelpers::loadCHelpers(BinFileUtils::BinFile *cHelpersBin) {
    cHelpersBin->startReadSection(CHELPERS_STAGES_SECTION);

    uint64_t nStages = cHelpersBin->readU32LE();

    for(uint64_t i = 0; i < nStages; ++i) {
        ParserParams parserParamsStage;

        uint32_t stage = cHelpersBin->readU32LE();
        uint32_t executeBefore = cHelpersBin->readU32LE();

        parserParamsStage.stage = stage;
        parserParamsStage.executeBefore = executeBefore;
        
        std::string stageName = "step" + std::to_string(stage);
        if(executeBefore == 0) stageName += "_after";

        parserParamsStage.nTemp1 = cHelpersBin->readU32LE();
        parserParamsStage.nTemp3 = cHelpersBin->readU32LE();

        parserParamsStage.nOps = cHelpersBin->readU32LE();
        parserParamsStage.ops = new uint32_t[parserParamsStage.nOps];
        for(uint64_t j = 0; j < parserParamsStage.nOps; ++j) {
            parserParamsStage.ops[j] = cHelpersBin->readU32LE();
        }

        parserParamsStage.nArgs = cHelpersBin->readU32LE();
        parserParamsStage.args = new uint32_t[parserParamsStage.nArgs];
        for(uint64_t j = 0; j < parserParamsStage.nArgs; ++j) {
            parserParamsStage.args[j] = cHelpersBin->readU32LE();
        }

        parserParamsStage.nNumbers = cHelpersBin->readU32LE();
        parserParamsStage.numbers = new uint64_t[parserParamsStage.nNumbers];
        for(uint64_t j = 0; j < parserParamsStage.nNumbers; ++j) {
            parserParamsStage.numbers[j] = cHelpersBin->readU64LE();
        }

        stagesInfo[stageName] = parserParamsStage;
    }
    
    cHelpersBin->endReadSection();
};