#include "stdafx.h"
#include "ServiceDeploy.h"
#pragma comment(lib, "Advapi32")

HRESULT ServiceDeploy::Install(PWSTR pwszServiceName)
{
    HRESULT hr;
    WCHAR szPath[MAX_PATH];

    SC_HANDLE scManager = NULL;
    SC_HANDLE scService = NULL;

    if (!GetModuleFileName(NULL, szPath, MAX_PATH))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto INSTALL_EXIT;
    }

    scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scManager == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto INSTALL_EXIT;
    }

    scService = CreateService(scManager,
                              pwszServiceName,
                              pwszServiceName,
                              SERVICE_ALL_ACCESS,
                              SERVICE_WIN32_OWN_PROCESS,
                              SERVICE_AUTO_START,
                              SERVICE_ERROR_NORMAL,
                              szPath,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              NULL);
    if (scService == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto INSTALL_EXIT;
    }

    hr = S_OK;

INSTALL_EXIT:
    if (scService != NULL)
    {
        CloseServiceHandle(scService);
    }

    if (scManager != NULL)
    {
        CloseServiceHandle(scManager);
    }

    return hr;
}

HRESULT ServiceDeploy::Remove(PWSTR pwszServiceName)
{
    HRESULT hr;
    SERVICE_STATUS status;

    SC_HANDLE scManager = NULL;
    SC_HANDLE scService = NULL;

    ZeroMemory(&status, sizeof(status));

    scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (scManager == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto REMOVE_EXIT;
    }

    scService = OpenService(scManager, pwszServiceName, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
    if (scService == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto REMOVE_EXIT;
    }

    if (ControlService(scService, SERVICE_CONTROL_STOP, &status))
    {
        Sleep(1000);
        while (QueryServiceStatus(scService, &status))
        {
            if (status.dwCurrentState != SERVICE_STOP_PENDING)
            {
                break;
            }
            Sleep(1000);
        }
    }

    if (!DeleteService(scService))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto REMOVE_EXIT;
    }

    hr = S_OK;

REMOVE_EXIT:
    if (scManager)
    {
        CloseServiceHandle(scManager);
    }
    if (scService)
    {
        CloseServiceHandle(scService);
    }

    return hr;
}
