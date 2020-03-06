//FiveChess.cpp: ����Ӧ�ó������ڵ㡣
#include "stdafx.h"
#include "FiveChess.h"
#include "ChessEngine.h"

#include <ObjIdl.h>
#include <Commdlg.h>
#include <gdiplus.h>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>

using namespace Gdiplus;
using namespace std;

#pragma comment(lib, "gdiplus.lib")

#define MAX_LOADSTRING    100
#define IDB_NEXT        0
#define IDB_PREVIOUS    1

enum WhichFirst { ME_FIRST, COMPUTER_FIRST };

//δ��ʼ������ִ�м��������ѿ�ʼ��û��ִ�м�������
enum GameStatus { NOT_STARTED, EXECUTING, STARTED };

// ȫ�ֱ���: 
HINSTANCE hInst;                                // ��ǰʵ��
WCHAR szTitle[MAX_LOADSTRING];                  // �������ı�
WCHAR szWindowClass[MAX_LOADSTRING];            // ����������
const int iWindowWidth = 660;                   // ���ڿ��
const int iWindowHeight = 700;                  // ���ڸ߶�
WhichFirst whichFirst;                          // ��¼˭����
string board;                                   // 15*15������
GameStatus gameStatus;                          // ��¼��Ϸ�Ƿ��Ѿ���ʼ
HWND hWndGlobal;                                // ȫ�ִ��ھ��
HANDLE lastThread = NULL;                       // ������һ���߳̾��
ChessEngine::Position lastMove(-1, -1);         // ������¼���һ���ߵ�λ��

Image* background;
Image* blackChess;
Image* whiteChess;

// �˴���ģ���а����ĺ�����ǰ������: 
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void DrawChess(int x, int y, Graphics& graphics, bool isWhite);                                 //������
void DrawRectangle(int x, int y, Graphics& graphics);                                           //���������ӵ���ɫ����
void OnPaint(HDC hdc);                                                                          //���滭����
DWORD WINAPI CalcProc(LPVOID lpParam);                                                          //������AI�����߳�
bool ShowTipsIfNecessary();                                                                     //���ߺ���
bool HasChess(int x, int y);                                                                    //�ж�(x, y)λ�����Ƿ�������
void OnMouseLButtonDown(int screenX, int screenY);                                              //�����������Ӧ����
void OnTakeBack();                                                                              //�����塱��ť��Ӧ����
int GetLevel(HWND hDlg);                                                                        //���ݡ�����Ϸ�����ڵ�radio��ťѡ������ȡAI����������
INT_PTR CALLBACK NewGameCallback(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);        //������Ϸ�����ڵ�ʱ�䴦����

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,_In_opt_ HINSTANCE hPrevInstance,_In_ LPWSTR lpCmdLine,_In_ int nCmdShow){
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	// ��ʼ��GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// ��ʼ���ز�ͼƬ
	background = new Image(L"pictures/board.jpg");
	blackChess = new Image(L"pictures/black_chess.png");
	whiteChess = new Image(L"pictures/white_chess.png");

	// ��ʼ����Ϸ״̬
	gameStatus = NOT_STARTED;
	ChessEngine::beforeStart();

	// ��ʼ������
	int i;
	for (i = 0; i < 225; i++)
		board.push_back('0');

	// ��ʼ��ȫ���ַ���
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_FIVECHESS, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// ִ��Ӧ�ó����ʼ��: 
	if (!InitInstance(hInstance, nCmdShow)){
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FIVECHESS));
	MSG msg;

	// ����Ϣѭ��: 
	while (GetMessage(&msg, nullptr, 0, 0)){
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	delete background;
	delete blackChess;
	delete whiteChess;
	GdiplusShutdown(gdiplusToken);
	return (int)msg.wParam;
}

//  ����: MyRegisterClass()
//  Ŀ��: ע�ᴰ����
ATOM MyRegisterClass(HINSTANCE hInstance){
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FIVECHESS));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_FIVECHESS);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//   ����: InitInstance(HINSTANCE, int)
//   Ŀ��: ����ʵ�����������������
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow){
	hInst = hInstance; // ��ʵ������洢��ȫ�ֱ�����
	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
		CW_USEDEFAULT, 0, iWindowWidth, iWindowHeight, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd){
		return FALSE;
	}

	hWndGlobal = hWnd;
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return TRUE;
}

void DrawChess(int x, int y, Graphics& graphics, bool isWhite) {
	float step = 39.1f;
	float start = 46 - step / 2;

	int rectTop = (int)(start + step * y);
	int rectLeft = (int)(start + step * x);
	Rect chessRect = Rect(rectLeft, rectTop, (int)step, (int)step);

	graphics.DrawImage(isWhite ? whiteChess : blackChess, chessRect);
}

// ������(x, y)��λ�û���ɫ����
void DrawRectangle(int x, int y, Graphics& graphics) {
	float step = 39.1f;
	float start = 46 - step / 2;

	int rectTop = (int)(start + step * y);
	int rectLeft = (int)(start + step * x);
	Rect chessRect = Rect(rectLeft, rectTop, (int)step, (int)step);

	Pen pen(Color(255, 0, 255, 0));
	graphics.DrawRectangle(&pen, chessRect);
}

void OnPaint(HDC hdc) {
	Bitmap bmp(700, 700);
	Graphics graphics(&bmp);

	// ��������ɫ����
	SolidBrush brush(Color(255, 255, 255));
	graphics.FillRectangle(&brush, Rect(0, 0, 700, 700));

	// ������
	Rect destRect(20, 20, 600, 600);
	graphics.DrawImage(background, destRect);

	//������
	int i, j;
	for (i = 0; i < 15; i++) {
		for (j = 0; j < 15; j++) {
			switch (board[i * 15 + j]) {
				case '0':
					break;
				case '1':
					//��������
					if(whichFirst == ME_FIRST)DrawChess(i, j, graphics, false);
					else DrawChess(i, j, graphics, true);
					break;
				case '2':
					//��������
					if (whichFirst == ME_FIRST)DrawChess(i, j, graphics, true);
					else DrawChess(i, j, graphics, false);
					break;
			}
		}
	}

	//�������ŵ����ֻ�һ����ɫ����
	if (gameStatus != NOT_STARTED && lastMove.x >= 0) {
		DrawRectangle(lastMove.x, lastMove.y, graphics);
	}

	Graphics realGraphics(hdc);
	realGraphics.DrawImage(&bmp, 0, 0);
}

DWORD WINAPI CalcProc(LPVOID lpParam){
	int x = LOWORD(lpParam);
	int y = HIWORD(lpParam);
	board = ChessEngine::nextStep(x, y);

	//��ȡ��һ�����µ�λ��
	lastMove = ChessEngine::getLastPosition();

	// ֪ͨ�����ػ�
	RedrawWindow(hWndGlobal, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

	int ret;
	int someoneWin = ChessEngine::isSomeOneWin();
	switch (someoneWin) {
	case -1:
		gameStatus = STARTED;
		break;
	case 0:
		ret = MessageBoxA(hWndGlobal, "��Ӯ��,̫ǿ��", "��ʾ", NULL);
		gameStatus = NOT_STARTED;
		break;
	case 1:
		ret = MessageBoxA(hWndGlobal, "����Ӯ��,����Ŭ��", "��ʾ", NULL);
		gameStatus = NOT_STARTED;
		break;
	}
	return 0;
}

bool ShowTipsIfNecessary() {
	if (gameStatus == EXECUTING) {
		MessageBoxA(hWndGlobal, "���Ի��ڼ����У����Ժ�", "��ʾ", NULL);
		return false;
	}
	if (gameStatus == NOT_STARTED) {
		MessageBoxA(hWndGlobal, "��Ϸ��δ��ʼ���������Ͻǡ�����Ϸ����ʼ����Ϸ��", "��ʾ", NULL);
		return false;
	}
	return true;
}

bool HasChess(int x, int y) {
	if (board[x * 15 + y] != '0')
		return true;
	return false;
}

void OnMouseLButtonDown(int screenX, int screenY) {
	float step = 39.1f;
	int left = (int)(46 - step / 2);
	int right = (int)(left + step * 15);

	if (screenX < left || screenX > right || screenY < left || screenY > right)
		return;

	if (!ShowTipsIfNecessary())
		return;

	int x = (int)((screenX - left) / step);
	int y = (int)((screenY - left) / step);

	if (x > 14) x = 14;
	if (y > 14) y = 14;

	if (HasChess(x, y))
		return;

	//������һ��λ��
	lastMove = ChessEngine::Position(x, y);

	//������������
	board[x * 15 + y] = '1';
	RedrawWindow(hWndGlobal, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

	//������һ���߳�
	if (lastThread != NULL) {
		WaitForSingleObject(lastThread, INFINITE);
		CloseHandle(lastThread);   // �ر��ں˶���  
	}

	gameStatus = EXECUTING;

	//�����߳�ִ�м�������
	DWORD threadID;
	lastThread = CreateThread(NULL, 0, CalcProc, (LPVOID)(x | (y << 16)), 0, &threadID);
}

void OnTakeBack() {
	if (!ShowTipsIfNecessary())
		return;

	//����
	board = ChessEngine::takeBack();
	//ȡ������ɫ��Ļ���
	lastMove = ChessEngine::Position(-1, -1);
	//�ػ�
	RedrawWindow(hWndGlobal, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
}

//  ����: WndProc(HWND, UINT, WPARAM, LPARAM)
//  Ŀ��:    ���������ڵ���Ϣ��
//  WM_COMMAND  - ����Ӧ�ó���˵�
//  WM_PAINT    - ����������
//  WM_DESTROY  - �����˳���Ϣ������
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	switch (message){
		case WM_COMMAND:{
			int wmId = LOWORD(wParam);
			// �����˵�ѡ��: 
			switch (wmId){
				case IDM_ME_FIRST:
					whichFirst = ME_FIRST;
					DialogBox(hInst, MAKEINTRESOURCE(IDD_NEW_GAME), hWnd, NewGameCallback);
					break;
				case IDM_COMPUTER_FIRST:
					whichFirst = COMPUTER_FIRST;
					DialogBox(hInst, MAKEINTRESOURCE(IDD_NEW_GAME), hWnd, NewGameCallback);
					break;
				case IDM_TAKE_BACK:
					if (!ShowTipsIfNecessary())
						return 0;
					OnTakeBack();
					break;
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
			break;
		case WM_PAINT:{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			OnPaint(hdc);
			EndPaint(hWnd, &ps);
		}
			break;
		case WM_ERASEBKGND:
			// ������˸
			return TRUE;
		case WM_LBUTTONDOWN:
			OnMouseLButtonDown(LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

int GetLevel(HWND hDlg) {
	int level = 0;
	if (IsDlgButtonChecked(hDlg, IDC_RADIO_EASY))
		level = 4;
	else if (IsDlgButtonChecked(hDlg, IDC_RADIO_MIDDLE))
		level = 5;
	else if (IsDlgButtonChecked(hDlg, IDC_RADIO_HARD))
		level = 8;
	return level;
}

// ���¶Ծ֡������Ϣ�������
INT_PTR CALLBACK NewGameCallback(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){
	UNREFERENCED_PARAMETER(lParam);
	switch (message){
		case WM_INITDIALOG:
			// ���á��ѡ�Ϊȱʡֵ
			CheckRadioButton(hDlg, IDC_RADIO_HARD, IDC_RADIO_HARD, IDC_RADIO_HARD);
			return (INT_PTR)TRUE;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK){
				int level = GetLevel(hDlg);
				EndDialog(hDlg, LOWORD(wParam));
				switch (whichFirst) {
					case ME_FIRST:
						board = ChessEngine::reset(0);
						//�����һ�ֵ���ɫ����
						lastMove = ChessEngine::Position(-1, -1);
						break;
					case COMPUTER_FIRST:
						board = ChessEngine::reset(1);
						//��ȡAI��һ���ߵ�λ��
						lastMove = ChessEngine::getLastPosition();
						break;
				}

				ChessEngine::setLevel(level);

				//������Ϸ״̬Ϊ��ʼ״̬
				gameStatus = STARTED;

				//���ء���һ�����͡���һ������ť
				SetWindowPos(hWndGlobal, NULL, 0, 0, iWindowWidth, iWindowHeight, SWP_NOMOVE);

				// ֪ͨ�����ػ�
				RedrawWindow(hWndGlobal, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
			}
			else if (LOWORD(wParam) == IDCANCEL) {
				EndDialog(hDlg, LOWORD(wParam));
			}
		return (INT_PTR)TRUE;
		break;
	}
	return (INT_PTR)FALSE;
}