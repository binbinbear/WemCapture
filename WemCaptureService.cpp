#include "stdafx.h"
#include "WemCaptureService.h"

#include <WtsApi32.h>
#include <tlhelp32.h>
#include <VirtDisk.h>

#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "VirtDisk.lib")


const GUID VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT = { 0xEC984AEC, 0xA0F9, 0x47e9, 0x90, 0x1F, 0x71, 0x41, 0x5A, 0x66, 0x34, 0x5B };

SERVICE_STATUS_HANDLE WemCaptureService::hStatus = NULL;
HANDLE WemCaptureService::hStop = NULL;
const PWSTR WemCaptureService::pwszServiceName = L"VMware WEM Capture Service";

VOID WINAPI WemCaptureService::ServiceMain(DWORD dwArgs, LPWSTR *lppszArgs)
{
    UNREFERENCED_PARAMETER(dwArgs);
    UNREFERENCED_PARAMETER(lppszArgs);

    HRESULT hr = Init();
    if (!SUCCEEDED(hr))
    {
        goto SVR_STOP;
    }

    hr = SetStatus(SERVICE_RUNNING);
    if (!SUCCEEDED(hr))
    {
        goto SVR_STOP;
    }

    WaitForSingleObject(hStop, INFINITE);

SVR_STOP:
    FinalizeService();
    SetStatus(SERVICE_STOPPED);
}

DWORD WINAPI WemCaptureService::ServiceControl(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    UNREFERENCED_PARAMETER(dwEventType);
    UNREFERENCED_PARAMETER(lpEventData);
    UNREFERENCED_PARAMETER(lpContext);

    switch (dwControl)
    {
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
        SetStatus(SERVICE_STOP_PENDING);
        SetEvent(hStop);
        break;

    case SERVICE_CONTROL_SESSIONCHANGE:
        SessionChanged(dwEventType, (PWTSSESSION_NOTIFICATION)lpEventData);
        break;

    default:
        break;
    }
    return 0;
}

HRESULT WemCaptureService::Init()
{
    HRESULT hr = CreateStatusHandle();
    if (!SUCCEEDED(hr))
    {
        return hr;
    }

    // TODO: security!!!
    hStop = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hStop == NULL)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

HRESULT WemCaptureService::CreateStatusHandle()
{
    hStatus = RegisterServiceCtrlHandlerEx(pwszServiceName, ServiceControl, NULL);
    if (hStatus == NULL)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

HRESULT WemCaptureService::SetStatus(DWORD dwStatus)
{
    SERVICE_STATUS status;
    ZeroMemory(&status, sizeof(status));

    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;
    status.dwCurrentState = dwStatus;
    status.dwWin32ExitCode = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint = 0;

    if (!SetServiceStatus(hStatus, &status))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

VOID WemCaptureService::FinalizeService()
{
    if (hStop != NULL)
    {
        CloseHandle(hStop);
    }
}

HRESULT WemCaptureService::SessionChanged(DWORD dwSessionEventType, PWTSSESSION_NOTIFICATION lpNotification)
{
    switch (dwSessionEventType)
    {
    case WTS_SESSION_LOGON:
        return StartCaptureAgent(lpNotification->dwSessionId);
    }

    return S_OK;
}

static HRESULT MountVhd(PCWSTR path)
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
		path,
       // L"c:\\vhd\\chrome-win7-x64.vhd", //TODO: get the vhd location from AV manager
        VIRTUAL_DISK_ACCESS_ATTACH_RO,
        OPEN_VIRTUAL_DISK_FLAG_NONE,
        &openParameters,
        &vhdHandle) != ERROR_SUCCESS)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (AttachVirtualDisk(vhdHandle,
        0,
        ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY | ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME,
        0,
        &attachParameters,
        0) != ERROR_SUCCESS)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

static PCWSTR GetVhdPath()
{
	HINTERNET hSession = WinHttpOpen(L"svservice",
		WINHTTP_ACCESS_TYPE_NO_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0);
	PCWSTR ans = NULL;
	DWORD dwValue = 2;
	BOOL bRet = WinHttpSetOption(hSession,
		WINHTTP_OPTION_CONNECT_RETRIES,
		&dwValue,
		sizeof(dwValue));

	dwValue = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	bRet = WinHttpSetOption(hSession,
		WINHTTP_OPTION_REDIRECT_POLICY,
		&dwValue,
		sizeof(dwValue));

	/*
	dwValue = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
	bRet = WinHttpSetOption(hSession,
	WINHTTP_OPTION_SECURE_PROTOCOLS,
	&dwValue,
	sizeof(dwValue));
	*/

	dwValue = 0;

	HINTERNET hConnect = WinHttpConnect(hSession,
		//L"10.1.23.4",
		L"10.112.116.48",
		3001,
		0);

	HINTERNET hRqst = WinHttpOpenRequest(hConnect,
		L"GET",
		L"/user-logon-from-ws?name=alice",
		NULL,
		WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		0);

	/*
	dwValue = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
	bRet = WinHttpSetOption(hRqst,
	WINHTTP_OPTION_SECURITY_FLAGS,
	&dwValue,
	sizeof(dwValue));
	*/

	bRet = WinHttpSetCredentials(hRqst,
		WINHTTP_AUTH_TARGET_SERVER,
		WINHTTP_AUTH_SCHEME_NTLM,
		NULL,
		NULL,
		NULL);

	dwValue = WINHTTP_AUTOLOGON_SECURITY_LEVEL_LOW;
	bRet = WinHttpSetOption(hRqst,
		WINHTTP_OPTION_AUTOLOGON_POLICY,
		&dwValue,
		sizeof(dwValue));

	bRet = WinHttpSendRequest(hRqst,
		WINHTTP_NO_ADDITIONAL_HEADERS,
		0,
		WINHTTP_NO_REQUEST_DATA,
		0,
		0,
		0);
	/*bRet = WinHttpReceiveResponse(hRqst, NULL);

	DWORD dwStatus;
	dwValue = sizeof(DWORD);
	bRet = WinHttpQueryHeaders(hRqst,
	WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
	NULL,
	&dwStatus,
	&dwValue,
	NULL);*/

	//DWORD dwErr = GetLastError();
	if (bRet)
		bRet = WinHttpReceiveResponse(hRqst, NULL);

	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;
	// Keep checking for data until there is nothing left.
	if (bRet)
	{
		do
		{
			// Check for available data.
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRqst, &dwSize))
			{
				printf("Error %u in WinHttpQueryDataAvailable.\n",
					GetLastError());
				break;
			}

			// No more available data.
			if (!dwSize)
				break;

			// Allocate space for the buffer.
			WCHAR* pszOutBuffer = new WCHAR[dwSize + 1];
			if (!pszOutBuffer)
			{
				printf("Out of memory\n");
				break;
			}

			// Read the Data.
			ZeroMemory(pszOutBuffer, sizeof(WCHAR)*(dwSize + 1));

			if (!WinHttpReadData(hRqst, (LPVOID)pszOutBuffer,
				dwSize, &dwDownloaded))
			{
				printf("Error %u in WinHttpReadData.\n", GetLastError());
			}
			else
			{
				//    wprintf(L"%s", pszOutBuffer);
				//printf("%s", pszOutBuffer);
				ans = pszOutBuffer;
			}

			// Free the memory allocated to the buffer.
			//delete[] pszOutBuffer;

			// This condition should never be reached since WinHttpQueryDataAvailable
			// reported that there are bits to read.
			if (!dwDownloaded)
				break;

		} while (dwSize > 0);
	}
	else
	{
		// Report any errors.
		printf("Error %d has occurred.\n", GetLastError());
	}

	// Close any open handles.
	if (hRqst) WinHttpCloseHandle(hRqst);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);
	//wprintf(L"%s", ans);
	return ans;
}

HRESULT WemCaptureService::StartCaptureAgent(DWORD dwSessionId)
{
    HRESULT hr;
    HANDLE hToken = NULL;
    HANDLE hDupToken = NULL;
    HANDLE hTokenToDup = NULL;
    TOKEN_LINKED_TOKEN linkedToken = { 0 };
    TOKEN_ELEVATION_TYPE elevationType;
    DWORD dwSize;

	PCWSTR vhdPath = GetVhdPath();

    //hr = MountVhd(vhdPath);
	//delete vhdPath;
	hr = MountVhd(L"\\10.117.162.250\Users\chrome-win7-x64.vhd");
    if (FAILED(hr))
    {
        goto SA_EXIT;
    }

    if (!WTSQueryUserToken(dwSessionId, &hToken))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto SA_EXIT;
    }

    dwSize = sizeof(elevationType);
    if (!GetTokenInformation(hToken, TokenElevationType, &elevationType, dwSize, &dwSize))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto SA_EXIT;
    }

    hTokenToDup = hToken;
    if (elevationType == TokenElevationTypeLimited)
    {
        dwSize = sizeof(linkedToken);
        if (!GetTokenInformation(hToken, TokenLinkedToken, &linkedToken, dwSize, &dwSize))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto SA_EXIT;
        }
        hTokenToDup = linkedToken.LinkedToken;
    }

    if (!DuplicateTokenEx(hTokenToDup, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &hDupToken))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto SA_EXIT;
    }
    
    hr = CreateWemAgent(hDupToken);

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

HRESULT WemCaptureService::CreateWemAgent(HANDLE hToken)
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
    wchar_t *szParameter = L"svservice app enable \"Google Chrome\""; //TODO: get the app name from AV manager

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
        return HRESULT_FROM_WIN32(GetLastError());
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return S_OK;
}
