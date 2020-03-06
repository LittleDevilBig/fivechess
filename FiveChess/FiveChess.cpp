//FiveChess.cpp: 定义应用程序的入口点。
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

//未开始、正在执行计算任务、已开始（没在执行计算任务）
enum GameStatus { NOT_STARTED, EXECUTING, STARTED };

// 全局变量: 
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
const int iWindowWidth = 660;                   // 窗口宽度
const int iWindowHeight = 700;                  // 窗口高度
WhichFirst whichFirst;                          // 记录谁先走
string board;                                   // 15*15的棋盘
GameStatus gameStatus;                          // 记录游戏是否已经开始
HWND hWndGlobal;                                // 全局窗口句柄
HANDLE lastThread = NULL;                       // 保存上一个线程句柄
ChessEngine::Position lastMove(-1, -1);         // 用来记录最近一步走的位置

Image* background;
Image* blackChess;
Image* whiteChess;

// 此代码模块中包含的函数的前向声明: 
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void DrawChess(int x, int y, Graphics& graphics, bool isWhite);                                 //画棋子
void DrawRectangle(int x, int y, Graphics& graphics);                                           //画环绕棋子的绿色矩形
void OnPaint(HDC hdc);                                                                          //主绘画函数
DWORD WINAPI CalcProc(LPVOID lpParam);                                                          //五子棋AI运算线程
bool ShowTipsIfNecessary();                                                                     //工具函数
bool HasChess(int x, int y);                                                                    //判断(x, y)位置上是否有棋子
void OnMouseLButtonDown(int screenX, int screenY);                                              //鼠标左键点击相应函数
void OnTakeBack();                                                                              //“悔棋”按钮响应函数
int GetLevel(HWND hDlg);                                                                        //根据“新游戏”窗口的radio按钮选项来获取AI的搜索层数
INT_PTR CALLBACK NewGameCallback(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);        //“新游戏”窗口的时间处理函数

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,_In_opt_ HINSTANCE hPrevInstance,_In_ LPWSTR lpCmdLine,_In_ int nCmdShow){
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	// 初始化GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// 初始化素材图片
	background = new Image(L"pictures/board.jpg");
	blackChess = new Image(L"pictures/black_chess.png");
	whiteChess = new Image(L"pictures/white_chess.png");

	// 初始化游戏状态
	gameStatus = NOT_STARTED;
	ChessEngine::beforeStart();

	// 初始化棋盘
	int i;
	for (i = 0; i < 225; i++)
		board.push_back('0');

	// 初始化全局字符串
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_FIVECHESS, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// 执行应用程序初始化: 
	if (!InitInstance(hInstance, nCmdShow)){
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FIVECHESS));
	MSG msg;

	// 主消息循环: 
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

//  函数: MyRegisterClass()
//  目的: 注册窗口类
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

//   函数: InitInstance(HINSTANCE, int)
//   目的: 保存实例句柄并创建主窗口
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow){
	hInst = hInstance; // 将实例句柄存储在全局变量中
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

// 在棋盘(x, y)的位置画绿色矩形
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

	// 画背景白色矩形
	SolidBrush brush(Color(255, 255, 255));
	graphics.FillRectangle(&brush, Rect(0, 0, 700, 700));

	// 画棋盘
	Rect destRect(20, 20, 600, 600);
	graphics.DrawImage(background, destRect);

	//画棋子
	int i, j;
	for (i = 0; i < 15; i++) {
		for (j = 0; j < 15; j++) {
			switch (board[i * 15 + j]) {
				case '0':
					break;
				case '1':
					//画黑棋子
					if(whichFirst == ME_FIRST)DrawChess(i, j, graphics, false);
					else DrawChess(i, j, graphics, true);
					break;
				case '2':
					//画白棋子
					if (whichFirst == ME_FIRST)DrawChess(i, j, graphics, true);
					else DrawChess(i, j, graphics, false);
					break;
			}
		}
	}

	//给最新着的棋字画一个绿色矩形
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

	//获取上一步棋下的位置
	lastMove = ChessEngine::getLastPosition();

	// 通知窗口重绘
	RedrawWindow(hWndGlobal, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

	int ret;
	int someoneWin = ChessEngine::isSomeOneWin();
	switch (someoneWin) {
	case -1:
		gameStatus = STARTED;
		break;
	case 0:
		ret = MessageBoxA(hWndGlobal, "你赢了,太强了", "提示", NULL);
		gameStatus = NOT_STARTED;
		break;
	case 1:
		ret = MessageBoxA(hWndGlobal, "电脑赢了,继续努力", "提示", NULL);
		gameStatus = NOT_STARTED;
		break;
	}
	return 0;
}

bool ShowTipsIfNecessary() {
	if (gameStatus == EXECUTING) {
		MessageBoxA(hWndGlobal, "电脑还在计算中，请稍候。", "提示", NULL);
		return false;
	}
	if (gameStatus == NOT_STARTED) {
		MessageBoxA(hWndGlobal, "游戏还未开始，请点击左上角“新游戏”开始新游戏。", "提示", NULL);
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

	//更新上一步位置
	lastMove = ChessEngine::Position(x, y);

	//画出所着棋子
	board[x * 15 + y] = '1';
	RedrawWindow(hWndGlobal, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

	//回收上一个线程
	if (lastThread != NULL) {
		WaitForSingleObject(lastThread, INFINITE);
		CloseHandle(lastThread);   // 关闭内核对象  
	}

	gameStatus = EXECUTING;

	//启动线程执行计算任务
	DWORD threadID;
	lastThread = CreateThread(NULL, 0, CalcProc, (LPVOID)(x | (y << 16)), 0, &threadID);
}

void OnTakeBack() {
	if (!ShowTipsIfNecessary())
		return;

	//悔棋
	board = ChessEngine::takeBack();
	//取消掉绿色框的绘制
	lastMove = ChessEngine::Position(-1, -1);
	//重绘
	RedrawWindow(hWndGlobal, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
}

//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//  目的:    处理主窗口的消息。
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	switch (message){
		case WM_COMMAND:{
			int wmId = LOWORD(wParam);
			// 分析菜单选择: 
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
			// 避免闪烁
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

// “新对局”框的消息处理程序。
INT_PTR CALLBACK NewGameCallback(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){
	UNREFERENCED_PARAMETER(lParam);
	switch (message){
		case WM_INITDIALOG:
			// 设置“难”为缺省值
			CheckRadioButton(hDlg, IDC_RADIO_HARD, IDC_RADIO_HARD, IDC_RADIO_HARD);
			return (INT_PTR)TRUE;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK){
				int level = GetLevel(hDlg);
				EndDialog(hDlg, LOWORD(wParam));
				switch (whichFirst) {
					case ME_FIRST:
						board = ChessEngine::reset(0);
						//清除上一局的绿色矩形
						lastMove = ChessEngine::Position(-1, -1);
						break;
					case COMPUTER_FIRST:
						board = ChessEngine::reset(1);
						//获取AI第一步走的位置
						lastMove = ChessEngine::getLastPosition();
						break;
				}

				ChessEngine::setLevel(level);

				//设置游戏状态为开始状态
				gameStatus = STARTED;

				//隐藏“上一步”和“下一步”按钮
				SetWindowPos(hWndGlobal, NULL, 0, 0, iWindowWidth, iWindowHeight, SWP_NOMOVE);

				// 通知窗口重绘
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