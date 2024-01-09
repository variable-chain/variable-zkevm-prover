
#include <stdio.h>
#include <iostream>
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

int generateProof(std::string constPols, std::string constTree, std::string starkInfoFile, std::string commitPols, std::string verkey );
int generateProof(void * pConstPolsAddress, void* pConstTreeAddress, nlohmann::json starkInfoJson, void *pCommit, nlohmann::json verkeyJson);
