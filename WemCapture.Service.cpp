// WemCapture.Service.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <winhttp.h>

#include "WemCaptureService.h"
#include "ServiceDeploy.h"

int _tmain(int argc, PWSTR argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    SERVICE_TABLE_ENTRY dispatch[] =
    {
        {
            WemCaptureService::pwszServiceName, (LPSERVICE_MAIN_FUNCTION)WemCaptureService::ServiceMain
        },
        { NULL, NULL }
    };

    if (argv[1] && wcscmp(argv[1], L"install") == 0)
    {
        return ServiceDeploy::Install(WemCaptureService::pwszServiceName);
    }

    if (argv[1] && wcscmp(argv[1], L"remove") == 0)
    {
        return ServiceDeploy::Remove(WemCaptureService::pwszServiceName);
    }

    if (!StartServiceCtrlDispatcher(dispatch))
    {
        return 1;
    }

	return 0;
}
