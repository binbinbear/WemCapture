// TestApp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <Windows.h>
#include <WtsApi32.h>
#include <stdio.h>
#include <VirtDisk.h>
#pragma comment(lib,"Advapi32.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "VirtDisk.lib")

static const GUID VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT = { 0xEC984AEC, 0xA0F9, 0x47e9, 0x90, 0x1F, 0x71, 0x41, 0x5A, 0x66, 0x34, 0x5B };

HRESULT CreateAgent(HANDLE hToken)
{
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.lpDesktop = L"winsta0\\default";
    si.wShowWindow = SW_SHOW;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    wchar_t *szPath = L"C:\\Program Files (x86)\\CloudVolumes\\Agent";
    wchar_t *szCmd = L"C:\\Program Files (x86)\\CloudVolumes\\Agent\\svservice.exe";
    wchar_t *szParameter = L"svservice app enable \"Google Chrome\"";

    if (!CreateProcessAsUser(hToken,
        szCmd,
        szParameter,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        szPath,
        &si,
        &pi))
    {
        printf("Failed to create process, error: %d\n", GetLastError());
        return HRESULT_FROM_WIN32(GetLastError());
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    ZeroMemory(&pi, sizeof(pi));

    szParameter = L"svservice activate filter";

    if (!CreateProcessAsUser(hToken,
        szCmd,
        szParameter,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        szPath,
        &si,
        &pi))
    {
        printf("Failed to create process, error: %d\n", GetLastError());
        return HRESULT_FROM_WIN32(GetLastError());
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return S_OK;
}

void MountVhd()
{
    OPEN_VIRTUAL_DISK_PARAMETERS openParameters;
    openParameters.Version = OPEN_VIRTUAL_DISK_VERSION_1;
    openParameters.Version1.RWDepth = OPEN_VIRTUAL_DISK_RW_DEPTH_DEFAULT;

    VIRTUAL_STORAGE_TYPE storageType;
    storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
    storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

    ATTACH_VIRTUAL_DISK_PARAMETERS attachParameters;
    attachParameters.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    HANDLE vhdHandle;

    if (OpenVirtualDisk(&storageType,
        L"c:\\vhd\\chrome-win7-x64.vhd",
        VIRTUAL_DISK_ACCESS_ATTACH_RO,
        OPEN_VIRTUAL_DISK_FLAG_NONE,
        &openParameters,
        &vhdHandle) != ERROR_SUCCESS)
    {
        printf("failed to open vhd, error: %d", GetLastError());
        return;
    }

    if (AttachVirtualDisk(vhdHandle,
        0,
        ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY | ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME,
        0,
        &attachParameters,
        0) != ERROR_SUCCESS) {
        printf("failed to attach vhd, error: %d", GetLastError());
    }
}

HRESULT StartAgent(DWORD dwSessionId)
{
    HRESULT hr = S_OK;
    HANDLE hToken = NULL;
    HANDLE hDupToken = NULL;
    HANDLE hTokenToDup = NULL;
    TOKEN_LINKED_TOKEN linkedToken = { 0 };
    TOKEN_ELEVATION_TYPE elevationType;
    DWORD dwSize;

    if (!WTSQueryUserToken(dwSessionId, &hToken))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        printf("Failed to WTSQueryUserToken, error: %d\n", GetLastError());
        goto SA_EXIT;
    }

    dwSize = sizeof(elevationType);
    if (!GetTokenInformation(hToken, TokenElevationType, &elevationType, dwSize, &dwSize))
    {
        printf("Failed to GetTokenInformation, error: %d\n", GetLastError());
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto SA_EXIT;
    }

    hTokenToDup = hToken;
    if (elevationType == TokenElevationTypeLimited)
    {
        dwSize = sizeof(linkedToken);
        if (!GetTokenInformation(hToken, TokenLinkedToken, &linkedToken, dwSize, &dwSize))
        {
            printf("Failed to GetTokenInformation, error: %d\n", GetLastError());
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto SA_EXIT;
        }
        hTokenToDup = linkedToken.LinkedToken;
    }

    if (!DuplicateTokenEx(hTokenToDup, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &hDupToken))
    {
        printf("Failed to DuplicateTokenEx, error: %d\n", GetLastError());
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto SA_EXIT;
    }

    MountVhd();

    hr = CreateAgent(hDupToken);

SA_EXIT:

    if (hToken != NULL)
    {
        CloseHandle(hToken);
    }

    if (linkedToken.LinkedToken != NULL)
    {
        CloseHandle(linkedToken.LinkedToken);
    }

    if (hDupToken != NULL)
    {
        CloseHandle(hDupToken);
    }

    return hr;
}

int main()
{
   printf("started\n");
    StartAgent(1);
	//MountVhd();
	//CreateAgent();
    printf("finished\n");
    getchar();
    return 0;
}

