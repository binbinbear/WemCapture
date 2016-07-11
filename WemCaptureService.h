#pragma once

#include <Windows.h>

class WemCaptureService
{
public:
    static const PWSTR pwszServiceName;

    static VOID WINAPI ServiceMain(DWORD dwArgs, LPWSTR *lpwszArgs);

private:
    static DWORD WINAPI ServiceControl(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);

    static HRESULT Init();
    static HRESULT CreateStatusHandle();
    static HRESULT SetStatus(DWORD dwStatus);
    static VOID FinalizeService();
    static HRESULT SessionChanged(DWORD dwSessionEvent, PWTSSESSION_NOTIFICATION lpNotification);
    static HRESULT StartCaptureAgent(DWORD dwSessionId);
    static HRESULT CreateWemAgent(HANDLE hToken);

    static SERVICE_STATUS_HANDLE hStatus;
    static HANDLE hStop;
};
