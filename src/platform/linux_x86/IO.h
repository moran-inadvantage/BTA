#pragma once

#include <memory>

#include "types.h"

using namespace std;

class IBTAdapterStatusTable;

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