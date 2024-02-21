#include "ZkevmSteps.hpp"

void ZkevmSteps::parser_avx(StarkInfo &starkInfo, StepsParams &params, ParserArgs &parserArgs, ParserParams &parserParams, uint32_t nrowsBatch, bool domainExtended) {
    uint64_t domainSize = domainExtended ? 1 << starkInfo.starkStruct.nBitsExt : 1 << starkInfo.starkStruct.nBits;
    Polinomial &x = domainExtended ? params.x_2ns : params.x_n;
    ConstantPolsStarks *constPols = domainExtended ? params.pConstPols2ns : params.pConstPols;
    Goldilocks3::Element_avx challenges[params.challenges.degree()];
    Goldilocks3::Element_avx challenges_ops[params.challenges.degree()];

    uint8_t *ops = &parserArgs.ops[parserParams.opsOffset];

    uint16_t *args = &parserArgs.args[parserParams.argsOffset]; 

    uint64_t* numbers = &parserArgs.numbers[parserParams.numbersOffset];

    __m256i numbers_[parserParams.nNumbers];

    uint64_t nStages = 3;
    uint64_t nextStride = domainExtended ? 1 << (starkInfo.starkStruct.nBitsExt - starkInfo.starkStruct.nBits) : 1;
    uint64_t nrowsBuff = nrowsBatch + nextStride;

    uint64_t nCols = starkInfo.nConstants;
    uint64_t buffTOffsetsSteps_[nStages + 2];
    uint64_t nColsSteps[nStages + 2];
    uint64_t offsetsSteps[nStages + 2];

    nColsSteps[0] = starkInfo.nConstants;
    buffTOffsetsSteps_[0] = 0;
    offsetsSteps[1] = domainExtended ? starkInfo.mapOffsets.section[eSection::cm1_2ns] : starkInfo.mapOffsets.section[eSection::cm1_n];
    nColsSteps[1] = starkInfo.mapSectionsN.section[eSection::cm1_2ns];
    buffTOffsetsSteps_[1] = 2*nColsSteps[0];
    nCols += nColsSteps[1];

    offsetsSteps[2] = domainExtended ? starkInfo.mapOffsets.section[eSection::cm2_2ns] : starkInfo.mapOffsets.section[eSection::cm2_n];
    nColsSteps[2] = starkInfo.mapSectionsN.section[eSection::cm2_2ns];
    buffTOffsetsSteps_[2] = buffTOffsetsSteps_[1] + 2*nColsSteps[1];
    nCols += nColsSteps[2];

    offsetsSteps[3] = domainExtended ? starkInfo.mapOffsets.section[eSection::cm3_2ns] : starkInfo.mapOffsets.section[eSection::cm3_n];
    nColsSteps[3] = starkInfo.mapSectionsN.section[eSection::cm3_2ns];
    buffTOffsetsSteps_[3] = buffTOffsetsSteps_[2] + 2*nColsSteps[2];
    nCols += nColsSteps[3];

    if(parserParams.stage <= nStages) {
        offsetsSteps[4] = starkInfo.mapOffsets.section[eSection::tmpExp_n];
        nColsSteps[4] = starkInfo.mapSectionsN.section[eSection::tmpExp_n];
    } else {
        offsetsSteps[4] = starkInfo.mapOffsets.section[eSection::cm4_2ns];
        nColsSteps[4] = starkInfo.mapSectionsN.section[eSection::cm4_2ns];
    }
    buffTOffsetsSteps_[4] = buffTOffsetsSteps_[3] + 2*nColsSteps[3];
    nCols += nColsSteps[4];

#pragma omp parallel for
    for(uint64_t i = 0; i < params.challenges.degree(); ++i) {
        challenges[i][0] = _mm256_set1_epi64x(params.challenges[i][0].fe);
        challenges[i][1] = _mm256_set1_epi64x(params.challenges[i][1].fe);
        challenges[i][2] = _mm256_set1_epi64x(params.challenges[i][2].fe);

        Goldilocks::Element challenges_aux[3];
        challenges_aux[0] = params.challenges[i][0] + params.challenges[i][1];
        challenges_aux[1] = params.challenges[i][0] + params.challenges[i][2];
        challenges_aux[2] = params.challenges[i][1] + params.challenges[i][2];
        challenges_ops[i][0] = _mm256_set1_epi64x(challenges_aux[0].fe);
        challenges_ops[i][1] =  _mm256_set1_epi64x(challenges_aux[1].fe);
        challenges_ops[i][2] =  _mm256_set1_epi64x(challenges_aux[2].fe);
    }
#pragma omp parallel for
    for(uint64_t i = 0; i < parserParams.nNumbers; ++i) {
        numbers_[i] = _mm256_set1_epi64x(numbers[i]);
    }
    __m256i publics[starkInfo.nPublics];
#pragma omp parallel for
    for(uint64_t i = 0; i < starkInfo.nPublics; ++i) {
        publics[i] = _mm256_set1_epi64x(params.publicInputs[i].fe);
    }
    Goldilocks3::Element_avx evals[params.evals.degree()];
#pragma omp parallel for
    for(uint64_t i = 0; i < params.evals.degree(); ++i) {
        evals[i][0] = _mm256_set1_epi64x(params.evals[i][0].fe);
        evals[i][1] = _mm256_set1_epi64x(params.evals[i][1].fe);
        evals[i][2] = _mm256_set1_epi64x(params.evals[i][2].fe);
    }
#pragma omp parallel for
    for (uint64_t i = 0; i < domainSize; i+= nrowsBatch) {
        bool const needModule = i + nrowsBatch + nextStride >= domainSize;
        uint64_t i_args = 0;

        uint64_t offsetsDest[4];
        __m256i tmp1[parserParams.nTemp1];
        Goldilocks3::Element_avx tmp3[parserParams.nTemp3];
        Goldilocks3::Element_avx tmp3_;
        // Goldilocks3::Element_avx tmp3_0;
        Goldilocks3::Element_avx tmp3_1;
        // __m256i tmp1_0;
        __m256i tmp1_1;
        __m256i bufferT_[2*nCols];

        uint64_t kk = 0;
        Goldilocks::Element bufferT[nrowsBuff];

        for(uint64_t k = 0; k < nColsSteps[0]; ++k) {
            for(uint64_t j = 0; j < nrowsBuff; ++j) {
                uint64_t l = i + j;
                if(l >= domainSize) l -= domainSize;
                bufferT[j] = ((Goldilocks::Element *)constPols->address())[l * nColsSteps[0] + k];
            }
            Goldilocks::load_avx(bufferT_[kk], &bufferT[0]);
            kk += 1;
            Goldilocks::load_avx(bufferT_[kk], &bufferT[nextStride]);
            kk += 1;
        }
        for(uint64_t k = 0; k < nColsSteps[1]; ++k) {
            for(uint64_t j = 0; j < nrowsBuff; ++j) {
                uint64_t l = i + j;
                if(l >= domainSize) l -= domainSize;
                bufferT[j] = params.pols[offsetsSteps[1] + l * nColsSteps[1] + k];
            }
            Goldilocks::load_avx(bufferT_[kk], &bufferT[0]);
            kk += 1;
            Goldilocks::load_avx(bufferT_[kk], &bufferT[nextStride]);
            kk += 1;
        }
        for(uint64_t s = 2; s <= nStages; ++s) {
            if(parserParams.stage < s) break;
            for(uint64_t k = 0; k < nColsSteps[s]; ++k) {
                for(uint64_t j = 0; j < nrowsBuff; ++j) {
                    uint64_t l = i + j;
                    if(l >= domainSize) l -= domainSize;
                    bufferT[j] = params.pols[offsetsSteps[s] + l * nColsSteps[s] + k];
                }
                Goldilocks::load_avx(bufferT_[kk], &bufferT[0]);
                kk += 1;
                Goldilocks::load_avx(bufferT_[kk], &bufferT[nextStride]);
                kk += 1;
            }
        }
        for(uint64_t k = 0; k < nColsSteps[nStages + 1]; ++k) {
            for(uint64_t j = 0; j < nrowsBuff; ++j) {
                uint64_t l = i + j;
                if(l >= domainSize) l -= domainSize;
                bufferT[j] = params.pols[offsetsSteps[nStages + 1] + l * nColsSteps[nStages + 1] + k];
            }
            Goldilocks::load_avx(bufferT_[kk], &bufferT[0]);
            kk += 1;
            Goldilocks::load_avx(bufferT_[kk], &bufferT[nextStride]);
            kk += 1;
        }


        for (uint64_t kk = 0; kk < parserParams.nOps; ++kk) {
            switch (ops[kk]) {
            case 0: {
                    // OPERATION WITH DEST: commit1 - SRC0: commit1 - SRC1: commit1
                    Goldilocks::op_avx(args[i_args], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], bufferT_[buffTOffsetsSteps_[args[i_args + 4]] + 2 * args[i_args + 5] + args[i_args + 6]], bufferT_[buffTOffsetsSteps_[args[i_args + 7]] + 2 * args[i_args + 8] + args[i_args + 9]]);
                    if(needModule) {
                        uint64_t stepOffset = offsetsSteps[args[i_args + 1]] + args[i_args + 2];
                        uint64_t nextStrideOffset = i + nextStride * args[i_args + 3];
                        offsetsDest[0] = stepOffset + (nextStrideOffset % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[1] = stepOffset + ((nextStrideOffset + 1) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[2] = stepOffset + ((nextStrideOffset + 2) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[3] = stepOffset + ((nextStrideOffset + 3) % domainSize) * nColsSteps[args[i_args + 1]];
                        Goldilocks::store_avx(&params.pols[0], offsetsDest, bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    } else {
                        Goldilocks::store_avx(&params.pols[offsetsSteps[args[i_args + 1]] + args[i_args + 2] + (i + nextStride * args[i_args + 3]) * nColsSteps[args[i_args + 1]]], nColsSteps[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    }
                    i_args += 10;
                    break;
            }
            case 1: {
                    // OPERATION WITH DEST: commit1 - SRC0: commit1 - SRC1: tmp1
                    Goldilocks::op_avx(args[i_args], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], bufferT_[buffTOffsetsSteps_[args[i_args + 4]] + 2 * args[i_args + 5] + args[i_args + 6]], tmp1[args[i_args + 7]]);
                    if(needModule) {
                        uint64_t stepOffset = offsetsSteps[args[i_args + 1]] + args[i_args + 2];
                        uint64_t nextStrideOffset = i + nextStride * args[i_args + 3];
                        offsetsDest[0] = stepOffset + (nextStrideOffset % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[1] = stepOffset + ((nextStrideOffset + 1) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[2] = stepOffset + ((nextStrideOffset + 2) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[3] = stepOffset + ((nextStrideOffset + 3) % domainSize) * nColsSteps[args[i_args + 1]];
                        Goldilocks::store_avx(&params.pols[0], offsetsDest, bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    } else {
                        Goldilocks::store_avx(&params.pols[offsetsSteps[args[i_args + 1]] + args[i_args + 2] + (i + nextStride * args[i_args + 3]) * nColsSteps[args[i_args + 1]]], nColsSteps[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    }
                    i_args += 8;
                    break;
            }
            case 2: {
                    // COPY tmp1 to commit1
                    Goldilocks::copy_avx(bufferT_[buffTOffsetsSteps_[args[i_args]] + 2 * args[i_args + 1] + args[i_args + 2]], tmp1[args[i_args + 3]]);
                    if(needModule) {
                        uint64_t stepOffset = offsetsSteps[args[i_args]] + args[i_args + 1];
                        uint64_t nextStrideOffset = i + nextStride * args[i_args + 2];
                        offsetsDest[0] = stepOffset + (nextStrideOffset % domainSize) * nColsSteps[args[i_args]];
                        offsetsDest[1] = stepOffset + ((nextStrideOffset + 1) % domainSize) * nColsSteps[args[i_args]];
                        offsetsDest[2] = stepOffset + ((nextStrideOffset + 2) % domainSize) * nColsSteps[args[i_args]];
                        offsetsDest[3] = stepOffset + ((nextStrideOffset + 3) % domainSize) * nColsSteps[args[i_args]];
                        Goldilocks::store_avx(&params.pols[0], offsetsDest, bufferT_[buffTOffsetsSteps_[args[i_args]] + 2 * args[i_args + 1] + args[i_args + 2]]);
                    } else {
                        Goldilocks::store_avx(&params.pols[offsetsSteps[args[i_args]] + args[i_args + 1] + (i + nextStride * args[i_args + 2]) * nColsSteps[args[i_args]]], nColsSteps[args[i_args]], bufferT_[buffTOffsetsSteps_[args[i_args]] + 2 * args[i_args + 1] + args[i_args + 2]]);
                    }
                    i_args += 4;
                    break;
            }
            case 3: {
                    // OPERATION WITH DEST: commit1 - SRC0: tmp1 - SRC1: tmp1
                    Goldilocks::op_avx(args[i_args], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], tmp1[args[i_args + 4]], tmp1[args[i_args + 5]]);
                    if(needModule) {
                        uint64_t stepOffset = offsetsSteps[args[i_args + 1]] + args[i_args + 2];
                        uint64_t nextStrideOffset = i + nextStride * args[i_args + 3];
                        offsetsDest[0] = stepOffset + (nextStrideOffset % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[1] = stepOffset + ((nextStrideOffset + 1) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[2] = stepOffset + ((nextStrideOffset + 2) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[3] = stepOffset + ((nextStrideOffset + 3) % domainSize) * nColsSteps[args[i_args + 1]];
                        Goldilocks::store_avx(&params.pols[0], offsetsDest, bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    } else {
                        Goldilocks::store_avx(&params.pols[offsetsSteps[args[i_args + 1]] + args[i_args + 2] + (i + nextStride * args[i_args + 3]) * nColsSteps[args[i_args + 1]]], nColsSteps[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    }
                    i_args += 6;
                    break;
            }
            case 4: {
                    // OPERATION WITH DEST: commit1 - SRC0: tmp1 - SRC1: number
                    Goldilocks::op_avx(args[i_args], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], tmp1[args[i_args + 4]], numbers_[args[i_args + 5]]);
                    if(needModule) {
                        uint64_t stepOffset = offsetsSteps[args[i_args + 1]] + args[i_args + 2];
                        uint64_t nextStrideOffset = i + nextStride * args[i_args + 3];
                        offsetsDest[0] = stepOffset + (nextStrideOffset % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[1] = stepOffset + ((nextStrideOffset + 1) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[2] = stepOffset + ((nextStrideOffset + 2) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[3] = stepOffset + ((nextStrideOffset + 3) % domainSize) * nColsSteps[args[i_args + 1]];
                        Goldilocks::store_avx(&params.pols[0], offsetsDest, bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    } else {
                        Goldilocks::store_avx(&params.pols[offsetsSteps[args[i_args + 1]] + args[i_args + 2] + (i + nextStride * args[i_args + 3]) * nColsSteps[args[i_args + 1]]], nColsSteps[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    }
                    i_args += 6;
                    break;
            }
            case 5: {
                    // COPY commit1 to tmp1
                    Goldilocks::copy_avx(tmp1[args[i_args]], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 6: {
                    // OPERATION WITH DEST: tmp1 - SRC0: commit1 - SRC1: commit1
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], bufferT_[buffTOffsetsSteps_[args[i_args + 5]] + 2 * args[i_args + 6] + args[i_args + 7]]);
                    i_args += 8;
                    break;
            }
            case 7: {
                    // OPERATION WITH DEST: tmp1 - SRC0: commit1 - SRC1: tmp1
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 8: {
                    // OPERATION WITH DEST: tmp1 - SRC0: commit1 - SRC1: public
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], publics[args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 9: {
                    // OPERATION WITH DEST: tmp1 - SRC0: commit1 - SRC1: number
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 10: {
                    // COPY tmp1 to tmp1
                    Goldilocks::copy_avx(tmp1[args[i_args]], tmp1[args[i_args + 1]]);
                    i_args += 2;
                    break;
            }
            case 11: {
                    // OPERATION WITH DEST: tmp1 - SRC0: tmp1 - SRC1: tmp1
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 12: {
                    // OPERATION WITH DEST: tmp1 - SRC0: tmp1 - SRC1: number
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], numbers_[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 13: {
                    // COPY number to tmp1
                    Goldilocks::copy_avx(tmp1[args[i_args]], numbers_[args[i_args + 1]]);
                    i_args += 2;
                    break;
            }
            case 14: {
                    // OPERATION WITH DEST: commit3 - SRC0: tmp3 - SRC1: tmp1
                    Goldilocks3::op_31_avx(args[i_args], &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2, 
                        &(tmp3[args[i_args + 4]][0]), 1, 
                        tmp1[args[i_args + 5]]);
                    if(needModule) {
                        uint64_t stepOffset = offsetsSteps[args[i_args + 1]] + args[i_args + 2];
                        uint64_t nextStrideOffset = i + nextStride * args[i_args + 3];
                        offsetsDest[0] = stepOffset + (nextStrideOffset % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[1] = stepOffset + ((nextStrideOffset + 1) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[2] = stepOffset + ((nextStrideOffset + 2) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[3] = stepOffset + ((nextStrideOffset + 3) % domainSize) * nColsSteps[args[i_args + 1]];
                        Goldilocks3::store_avx(&params.pols[0], offsetsDest, &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2);
                    } else {
                        Goldilocks3::store_avx(&params.pols[offsetsSteps[args[i_args + 1]] + args[i_args + 2] + (i + nextStride * args[i_args + 3]) * nColsSteps[args[i_args + 1]]], nColsSteps[args[i_args + 1]], &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2);
                    }
                    i_args += 6;
                    break;
            }
            case 15: {
                    // OPERATION WITH DEST: commit3 - SRC0: commit3 - SRC1: tmp3
                    Goldilocks3::op_avx(args[i_args], &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 4]] + 2 * args[i_args + 5] + args[i_args + 6]], 2, 
                        &(tmp3[args[i_args + 7]][0]), 1);
                    if(needModule) {
                        uint64_t stepOffset = offsetsSteps[args[i_args + 1]] + args[i_args + 2];
                        uint64_t nextStrideOffset = i + nextStride * args[i_args + 3];
                        offsetsDest[0] = stepOffset + (nextStrideOffset % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[1] = stepOffset + ((nextStrideOffset + 1) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[2] = stepOffset + ((nextStrideOffset + 2) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[3] = stepOffset + ((nextStrideOffset + 3) % domainSize) * nColsSteps[args[i_args + 1]];
                        Goldilocks3::store_avx(&params.pols[0], offsetsDest, &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2);
                    } else {
                        Goldilocks3::store_avx(&params.pols[offsetsSteps[args[i_args + 1]] + args[i_args + 2] + (i + nextStride * args[i_args + 3]) * nColsSteps[args[i_args + 1]]], nColsSteps[args[i_args + 1]], &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2);
                    }
                    i_args += 8;
                    break;
            }
            case 16: {
                    // OPERATION WITH DEST: commit3 - SRC0: tmp3 - SRC1: tmp3
                    Goldilocks3::op_avx(args[i_args], &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2, 
                        &(tmp3[args[i_args + 4]][0]), 1, 
                        &(tmp3[args[i_args + 5]][0]), 1);
                    if(needModule) {
                        uint64_t stepOffset = offsetsSteps[args[i_args + 1]] + args[i_args + 2];
                        uint64_t nextStrideOffset = i + nextStride * args[i_args + 3];
                        offsetsDest[0] = stepOffset + (nextStrideOffset % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[1] = stepOffset + ((nextStrideOffset + 1) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[2] = stepOffset + ((nextStrideOffset + 2) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[3] = stepOffset + ((nextStrideOffset + 3) % domainSize) * nColsSteps[args[i_args + 1]];
                        Goldilocks3::store_avx(&params.pols[0], offsetsDest, &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2);
                    } else {
                        Goldilocks3::store_avx(&params.pols[offsetsSteps[args[i_args + 1]] + args[i_args + 2] + (i + nextStride * args[i_args + 3]) * nColsSteps[args[i_args + 1]]], nColsSteps[args[i_args + 1]], &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2);
                    }
                    i_args += 6;
                    break;
            }
            case 17: {
                    // OPERATION WITH DEST: commit3 - SRC0: tmp3 - SRC1: challenge
                    Goldilocks3::op_avx(args[i_args], &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2, 
                        &(tmp3[args[i_args + 4]][0]), 1, 
                        &(challenges[args[i_args + 5]][0]), 1);
                    if(needModule) {
                        uint64_t stepOffset = offsetsSteps[args[i_args + 1]] + args[i_args + 2];
                        uint64_t nextStrideOffset = i + nextStride * args[i_args + 3];
                        offsetsDest[0] = stepOffset + (nextStrideOffset % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[1] = stepOffset + ((nextStrideOffset + 1) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[2] = stepOffset + ((nextStrideOffset + 2) % domainSize) * nColsSteps[args[i_args + 1]];
                        offsetsDest[3] = stepOffset + ((nextStrideOffset + 3) % domainSize) * nColsSteps[args[i_args + 1]];
                        Goldilocks3::store_avx(&params.pols[0], offsetsDest, &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2);
                    } else {
                        Goldilocks3::store_avx(&params.pols[offsetsSteps[args[i_args + 1]] + args[i_args + 2] + (i + nextStride * args[i_args + 3]) * nColsSteps[args[i_args + 1]]], nColsSteps[args[i_args + 1]], &bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]], 2);
                    }
                    i_args += 6;
                    break;
            }
            case 18: {
                    // OPERATION WITH DEST: tmp3 - SRC0: commit3 - SRC1: number
                    Goldilocks3::op_31_avx(args[i_args], &(tmp3[args[i_args + 1]][0]), 1, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], 2, 
                        numbers_[args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 19: {
                    // OPERATION WITH DEST: tmp3 - SRC0: tmp3 - SRC1: commit1
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 20: {
                    // OPERATION WITH DEST: tmp3 - SRC0: tmp3 - SRC1: tmp1
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 21: {
                    // OPERATION WITH DEST: tmp3 - SRC0: tmp3 - SRC1: x
                    Goldilocks::load_avx(tmp1_1, x[i], x.offset());
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1_1);
                    i_args += 3;
                    break;
            }
            case 22: {
                    // OPERATION WITH DEST: tmp3 - SRC0: challenge - SRC1: commit1
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], challenges[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 23: {
                    // OPERATION WITH DEST: tmp3 - SRC0: challenge - SRC1: tmp1
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], challenges[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 24: {
                    // OPERATION WITH DEST: tmp3 - SRC0: challenge - SRC1: x
                    Goldilocks::load_avx(tmp1_1, x[i], x.offset());
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], challenges[args[i_args + 2]], tmp1_1);
                    i_args += 3;
                    break;
            }
            case 25: {
                    // OPERATION WITH DEST: tmp3 - SRC0: challenge - SRC1: number
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], challenges[args[i_args + 2]], numbers_[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 26: {
                    // OPERATION WITH DEST: tmp3 - SRC0: commit3 - SRC1: commit3
                    Goldilocks3::op_avx(args[i_args], &(tmp3[args[i_args + 1]][0]), 1, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], 2, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 5]] + 2 * args[i_args + 6] + args[i_args + 7]], 2);
                    i_args += 8;
                    break;
            }
            case 27: {
                    // OPERATION WITH DEST: tmp3 - SRC0: commit3 - SRC1: tmp3
                    Goldilocks3::op_avx(args[i_args], &(tmp3[args[i_args + 1]][0]), 1, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], 2, 
                        &(tmp3[args[i_args + 5]][0]), 1);
                    i_args += 6;
                    break;
            }
            case 28: {
                    // MULTIPLICATION WITH DEST: tmp3 - SRC0: commit3 - SRC1: challenge
                    Goldilocks3::mul_avx(&(tmp3[args[i_args + 1]][0]), 1, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], 2, 
                        challenges[args[i_args + 5]], challenges_ops[args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 29: {
                    // OPERATION WITH DEST: tmp3 - SRC0: commit3 - SRC1: challenge
                    Goldilocks3::op_avx(args[i_args], &(tmp3[args[i_args + 1]][0]), 1, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], 2, 
                        &(challenges[args[i_args + 5]][0]), 1);
                    i_args += 6;
                    break;
            }
            case 30: {
                    // OPERATION WITH DEST: tmp3 - SRC0: tmp3 - SRC1: tmp3
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp3[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 31: {
                    // MULTIPLICATION WITH DEST: tmp3 - SRC0: tmp3 - SRC1: challenge
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 32: {
                    // OPERATION WITH DEST: tmp3 - SRC0: tmp3 - SRC1: challenge
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 33: {
                    // OPERATION WITH DEST: tmp3 - SRC0: eval - SRC1: commit1
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], evals[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 34: {
                    // OPERATION WITH DEST: tmp3 - SRC0: commit3 - SRC1: eval
                    Goldilocks3::op_avx(args[i_args], &(tmp3[args[i_args + 1]][0]), 1, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], 2, 
                        &(evals[args[i_args + 5]][0]), 1);
                    i_args += 6;
                    break;
            }
            case 35: {
                    // OPERATION WITH DEST: tmp3 - SRC0: tmp3 - SRC1: xDivXSubXi
                    Goldilocks3::load_avx(tmp3_1, params.xDivXSubXi[i + args[i_args + 3]*domainSize], params.xDivXSubXi.offset());
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp3_1);
                    i_args += 4;
                    break;
            }
            case 36: {
                    // OPERATION WITH DEST: q - SRC0: tmp3 - SRC1: Zi
                    Goldilocks::Element tmp_inv[3];
                    Goldilocks::Element ti0[4];
                    Goldilocks::Element ti1[4];
                    Goldilocks::Element ti2[4];
                    Goldilocks::store_avx(ti0, tmp3[args[i_args]][0]);
                    Goldilocks::store_avx(ti1, tmp3[args[i_args]][1]);
                    Goldilocks::store_avx(ti2, tmp3[args[i_args]][2]);
                    for (uint64_t j = 0; j < AVX_SIZE_; ++j) {
                        tmp_inv[0] = ti0[j];
                        tmp_inv[1] = ti1[j];
                        tmp_inv[2] = ti2[j];
                        Goldilocks3::mul((Goldilocks3::Element &)(params.q_2ns[(i + j) * 3]), params.zi[i + j][0],(Goldilocks3::Element &)tmp_inv);
                    }
                    i_args += 1;
                    break;
            }
            case 37: {
                    // OPERATION WITH DEST: f - SRC0: tmp3 - SRC1: tmp3
                    Goldilocks3::op_avx(args[i_args], tmp3_, tmp3[args[i_args + 1]], tmp3[args[i_args + 2]]);
                    Goldilocks3::store_avx(&params.f_2ns[i*3], uint64_t(3), tmp3_);
                    i_args += 3;
                    break;
            }
            case 38: {
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 39: {
                    Goldilocks::copy_avx(tmp1[args[i_args]], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::copy_avx(tmp1[args[i_args]], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::copy_avx(tmp1[args[i_args]], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::copy_avx(tmp1[args[i_args]], bufferT_[buffTOffsetsSteps_[args[i_args + 1]] + 2 * args[i_args + 2] + args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 40: {
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], bufferT_[buffTOffsetsSteps_[args[i_args + 5]] + 2 * args[i_args + 6] + args[i_args + 7]]);
                    i_args += 8;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], bufferT_[buffTOffsetsSteps_[args[i_args + 5]] + 2 * args[i_args + 6] + args[i_args + 7]]);
                    i_args += 8;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 41: {
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], bufferT_[buffTOffsetsSteps_[args[i_args + 5]] + 2 * args[i_args + 6] + args[i_args + 7]]);
                    i_args += 8;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], bufferT_[buffTOffsetsSteps_[args[i_args + 5]] + 2 * args[i_args + 6] + args[i_args + 7]]);
                    i_args += 8;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 42: {
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], bufferT_[buffTOffsetsSteps_[args[i_args + 5]] + 2 * args[i_args + 6] + args[i_args + 7]]);
                    i_args += 8;
                    break;
            }
            case 43: {
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 44: {
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], bufferT_[buffTOffsetsSteps_[args[i_args + 5]] + 2 * args[i_args + 6] + args[i_args + 7]]);
                    i_args += 8;
                    break;
            }
            case 45: {
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 46: {
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], bufferT_[buffTOffsetsSteps_[args[i_args + 5]] + 2 * args[i_args + 6] + args[i_args + 7]]);
                    i_args += 8;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 47: {
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], numbers_[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 48: {
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], tmp1[args[i_args + 2]], tmp1[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 49: {
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks::op_avx(args[i_args], tmp1[args[i_args + 1]], bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], tmp1[args[i_args + 5]]);
                    i_args += 6;
                    break;
            }
            case 50: {
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], evals[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp3[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], evals[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp3[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], evals[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp3[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], evals[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp3[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 51: {
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_31_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], bufferT_[buffTOffsetsSteps_[args[i_args + 3]] + 2 * args[i_args + 4] + args[i_args + 5]]);
                    i_args += 6;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
            case 52: {
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp3[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_avx(args[i_args], &(tmp3[args[i_args + 1]][0]), 1, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], 2, 
                        &(evals[args[i_args + 5]][0]), 1);
                    i_args += 6;
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp3[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::op_avx(args[i_args], &(tmp3[args[i_args + 1]][0]), 1, 
                        &bufferT_[buffTOffsetsSteps_[args[i_args + 2]] + 2 * args[i_args + 3] + args[i_args + 4]], 2, 
                        &(evals[args[i_args + 5]][0]), 1);
                    i_args += 6;
                    Goldilocks3::op_avx(args[i_args], tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], tmp3[args[i_args + 3]]);
                    i_args += 4;
                    Goldilocks3::mul_avx(tmp3[args[i_args + 1]], tmp3[args[i_args + 2]], challenges[args[i_args + 3]], challenges_ops[args[i_args + 3]]);
                    i_args += 4;
                    break;
            }
                default: {
                    std::cout << " Wrong operation!" << std::endl;
                    exit(1);
                }
            }
        }
        if (i_args != parserParams.nArgs) std::cout << " " << i_args << " - " << parserParams.nArgs << std::endl;
        assert(i_args == parserParams.nArgs);
    }
}