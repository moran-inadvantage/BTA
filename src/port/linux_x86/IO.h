#pragma once

#include <memory>

#include "bta/BTAStatusTable.h"
#include "types.h"

using namespace std;

class IAVDSNode
{
public:
    virtual ERROR_CODE_T GetBTAdapterStatusTable(
        shared_ptr<IBTAdapterStatusTable> &pBTAdapterStatusTable);
};

class CIO
{
public:
    static shared_ptr<IAVDSNode> g_pNode;
};