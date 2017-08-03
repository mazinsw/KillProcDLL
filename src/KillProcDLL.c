//
// KillProcDLL. �2003 by DITMan, based upon the KILL_PROC_BY_NAME function
// programmed by Ravi, reach him at: http://www.physiology.wisc.edu/ravi/
//
// You may use this source code in any of your projects, while you keep this
// header intact, otherwise you CAN NOT use this code.
//
// My homepage:
//    http://petra.uniovi.es/~i6948857/index.php
//


#include <windows.h>
#include <tlhelp32.h>
//To make it a NSIS Plug-In
#include "exdll.h"

#ifdef _UNICODE
#define TGET_MODULE_BASE_NAME "GetModuleBaseNameW"
#else
#define TGET_MODULE_BASE_NAME "GetModuleBaseNameA"
#endif

HINSTANCE g_hInstance;
HWND g_hwndParent;

BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    g_hInstance = hInst;
    return TRUE;
}

int KILL_PROC_BY_NAME(const TCHAR* szToTerminate)
// Created: 6/23/2000  (RK)
// Last modified: 3/10/2002  (RK)
// Please report any problems or bugs to kochhar@physiology.wisc.edu
// The latest version of this routine can be found at:
//     http://www.neurophys.wisc.edu/ravi/software/killproc/
// Terminate the process "szToTerminate" if it is currently running
// This works for Win/95/98/ME and also Win/NT/2000/XP
// The process name is case-insensitive, i.e. "notepad.exe" and "NOTEPAD.EXE"
// will both work (for szToTerminate)
// Return codes are as follows:
//   0   = Process was successfully terminated
//   603 = Process was not currently running
//   604 = No permission to terminate process
//   605 = Unable to load PSAPI.DLL
//   602 = Unable to terminate process for some other reason
//   606 = Unable to identify system type
//   607 = Unsupported OS
//   632 = Invalid process name
//   700 = Unable to get procedure address from PSAPI.DLL
//   701 = Unable to get process list, EnumProcesses failed
//   702 = Unable to load KERNEL32.DLL
//   703 = Unable to get procedure address from KERNEL32.DLL
//   704 = CreateToolhelp32Snapshot failed
// Change history:
//   modified 3/8/2002  - Borland-C compatible if BORLANDC is defined as
//                        suggested by Bob Christensen
//   modified 3/10/2002 - Removed memory leaks as suggested by
//                        Jonathan Richard-Brochu (handles to Proc and Snapshot
//                        were not getting closed properly in some cases)
{
    BOOL bResult, bResultm;
    DWORD aiPID[1000], iCb = 1000, iNumProc;
    DWORD iCbneeded, i, iFound = 0;
    TCHAR szName[MAX_PATH], szToTermUpper[MAX_PATH];
    HANDLE hProc, hSnapShot, hSnapShotm;
    OSVERSIONINFO osvi;
    HINSTANCE hInstLib = 0;
    int iLenP, indx;
    HMODULE hMod;
    PROCESSENTRY32 procentry;
    MODULEENTRY32 modentry;

    // Transfer Process name into "szToTermUpper" and
    // convert it to upper case
    iLenP = _tcslen(szToTerminate);
    if (iLenP < 1 || iLenP > MAX_PATH) return 632;
    for (indx = 0; indx < iLenP; indx++)
        szToTermUpper[indx] = toupper(szToTerminate[indx]);
    szToTermUpper[iLenP] = 0;

    // PSAPI Function Pointers.
    BOOL (WINAPI * lpfEnumProcesses)(DWORD*, DWORD cb, DWORD*);
    BOOL (WINAPI * lpfEnumProcessModules)(HANDLE, HMODULE*,
                                          DWORD, LPDWORD);
    DWORD (WINAPI * lpfGetModuleBaseName)(HANDLE, HMODULE,
                                          LPTSTR, DWORD);

    // ToolHelp Function Pointers.
    HANDLE(WINAPI * lpfCreateToolhelp32Snapshot)(DWORD, DWORD) ;
    BOOL (WINAPI * lpfProcess32First)(HANDLE, LPPROCESSENTRY32) ;
    BOOL (WINAPI * lpfProcess32Next)(HANDLE, LPPROCESSENTRY32) ;
    BOOL (WINAPI * lpfModule32First)(HANDLE, LPMODULEENTRY32) ;
    BOOL (WINAPI * lpfModule32Next)(HANDLE, LPMODULEENTRY32) ;

    // First check what version of Windows we're in
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    bResult = GetVersionEx(&osvi);
    if (!bResult)    // Unable to identify system version
        return 606;

    // At Present we only support Win/NT/2000/XP or Win/9x/ME
    if ((osvi.dwPlatformId != VER_PLATFORM_WIN32_NT) &&
        (osvi.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS))
        return 607;
    
    // skip killing current process
    DWORD cpID = GetCurrentProcessId();

    if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        // Win/NT or 2000 or XP

        // Load library and get the procedures explicitly. We do
        // this so that we don't have to worry about modules using
        // this code failing to load under Windows 9x, because
        // it can't resolve references to the PSAPI.DLL.
        hInstLib = LoadLibrary(_T("PSAPI.DLL"));
        if (hInstLib == NULL)
            return 605;

        // Get procedure addresses.
        lpfEnumProcesses = (BOOL(WINAPI*)(DWORD*, DWORD, DWORD*))
                           GetProcAddress(hInstLib, "EnumProcesses") ;
        lpfEnumProcessModules = (BOOL(WINAPI*)(HANDLE, HMODULE*,
                                               DWORD, LPDWORD)) GetProcAddress(hInstLib,
                                                       "EnumProcessModules") ;
        lpfGetModuleBaseName = (DWORD (WINAPI*)(HANDLE, HMODULE,
                                                LPTSTR, DWORD)) GetProcAddress(hInstLib,
                                                        TGET_MODULE_BASE_NAME
                                                    ) ;

        if (lpfEnumProcesses == NULL ||
            lpfEnumProcessModules == NULL ||
            lpfGetModuleBaseName == NULL)
        {
            FreeLibrary(hInstLib);
            return 700;
        }

        bResult = lpfEnumProcesses(aiPID, iCb, &iCbneeded);
        if (!bResult)
        {
            // Unable to get process list, EnumProcesses failed
            FreeLibrary(hInstLib);
            return 701;
        }

        // How many processes are there?
        iNumProc = iCbneeded / sizeof(DWORD);

        // Get and match the name of each process
        for (i = 0; i < iNumProc; i++)
        {
            // skip kill self process
            if (cpID == aiPID[i])
            {
                continue;
            }
            // Get the (module) name for this process

            _tcscpy(szName, _T("Unknown"));
            // First, get a handle to the process
            hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE,
                                aiPID[i]);
            // Now, get the process name
            if (hProc)
            {
                if (lpfEnumProcessModules(hProc, &hMod, sizeof(hMod), &iCbneeded))
                {
                    lpfGetModuleBaseName(hProc, hMod, szName, MAX_PATH);
                }
            }
            CloseHandle(hProc);
            // We will match regardless of lower or upper case
#ifdef BORLANDC
            if (_tcscmp(_tcsupr(szName), szToTermUpper) == 0)
#else
            if (_tcscmp(_tcsupr(szName), szToTermUpper) == 0)
#endif
            {
                // Process found, now terminate it
                iFound = 1;
                // First open for termination
                hProc = OpenProcess(PROCESS_TERMINATE, FALSE, aiPID[i]);
                if (hProc)
                {
                    if (TerminateProcess(hProc, 0))
                    {
                        // process terminated
                        CloseHandle(hProc);
                        FreeLibrary(hInstLib);
                        return 0;
                    }
                    else
                    {
                        // Unable to terminate process
                        CloseHandle(hProc);
                        FreeLibrary(hInstLib);
                        return 602;
                    }
                }
                else
                {
                    // Unable to open process for termination
                    FreeLibrary(hInstLib);
                    return 604;
                }
            }
        }
    }

    if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    {
        // Win/95 or 98 or ME

        hInstLib = LoadLibrary(_T("Kernel32.DLL"));
        if (hInstLib == NULL)
            return 702;

        // Get procedure addresses.
        // We are linking to these functions of Kernel32
        // explicitly, because otherwise a module using
        // this code would fail to load under Windows NT,
        // which does not have the Toolhelp32
        // functions in the Kernel 32.
        lpfCreateToolhelp32Snapshot =
            (HANDLE(WINAPI*)(DWORD, DWORD))
            GetProcAddress(hInstLib,
                           "CreateToolhelp32Snapshot") ;
        lpfProcess32First =
            (BOOL(WINAPI*)(HANDLE, LPPROCESSENTRY32))
            GetProcAddress(hInstLib, "Process32First") ;
        lpfProcess32Next =
            (BOOL(WINAPI*)(HANDLE, LPPROCESSENTRY32))
            GetProcAddress(hInstLib, "Process32Next") ;
        lpfModule32First =
            (BOOL(WINAPI*)(HANDLE, LPMODULEENTRY32))
            GetProcAddress(hInstLib, "Module32First") ;
        lpfModule32Next =
            (BOOL(WINAPI*)(HANDLE, LPMODULEENTRY32))
            GetProcAddress(hInstLib, "Module32Next") ;
        if (lpfProcess32Next == NULL ||
            lpfProcess32First == NULL ||
            lpfModule32Next == NULL ||
            lpfModule32First == NULL ||
            lpfCreateToolhelp32Snapshot == NULL)
        {
            FreeLibrary(hInstLib);
            return 703;
        }

        // The Process32.. and Module32.. routines return names in all uppercase

        // Get a handle to a Toolhelp snapshot of all the systems processes.

        hSnapShot = lpfCreateToolhelp32Snapshot(
                        TH32CS_SNAPPROCESS, 0) ;
        if (hSnapShot == INVALID_HANDLE_VALUE)
        {
            FreeLibrary(hInstLib);
            return 704;
        }

        // Get the first process' information.
        procentry.dwSize = sizeof(PROCESSENTRY32);
        bResult = lpfProcess32First(hSnapShot, &procentry);

        // While there are processes, keep looping and checking.
        while (bResult)
        {
            // Get a handle to a Toolhelp snapshot of this process.
            hSnapShotm = lpfCreateToolhelp32Snapshot(
                             TH32CS_SNAPMODULE, procentry.th32ProcessID) ;
            if (hSnapShotm == INVALID_HANDLE_VALUE)
            {
                CloseHandle(hSnapShot);
                FreeLibrary(hInstLib);
                return 704;
            }
            // Get the module list for this process
            modentry.dwSize = sizeof(MODULEENTRY32);
            bResultm = lpfModule32First(hSnapShotm, &modentry);

            // While there are modules, keep looping and checking
            while (bResultm)
            {
                if (_tcscmp(modentry.szModule, szToTermUpper) == 0)
                {
                    // Process found, now terminate it
                    iFound = 1;
                    // First open for termination
                    hProc = OpenProcess(PROCESS_TERMINATE, FALSE, procentry.th32ProcessID);
                    if (hProc)
                    {
                        if (TerminateProcess(hProc, 0))
                        {
                            // process terminated
                            CloseHandle(hSnapShotm);
                            CloseHandle(hSnapShot);
                            CloseHandle(hProc);
                            FreeLibrary(hInstLib);
                            return 0;
                        }
                        else
                        {
                            // Unable to terminate process
                            CloseHandle(hSnapShotm);
                            CloseHandle(hSnapShot);
                            CloseHandle(hProc);
                            FreeLibrary(hInstLib);
                            return 602;
                        }
                    }
                    else
                    {
                        // Unable to open process for termination
                        CloseHandle(hSnapShotm);
                        CloseHandle(hSnapShot);
                        FreeLibrary(hInstLib);
                        return 604;
                    }
                }
                else
                {   // Look for next modules for this process
                    modentry.dwSize = sizeof(MODULEENTRY32);
                    bResultm = lpfModule32Next(hSnapShotm, &modentry);
                }
            }

            //Keep looking
            CloseHandle(hSnapShotm);
            procentry.dwSize = sizeof(PROCESSENTRY32);
            bResult = lpfProcess32Next(hSnapShot, &procentry);
        }
        CloseHandle(hSnapShot);
    }
    if (iFound == 0)
    {
        FreeLibrary(hInstLib);
        return 603;
    }
    FreeLibrary(hInstLib);
    return 0;
}


//
// This is the only exported function, KillProc. It receives the name
// of a process through the NSIS stack. The return-value from the
// KILL_PROC_BY_NAME function is stored in the $R0 variable, so push
// it before calling KillProc function if you don't want to lose the
// data that R0 could contain.
//
// You can call this function in NSIS like this:
//
// KillProcDLL::KillProc "process_name.exe"
//
// example:
//    KillProcDLL::KillProc "msnmsgr.exe"
//  would close MSN Messenger if running, and return 0 in R0
//  if it's not running, it would return 603.
//
//  ---------------------------      ----     ---    --   -

#ifdef __cplusplus
extern "C" {
#endif
void __declspec(dllexport) KillProc(HWND hwndParent, int string_size,
        TCHAR* variables, stack_t** stacktop,
        extra_parameters* extra)
{
    TCHAR parameter[200];
    TCHAR temp[12];
    int value;
    g_hwndParent = hwndParent;
    EXDLL_INIT();
    {
        popstring(parameter);
        value = KILL_PROC_BY_NAME(parameter);
        wsprintf(temp, _T("%d"), value);
        setuservariable(INST_R0, temp);
    }
}
#ifdef __cplusplus
}
#endif