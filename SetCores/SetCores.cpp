// SetCores.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "SetCores.h"
#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <algorithm>
#include "Windowsx.h"
#include "message_map.h"
#include "Winuser.h"
#include "resource.h"
#include <psapi.h>

#define MAX_LOADSTRING 100
#define IDT_TIMER1 123422

#ifndef WS_VSCROLL
#define WS_VSCROLL 0x00200000L
#endif

//#define BM_GETSTATE = 0xF2
#define BST_CHECKED 0x1
#define BST_FOCUS   0x8
#define BST_INDETERMINATE 0x2
#define BST_PUSHED 0x4
#define BST_UNCHECKED 0x0

std::vector<HWND> CoreToggleButtons;
HWND listHwnd = 0;
HWND appHwnd = 0;
// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name



// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
DWORD findProcessIDByName(const WCHAR *processName);
HANDLE findProcessByName(const WCHAR *processName);
void SetButtonToggleStatus(DWORD affinityMask);
//#define DEBUG_OUTPUT_WINCOMMANDS
//#define DEBUG_OUTPUT_DLGCOMMANDS
#define DEBUG_OUTPUT_UNHANDLEDCOMMANDS


void OutputErrorMessage(DWORD msg)
{
	enum { size = 1024 };
	char buffer[size];
	if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL ,
		msg,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buffer,
		size,
		NULL))
	{
		std::snprintf(buffer, size, "Format message failed with 0x%x\n", msg);
		
	}		
	OutputDebugStringA(buffer);
}


BOOL IsProcessRunning(DWORD pid)
{
	HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
	DWORD ret = WaitForSingleObject(process, 0);
	CloseHandle(process);
	return ret == WAIT_TIMEOUT;
}


/*

INT_PTR CALLBACK ListBoxExampleProc(HWND hDlg, UINT message,
	WPARAM wParam, LPARAM lParam)
{


	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;

		
		return TRUE;
		}
	}
	return FALSE;
}
*/

struct ProcessAffinityInfo
{
	ProcessAffinityInfo(const wchar_t *processName)
		:szProcessName(processName)
		, processInstance(0)
		, desiredAffinity(0xFFFFFFFF)
		,processID(0)
	{
		processID = findProcessIDByName(szProcessName.c_str());
		desiredAffinity = getCurrentAffinity();		
	}
	std::wstring szProcessName;
	DWORD processID;
	HANDLE processInstance;
	DWORD desiredAffinity;
	static DWORD sysAffinity;

	HANDLE GetProcess()
	{
		if ((processID == 0) || (!IsProcessRunning(processID)))
		{
			processID = findProcessIDByName(szProcessName.c_str());
		}
		if (processID !=0)
		{
			return OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
		}
		return 0;
	}

	ULONG getCurrentAffinity()
	{
		processInstance = GetProcess();
		DWORD affinity = 0;
		GetProcessAffinityMask(processInstance, &affinity, &sysAffinity);
		CloseHandle(processInstance);
		return affinity;
	}

	void PopulateUI()
	{
		ULONG affinity = getCurrentAffinity();
		
		SetButtonToggleStatus(affinity);
	}

	void SetCore(UINT32 coreIdx, bool state)
	{
		DWORD oldAffinity = desiredAffinity;
		UINT32 mask = 1 << coreIdx;
		
		if (state)
			desiredAffinity |= mask;
		else
			desiredAffinity &= ~mask;
		if (desiredAffinity != oldAffinity)
		{
			
			SetCores();
		}
	}

	void SetDesiredAffinity(DWORD affinityMask)
	{
		if (desiredAffinity != affinityMask)
		{
			desiredAffinity = affinityMask & sysAffinity;
			SetCores();
		}
	}

	//actually set the affinity on the process that this object refers to
	void SetCores()
	{
		char infotext[512];		
		std::snprintf(infotext, 512, "Setting core affinity: %i \n", desiredAffinity);		
		OutputDebugStringA(infotext);
		processInstance = GetProcess();
		
		DWORD tempAffinity = desiredAffinity;
		BOOL res  =SetProcessAffinityMask(processInstance, (ULONG_PTR)tempAffinity);
		if (!res)
		{
			DWORD err = GetLastError();
			OutputErrorMessage(err);
		}
		CloseHandle(processInstance);		
	}
};

std::vector<ProcessAffinityInfo> mMonitoredProcesses;
UINT32 currentlySelectedProcess = 0;

DWORD ProcessAffinityInfo::sysAffinity = 0xFFFF;




bool handleListSelection(DWORD wParam)
{
	//case IDC_LISTBOX_EXAMPLE:
	{
		switch (HIWORD(wParam))
		{
			case LBN_SELCHANGE:
			{
				// Get selected index.
				int lbItem = (int)SendMessage(listHwnd, LB_GETCURSEL, 0, 0);

				// Get item data.
				int i = (int)SendMessage(listHwnd, LB_GETITEMDATA, lbItem, 0);

				// Do something with the data from Roster[i]

				//SetDlgItemText(hDlg, IDC_STATISTICS, buff);
				return true;
			}
		}
	}
	return false;
}


//L"3dsmax.exe"


void populateList(HWND hwnd = listHwnd)
{
	(int)SendMessage(hwnd, LB_RESETCONTENT, 0, 0);
	for (size_t i = 0; i < mMonitoredProcesses.size(); i++)
	{
		int pos = (int)SendMessage(hwnd, LB_ADDSTRING, 0,
			(LPARAM)mMonitoredProcesses[i].szProcessName.c_str());

		SendMessage(hwnd, LB_SETITEMDATA, pos, (LPARAM)i);
	}
	// Set input focus to the list box.
	SetFocus(hwnd);
}

TCHAR* GetProcessNameAndID(DWORD processID)
{
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
	static TCHAR szFormattedName[MAX_PATH];

	// Get a handle to the process.

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

	// Get the process name.

	if (hProcess != NULL)
	{
		HMODULE hMod = NULL;
		DWORD cbNeeded = 0;

		if (EnumProcessModules(hProcess, &hMod, sizeof(hMod),&cbNeeded))
		{
			GetModuleBaseName(hProcess, hMod, szProcessName,sizeof(szProcessName) / sizeof(TCHAR));
		}
		else
		{
			OutputErrorMessage(GetLastError());
			GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
		}


	}

	// Print the process name and identifier.


	std::swprintf(szFormattedName, MAX_PATH, L"%s  (PID: %u)", szProcessName, processID);

	// Release the handle to the process.
	CloseHandle(hProcess);
	return szFormattedName;
}

void populateProcessSelectionList(HWND hwnd)
{
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i = 0;

	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
	{
		return;
	}

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	HANDLE hProcess = 0;
	
	
	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			const TCHAR * name = entry.szExeFile;
			int pos = (int)SendMessage(hwnd, LB_ADDSTRING, 0,
				(LPARAM)name);
			SendMessage(hwnd, LB_SETITEMDATA, pos, (LPARAM)i);
			++i;
		}
	}


	// Set input focus to the list box.
	SetFocus(hwnd);
}


int rectWidth(const RECT &in)
{
	return abs(in.right - in.left);
}

int rectHeight(const RECT &in)
{
	return abs(in.top - in.bottom);
}

int rectXPos(const RECT &in, float split)
{
	return in.left + (int)(rectWidth(in) *split);
}


RECT getInnerRect(const RECT &rect, const RECT &padding)
{
	RECT r = rect;
	r.left += padding.left;
	r.right -= padding.right;
	r.top += padding.top;
	r.bottom -= padding.bottom;
	return r;
}

template <class LIST_TYPE>
void wrapRowResize(const RECT &bounds, const RECT &padding, const POINT &spacing, LIST_TYPE &childWindows)
{
	RECT innerRect = getInnerRect(bounds, padding);
	int cx = innerRect.left;
	int cy = innerRect.top;
	for (auto c : childWindows)
	{
		RECT rect;
		GetWindowRect(c, &rect);
		int w = rectWidth(rect);
		int h = rectHeight(rect);

		MoveWindow(c, cx, cy, w, h, false);
		cx += w + spacing.x;
		if (cx + w> innerRect.right)
		{
			cx = innerRect.left;
			cy += h + spacing.y;
		}
	}
}

RECT leftHalf(const RECT &in, float split)
{
	RECT res;
	res.left = in.left;
	res.right = rectXPos(in, split);
	res.top = in.top;
	res.bottom = in.bottom;
	return res;
}


RECT MakeRect(int left, int top, int right, int bottom)
{
	RECT res;
	res.left = left;
	res.top = top;
	res.right = right;
	res.bottom = bottom;
	return res;
}

RECT MakeRectWH(int left, int top, int w, int h)
{
	RECT res;
	res.left = left;
	res.top = top;
	res.right = left + w;
	res.bottom = top + h;
	return res;
}


void resizeChildren()
{
	RECT rect;
	GetWindowRect(appHwnd, &rect);
	int w = rectWidth(rect);
	int h = rectHeight(rect);
	int half = w >> 1;
	
	//auto res = SetWindowPos(

	RECT localRect = MakeRect(0, 0, w, h);
	RECT padding = MakeRect(4, 4, 4, 4);
	RECT innerRect = getInnerRect(localRect, padding);
	
	
	auto res = MoveWindow(listHwnd, innerRect.left, innerRect.top, half, rectHeight(innerRect)-20, false);
	if (!res)
	{
		DWORD err = GetLastError();
		OutputErrorMessage(GetLastError());
	}
	RECT bounds = innerRect;
	bounds.left = half + padding.left;
	wrapRowResize(bounds,padding, { 2,2 }, CoreToggleButtons);
	UpdateWindow(appHwnd);
}

void AddProcessToMonitor(const wchar_t *processName)
{
	auto itr = std::find_if(mMonitoredProcesses.begin(), mMonitoredProcesses.end(), [processName](auto p)
	{
		return wcscmp(p.szProcessName.c_str(), processName) == 0;
	});
	if (itr == mMonitoredProcesses.end())
	{
		mMonitoredProcesses.push_back(ProcessAffinityInfo(processName));

		if (currentlySelectedProcess == mMonitoredProcesses.size() - 1)
			mMonitoredProcesses[currentlySelectedProcess].PopulateUI();
	}
	populateList();
}

DWORD CountSetBits(ULONG_PTR bitMask)
{
	DWORD LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
	DWORD bitSetCount = 0;
	ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
	DWORD i;

	for (i = 0; i <= LSHIFT; ++i)
	{
		bitSetCount += ((bitMask & bitTest) ? 1 : 0);
		bitTest >>= 1;
	}

	return bitSetCount;
}

DWORD findProcessIDByName(const WCHAR *processName)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	HANDLE hProcess = 0;

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (wcscmp(entry.szExeFile, processName) == 0)
			{				
				return entry.th32ProcessID;
			}
		}
	}

	
	return 0;
}

HANDLE findProcessByName(const WCHAR *processName)
{
	DWORD id = findProcessIDByName(processName);
	if (id != 0)
	{
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, id);
		return hProcess;
	}
	else
		return 0;
}
/*	HANDLE hProcess = GetProcessId()
	SetProcessAffinityMask(
		_In_ HANDLE    hProcess,
		_In_ DWORD_PTR dwProcessAffinityMask
	);

}*/

void SetButtonToggleStatus(DWORD affinityMask)
{
	for (UINT32 i = 0; i < CoreToggleButtons.size(); ++i)
	{
		DWORD mask = (1 << i);
		bool coreOn = (affinityMask & mask) != 0;
		Button_SetCheck(CoreToggleButtons[i], coreOn);
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SETCORES, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SETCORES));

    MSG msg;

	
    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SETCORES));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_SETCORES);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterToggleButtonClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SETCORES));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SETCORES);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}


//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
#define IDC_BTN_SLEEPCLICK 34122
#define IDC_LISTBOX		   34120

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   appHwnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);


   // SetSecurityInfo
   //PROCESS_SET_INFORMATION 
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION processorInfoBuffer[128];
	DWORD bufsize = sizeof(processorInfoBuffer);
	GetLogicalProcessorInformation(processorInfoBuffer, &bufsize);
	UINT32 numaNodeCount = 0;
	UINT32 coreCount = 0;
	UINT32 logicalProcessorCount = 0;
	UINT32 entryCount = bufsize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

	for (UINT32 i= 0; i < entryCount;++i)
	{
		const auto &p = processorInfoBuffer[i];
		switch (p.Relationship)
		{
		case RelationNumaNode:
			// Non-NUMA systems report a single record of this type.
			numaNodeCount++;
			break;

		case RelationProcessorCore:
			coreCount++;

			// A hyperthreaded core supplies more than one logical processor.
			logicalProcessorCount += CountSetBits(p.ProcessorMask);
			break;
		}
	}

	enum { rowCount = 4, width = 32, height = 32, margin=4 };
	int posx = 5;
	int posy = 5;

	listHwnd = CreateWindowExW(WS_EX_CLIENTEDGE
		, L"LISTBOX", NULL
		, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL
		, 7, 35, 500, 200
		, appHwnd, (HMENU)IDC_LISTBOX, hInstance, NULL);

	for (DWORD i = 0; i < logicalProcessorCount; ++i)
	{
		wchar_t id[32];
		swprintf_s(id, 32, L"%i", i);
		CoreToggleButtons.push_back(CreateWindowEx(0, L"BUTTON", id, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, posx, posy, width, height, appHwnd, (HMENU)IDC_BTN_SLEEPCLICK + i, GetModuleHandle(NULL), NULL));
		
		posx += width + margin;
		if ((i% rowCount) == rowCount)
		{
			posx = 5;
			posy += height + margin;
		}
	}

	AddProcessToMonitor(L"3dsmax.exe");

	SetTimer(appHwnd,             // handle to main window 
		IDT_TIMER1,            // timer identifier 
		20000,                 // 10-second interval 
		(TIMERPROC)NULL);     // no timer callback 

   if (!appHwnd)
   {
      return FALSE;
   }
   populateMap();
   populateList();
   ShowWindow(appHwnd, nCmdShow);   
   UpdateWindow(appHwnd);
   resizeChildren();

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
INT_PTR CALLBACK AddProcessWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifdef DEBUG_OUTPUT_DLGCOMMANDS
	char infotext[512];
	const char *msgtext = GetMessageText(message);
	std::snprintf(infotext, 512, "HWND command: 0x%05x/%s, WPARAM: %i, LPARAM %i\n", message, msgtext, wParam, lParam);
	OutputDebugStringA(infotext);
#endif
	switch (message)
	{
	case WM_INITDIALOG:
	{
		
		HWND listHwnd = GetDlgItem(hDlg, IDC_PROCESS_LIST);
		if (listHwnd != 0)
		{
			populateProcessSelectionList(listHwnd);
		}
	}
		break;

	case WM_COMMAND:
	{
		HWND listHwnd = GetDlgItem(hDlg, IDC_PROCESS_LIST);
		switch (wParam)
		{
		case 1:
		{
			int lbItem = (int)SendMessage(listHwnd, LB_GETCURSEL, 0, 0);
			// Get item data.
			int i = (int)SendMessage(listHwnd, LB_GETITEMDATA, lbItem, 0);
			wchar_t Temp[256] = L"testing";
			int len = SendMessage(listHwnd, LB_GETTEXT, (WPARAM)(int)(lbItem), (LPARAM)(LPCTSTR)(Temp));
			if (len > 0)
			{
				wchar_t *sub_str = wcswcs(Temp, L"(");
				if (sub_str != 0)
				{
					size_t l = sub_str - Temp;
					Temp[l-2] = 0;
				}
				AddProcessToMonitor(Temp);
			}
			
			EndDialog(hDlg, 0);
		}
			break;
		case 2:
			EndDialog(hDlg, 0);
			break;
		}
	}
	default:
		return DefWindowProc(hDlg, message, wParam, lParam);
	}

}

void UpdateUIFromListboxSelection(int lbItem)
{
	char infotext[512];
	if (lbItem == -1)
		lbItem = (int)SendMessage(listHwnd, LB_GETCURSEL, 0, 0);
	if (lbItem != currentlySelectedProcess)
	{
		if (lbItem < mMonitoredProcesses.size())
		{
			currentlySelectedProcess = lbItem;
			mMonitoredProcesses[currentlySelectedProcess].PopulateUI();
			std::snprintf(infotext, 512, "Selected process index: %i\n", currentlySelectedProcess);
			OutputDebugStringA(infotext);
		}
	}	
}
#define WM_LBTRACKPOINT  0x131  //track item from point?
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	char infotext[512];
#ifdef DEBUG_OUTPUT_WINCOMMANDS
	
	const char *msgtext = GetMessageText(message);
	std::snprintf(infotext, 512, "HWND commannd: 0x%05x/%s, WPARAM: %i, LPARAM %i\n", message, msgtext , wParam, lParam);
	OutputDebugStringA(infotext);
#endif
    switch (message)
    {
	case WM_LBTRACKPOINT:
		UpdateUIFromListboxSelection(wParam);
		break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
			
			case ID_FILE_ADD_PROCESS:

				DialogBox(hInst, MAKEINTRESOURCE(IDD_ADD_PROCESS), hWnd, AddProcessWndProc);				
				break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
			case IDC_LISTBOX:
			{
				UINT32 param = LOWORD(wParam);
				OutputDebugStringA("Recieved listbox command\n");
				/*switch (param)
				{
					case LBN_SELCHANGE:*/
					{
					//UpdateUIFromListboxSelection(-1);
					}
				//}
				break;
			}
            default:
				UINT32 coreCount = CoreToggleButtons.size();
				int coreIdx= (int)wParam - IDC_BTN_SLEEPCLICK;
				coreIdx /= 4;
				
				if ((coreIdx >=0) && (coreIdx < coreCount))
				{
					std::snprintf(infotext, 512, "Core toggle: %i\n", coreIdx);
					OutputDebugStringA(infotext);
					auto &p = mMonitoredProcesses[currentlySelectedProcess];
					DWORD val1 = 0, val2 = 0;
					DWORD state = SendMessage((HWND)lParam, BM_GETSTATE, val1, val2);
					bool bstate = (state & BST_CHECKED) != 0;					
					p.SetCore(coreIdx, bstate);					
				}
				else
				{
#ifdef DEBUG_OUTPUT_UNHANDLEDCOMMANDS

					const char *msgtext = GetMessageText(message);
					std::snprintf(infotext, 512, "HWND commannd: 0x%05x/%s, WPARAM: %i, LPARAM %i\n", message, msgtext, wParam, lParam);
					OutputDebugStringA(infotext);
#endif

				}
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
	case WM_TIMER:
	{
		switch (wParam)
		{
			case IDT_TIMER1:
				for (UINT32 i = 0; i < mMonitoredProcesses.size(); ++i)
				{
					auto &p = mMonitoredProcesses[i];
					if (i == currentlySelectedProcess)
						p.PopulateUI();
					p.SetCores();
					if (i == currentlySelectedProcess)
						p.PopulateUI();
				}
		}
	}
	case WM_SETCURSOR:
	case WM_MOUSEMOVE:
	case WM_NCACTIVATE:
	case WM_GETICON:
	case WM_NCHITTEST:
		return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
	case WM_SIZE:
	{
		resizeChildren();
		break;
	}
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
#ifdef DEBUG_OUTPUT_UNHANDLEDCOMMANDS

		const char *msgtext = GetMessageText(message);
		std::snprintf(infotext, 512, "HWND commannd: 0x%05x/%s, WPARAM: %i, LPARAM %i\n", message, msgtext, wParam, lParam);
		OutputDebugStringA(infotext);
#endif
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
