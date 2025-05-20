#pragma once

#include <memory>

#include "CPPInterfaces.h"
#include "types.h"

using namespace std;

class IAVDSNode
{
  public:
    virtual ERROR_CODE_T GetBTAdapterConfigTable(shared_ptr<IBTAdapterConfigTable> &pBTAdapterConfigTable);
};

class CIO
{
  public:
    static shared_ptr<IAVDSNode> g_pNode;
};