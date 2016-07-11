#include "stdafx.h"
#include <Windows.h>

class ServiceDeploy
{
public:
    static HRESULT Install(PWSTR pwszServiceName);
    static HRESULT Remove(PWSTR pwszServiceName);
};
