
#include "resource.h"
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")

#include <commctrl.h>
#pragma comment( lib, "comctl32.lib" )

//�궨��

//��Ļ��
#define	TO_CLIENT_SCREEN	20001
#define GET_CLIENT_SCREEN	20002



//���ṹ
#pragma pack(push)
#pragma pack(1)
struct MyWrap
{
	DWORD	dwType;			//��������
	DWORD	dwLength;		//�����ݵĳ���
	PVOID	lpBuffer;		//������
};
#pragma pack(pop)



//������ʾ
struct _Show
{
	HWND	hWnd;
	DWORD 	dwItem;
};


//�ͻ��˽ṹ
struct ClientData
{
	MyWrap		stWrap;			//�ͻ��˵����ݰ�
	SOCKET		hSocketClient;	//������ָ���ͻ��˴������ݵ��׽��־��
	HANDLE		hThread;		//�������հ����߳̾��
	_Show		stShow;			//��ʾ���������̲߳���

	HWND		hScreenWindow;	//��Ļ���ھ��
	HANDLE		hScreenThread;	//��Ļ��ʾ�߳̾��

	HWND		hCmdWindow;		//Cmd���ھ��
	HANDLE		hCmdThread;		//Cmd��ʾ�߳̾��

	ClientData* Next;			//ָ����һ��ClientData�ṹ
};


ClientData* pHandClient = NULL;		//�ͻ�����������ͷָ��




HWND hWinMain;					//�����ھ��
SOCKET hSocketWait;				//���ڵȴ��ͻ������ӵ��׽��־��		


HANDLE hmutex;					//����ȫ�ֻ��������������ͬ���յ����Կͻ��˵İ���
HANDLE hmutex1;					//����ȫ�ֻ��������������ͬ���յ����͵��ͻ��˵İ���


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



//�����ڴ��ڹ���
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
		case ID_40001:				//��ʾ����
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG2), NULL, _DialogScreen, lo.iItem);
			break;
		case ID_40003:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG3), NULL, _DialogCmd, lo.iItem);
		}
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case NM_RCLICK:			//����Ŀ���һ�
			switch (((LPNMITEMACTIVATE)lParam)->hdr.idFrom)
			{
			case IDC_LIST1:
				GetCursorPos(&stPos);					//��������
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
		hmutex = CreateMutex(NULL, false, NULL);//����������󲢷�������
		hmutex1 = CreateMutex(NULL, false, NULL);//����������󲢷�������
		hMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU1));
		_InitSocket();					//��ʼ������

		//�����߳�����ѭ�����տͻ�������
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_ReceiveClient, NULL, 0, NULL);


		break;

	default:
		return FALSE;
	}

	return TRUE;
}


//�����ʼ��
VOID _stdcall _InitSocket()
{
	//��ʼ��
	WORD	wVersionRequested;
	WSADATA	wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);


}



//���տͻ�������
BOOL _stdcall _ReceiveClient()
{
	char szIP[] = TEXT("IP��ַ");
	char szPort[] = TEXT("Port�˿�");
	LVCOLUMN lvColumn;			//������
	LVITEM lvItem;				//������
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




	//�����׽���
	hSocketWait = socket(
		AF_INET,
		SOCK_STREAM,
		0);




	//�󶨺ͼ����˿�
	sockaddr_in addr;				//��������ip�Ͷ˿ڵĽṹ��
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");
	addr.sin_port = htons(10087);	//�˶˿���������˭�����ӷ����	
	bind(hSocketWait, (sockaddr*)&addr, sizeof(sockaddr_in));

	listen(hSocketWait, 5);			//����


	//��������
	sockaddr_in remoteAddr;
	int nLen = sizeof(remoteAddr);

	ClientData* pClient = pHandClient;		//����ָ��
	_Show lpScreen;						//�̲߳���

	TCHAR szBuffer[256];

	int i;
	while (1)
	{

		memset(&remoteAddr, 0, sizeof(remoteAddr));
		remoteAddr.sin_family = AF_INET;
		SOCKET h = accept(hSocketWait, (sockaddr*)&remoteAddr, &nLen);				//�ȴ��ͻ�������



		i = 1;												//����һ���ͻ������ݽṹ
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
		pClient->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_ToClient, &lpScreen, 0, NULL);					//�����߳���ѭ���������ݰ�

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




//ѭ����ȡ���Կͻ��˵����ݰ�
BOOL _stdcall _ToClient(_Show * lpScreen)
{
	BYTE szBuffer[256];
	BYTE* lpBuf;
	int len;
	int len1;
	int flag = 0;				//���������߳�

	MyWrap	stSendWrap;			//�����
	ClientData* pClient		 = pHandClient;		//����ָ��
	ClientData* pClientFront			;		//����ָ��
	for (int i = 0;i < lpScreen->dwItem; i++)
	{
		pClient = pClient->Next;
	}



	while (1)
	{

		len = 0;
		WaitForSingleObject(hmutex, INFINITE);		//���󻥳����


		//�������Կͻ��˵�����(ѭ������)
		memset(szBuffer, 0, sizeof(szBuffer));
		while (1)
		{
			len = recv(pClient->hSocketClient, (char*)(szBuffer + len), 8 - len, 0);
			if (len <= 0)							//recv����
			{
				flag = 1;
				break;
			}
			if (len == 8)
				break;
		}
		if (flag == 1)		//���recv����,�Ͽ����ӽ����߳�
		{
			SendMessage(pClient->hScreenWindow, WM_CLOSE, 0, 0);								//�ر���Ļ��ʾ���ں��߳�
			SendMessage(pClient->hCmdWindow, WM_CLOSE, 0, 0);									//�ر�cmd����

																							//ɾ���ͻ�������������Ӧ�Ŀͻ���
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

											
			ListView_DeleteItem(GetDlgItem(hWinMain, IDC_LIST1), pClient->stShow.dwItem);		//ɾ����ӦUI�е�����
			break;																				//�˳��߳�
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

		ReleaseMutex(hmutex);						//�ͷŻ������
		ReleaseMutex(hmutex);						//�ͷŻ������
		WaitForSingleObject(hmutex, INFINITE);		//���󻥳����
		delete[] pClient->stWrap.lpBuffer;			//�ͷ��ڴ�



		switch (pClient->stWrap.dwType)
		{
		case TO_CLIENT_SCREEN:				//��Ļ��
			stSendWrap.dwType = GET_CLIENT_SCREEN;				//���������������
			stSendWrap.dwLength = 0;
			WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
			send(pClient->hSocketClient, (char*)&stSendWrap, 8, 0);
			ReleaseMutex(hmutex1);						//�ͷŻ������1
			break;
		}

	}
	return 0;
}




//��ʾ��Ļ����(���ڻص�)
BOOL CALLBACK _DialogScreen(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	ClientData* pClient = pHandClient;		//����ָ��
	
	switch (message)
	{
	case WM_CLOSE:										//�ر���ʾ��Ļ�߳�
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
		
		for (int i = 0;i < lParam; i++)		//�����Ӧ��Ļ������Ϣ
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




//cmd���ڣ����ڻص���
BOOL _stdcall _DialogCmd(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	ClientData* pClient = pHandClient;		//����ָ��
	MyWrap	stSendWrap;			//�����
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
					WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
					send(pClient->hSocketClient, (char*)&stSendWrap, 8,0);				//����ִ��cmd
					send(pClient->hSocketClient, szBuffer, stSendWrap.dwLength, 0);
					ReleaseMutex(hmutex1);						//�ͷŻ������1

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
		for (int i = 0;i < lParam; i++)		//�����Ӧcmd������Ϣ
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





//��ʾ����
BOOL _stdcall _ShowScreen(_Show* lpScreen)
{


	HDC		hDeskDc;			//�����DC���
	HDC		hMyDc;				//���ǵ�DC(��������DC)
	HBITMAP hMyBitMap;			//����λͼ(���������)
	DWORD		nWidth = 0;				//��������ؿ�
	DWORD		nHeight = 0;			//��������ظ�

	ClientData* pClient = pHandClient;		//����ָ��
	for (int i = 0;i < lpScreen->dwItem; i++)
	{
		pClient = pClient->Next;
	}

	MyWrap	stSendWrap;			//�����
	stSendWrap.dwType = GET_CLIENT_SCREEN;
	stSendWrap.dwLength = 0;
	WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
	int nnn = send(pClient->hSocketClient, (char*)&stSendWrap, 8, 0);
	ReleaseMutex(hmutex1);						//�ͷŻ������1

	hDeskDc = GetDC(lpScreen->hWnd);
	hMyDc = CreateCompatibleDC(hDeskDc);



	while (1)
	{
		
		/*if (pClient == NULL)
			break;*/
		if (pClient->stWrap.dwType == TO_CLIENT_SCREEN)			//��ʾ��Ļ��
		{


			WaitForSingleObject(hmutex, INFINITE);//���󻥳����

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
				SRCCOPY			//����ģʽ
			);
			DeleteObject(hMyBitMap);
			ReleaseMutex(hmutex);//�ͷŻ��������

		}

	}
	ReleaseDC(lpScreen->hWnd, hDeskDc);
	DeleteDC(hMyDc);



	return 0;
}



//��ʾcmd
BOOL _stdcall _ShowCmd(_Show* lpScreen)
{

	ClientData* pClient = pHandClient;		//����ָ��
	for (int i = 0;i < lpScreen->dwItem; i++)
	{
		pClient = pClient->Next;
	}

	while (1)
	{

		if (pClient->stWrap.dwType == 10001)			//��ʾcmd
		{
			WaitForSingleObject(hmutex, INFINITE);		//���󻥳����
			SendMessage(GetDlgItem(pClient->hCmdWindow, IDC_EDIT1), EM_SETSEL, -2, -1);					//���дcmd����
			SendMessage(GetDlgItem(pClient->hCmdWindow, IDC_EDIT1), EM_REPLACESEL, true, (DWORD)pClient->stWrap.lpBuffer);
			ReleaseMutex(hmutex);						//�ͷŻ��������
		}

	}

	return 0;
}


