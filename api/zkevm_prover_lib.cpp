
#include <stdio.h>
#include "starks.hpp"
#include "proof2zkinStark.hpp"
#include "api_steps.hpp"

int generateProof(std::string constPols, std::string constTree, std::string starkInfoFile, std::string commitPols, std::string verkey ){

    Config config;
    config.runFileGenBatchProof = true; // So that starkInfo is created
    config.mapConstPolsFile = false;
    config.mapConstantsTreeFile = false;

    StarkInfo starkInfo(config, starkInfoFile);

    uint64_t polBits = starkInfo.starkStruct.steps[starkInfo.starkStruct.steps.size() - 1].nBits;
    FRIProof fproof((1 << polBits), FIELD_EXTENSION, starkInfo.starkStruct.steps.size(), starkInfo.evMap.size(), starkInfo.nPublics);

    void *pCommit = copyFile(commitPols, starkInfo.nCm1 * sizeof(Goldilocks::Element) * (1 << starkInfo.starkStruct.nBits));
    void *pAddress = (void *)calloc(starkInfo.mapTotalN + (starkInfo.mapSectionsN.section[eSection::cm1_n] * (1 << starkInfo.starkStruct.nBits) * FIELD_EXTENSION ), sizeof(uint64_t));

    Starks starks(config, {constPols, config.mapConstPolsFile, constTree, starkInfoFile}, pAddress);

    starks.nrowsStepBatch = 4;

    uint64_t N = (1 << starkInfo.starkStruct.nBits);
    #pragma omp parallel for
    for (uint64_t i = 0; i < N; i += 1)
    {
        std::memcpy((uint8_t*)pAddress + i*starkInfo.nCm1*sizeof(Goldilocks::Element), (uint8_t*)pCommit + i*starkInfo.nCm1*sizeof(Goldilocks::Element), starkInfo.nCm1*sizeof(Goldilocks::Element));
    }

    Goldilocks::Element publicInputs[3] = {
        Goldilocks::fromU64(1),
        Goldilocks::fromU64(2),
        Goldilocks::fromU64(74469561660084004),
    };

    json publicStarkJson;
    for (int i = 0; i < 3; i++)
    {
        publicStarkJson[i] = Goldilocks::toString(publicInputs[i]);
    }
    // AllSteps steps;
    APISteps steps;

    json verkeyJson;
    file2json(verkey, verkeyJson);
    Goldilocks::Element Verkey[4];
    Verkey[0] = Goldilocks::fromU64(verkeyJson["constRoot"][0]);
    Verkey[1] = Goldilocks::fromU64(verkeyJson["constRoot"][1]);
    Verkey[2] = Goldilocks::fromU64(verkeyJson["constRoot"][2]);
    Verkey[3] = Goldilocks::fromU64(verkeyJson["constRoot"][3]);

    starks.genProof(fproof, &publicInputs[0], Verkey, &steps);

    nlohmann::ordered_json jProof = fproof.proofs.proof2json();
    nlohmann::json zkin = proof2zkinStark(jProof);
    // Generate publics
    jProof["publics"] = publicStarkJson;
    zkin["publics"] = publicStarkJson;

    json2file(publicStarkJson, "./publics.json");
    json2file(zkin, "./zkin.json");
    json2file(jProof, "./jProof.json");

    return 1;
}