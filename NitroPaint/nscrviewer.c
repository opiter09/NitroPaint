#include "nscrviewer.h"
#include "ncgrviewer.h"
#include "nclrviewer.h"
#include "childwindow.h"
#include "resource.h"
#include "nitropaint.h"
#include "nscr.h"
#include "gdip.h"
#include "palette.h"

extern HICON g_appIcon;

#define NV_INITIALIZE (WM_USER+1)
#define NV_SETDATA (WM_USER+2)
#define NV_INITIMPORTDIALOG (WM_USER+3)

DWORD * renderNscrBits(NSCR * renderNscr, NCGR * renderNcgr, NCLR * renderNclr, BOOL drawGrid, BOOL checker, int * width, int * height, int tileMarks, int highlightTile) {
	int bWidth = renderNscr->nWidth;
	int bHeight = renderNscr->nHeight;
	if (drawGrid) {
		bWidth = bWidth * 9 / 8 + 1;
		bHeight = bHeight * 9 / 8 + 1;
	}
	*width = bWidth;
	*height = bHeight;

	LPDWORD bits = (LPDWORD) calloc(bWidth * bHeight, 4);

	int tilesX = renderNscr->nWidth >> 3;
	int tilesY = renderNscr->nHeight >> 3;

	DWORD block[64];

	for (int y = 0; y < tilesY; y++) {
		int offsetY = y << 3;
		if (drawGrid) offsetY = y * 9 + 1;
		for (int x = 0; x < tilesX; x++) {
			int offsetX = x << 3;
			if (drawGrid) offsetX = x * 9 + 1;

			int tileNo = -1;
			nscrGetTileEx(renderNscr, renderNcgr, renderNclr, x, y, checker, block, &tileNo);
			DWORD dwDest = x * 8 + y * 8 * bWidth;

			if (tileMarks != -1 && tileMarks == tileNo) {
				for (int i = 0; i < 64; i++) {
					DWORD d = block[i];
					int b = d & 0xFF;
					int g = (d >> 8) & 0xFF;
					int r = (d >> 16) & 0xFF;
					r = r >> 1;
					g = (g + 255) >> 1;
					b = (b + 255) >> 1;
					block[i] = (d & 0xFF000000) | b | (g << 8) | (r << 16);
				}
			}

			if (highlightTile != -1 && (x + y * tilesX) == highlightTile) {
				for (int i = 0; i < 64; i++) {
					DWORD d = block[i];
					int b = d & 0xFF;
					int g = (d >> 8) & 0xFF;
					int r = (d >> 16) & 0xFF;
					r = (r + 255) >> 1;
					g = (g + 255) >> 1;
					b = (b + 255) >> 1;
					block[i] = (d & 0xFF000000) | b | (g << 8) | (r << 16);
				}
			}

			for (int i = 0; i < 8; i++) {
				CopyMemory(bits + dwDest + i * bWidth, block + (i << 3), 32);
			}
		}
	}

	/*for (int i = 0; i < bWidth * bHeight; i++) {
		DWORD d = bits[i];
		int r = d & 0xFF;
		int g = (d >> 8) & 0xFF;
		int b = (d >> 16) & 0xFF;
		int a = (d >> 24) & 0xFF;
		bits[i] = b | (g << 8) | (r << 16) | (a << 24);
	}*/
	return bits;
}

HBITMAP renderNscr(NSCR * renderNscr, NCGR * renderNcgr, NCLR * renderNclr, BOOL drawGrid, int * width, int * height, int highlightNclr, int highlightTile) {
	if (renderNcgr != NULL) {
		if (renderNscr->nHighestIndex >= renderNcgr->nTiles && highlightNclr != -1) highlightNclr -= (renderNcgr->nTiles - renderNscr->nHighestIndex - 1);
		DWORD * bits = renderNscrBits(renderNscr, renderNcgr, renderNclr, drawGrid, TRUE, width, height, highlightNclr, highlightTile);

		HBITMAP hBitmap = CreateBitmap(*width, *height, 1, 32, bits);
		free(bits);
		return hBitmap;
	}
	return NULL;
}

LRESULT WINAPI NscrViewerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	NSCRVIEWERDATA *data = (NSCRVIEWERDATA *) GetWindowLongPtr(hWnd, 0);
	if (!data) {
		data = (NSCRVIEWERDATA *) calloc(1, sizeof(NSCRVIEWERDATA));
		SetWindowLongPtr(hWnd, 0, (LONG) data);
	}
	switch (msg) {
		case WM_CREATE:
		{
			data->showBorders = 0;
			data->scale = 1;
			break;
		}
		case NV_INITIALIZE:
		{
			LPWSTR path = (LPWSTR) wParam;
			memcpy(data->szOpenFile, path, 2 * (wcslen(path) + 1));
			memcpy(&data->nscr, (NSCR *) lParam, sizeof(NSCR));
			WCHAR titleBuffer[MAX_PATH + 15];
			memcpy(titleBuffer, path, wcslen(path) * 2 + 2);
			memcpy(titleBuffer + wcslen(titleBuffer), L" - NSCR Viewer", 30);
			SetWindowText(hWnd, titleBuffer);
			data->frameData.contentWidth = getDimension(data->nscr.nWidth / 8, data->showBorders, data->scale);
			data->frameData.contentHeight = getDimension(data->nscr.nHeight / 8, data->showBorders, data->scale);

			RECT rc = { 0 };
			rc.right = data->frameData.contentWidth;
			rc.bottom = data->frameData.contentHeight;
			AdjustWindowRect(&rc, WS_CAPTION | WS_THICKFRAME | WS_SYSMENU, FALSE);
			int width = rc.right - rc.left + 4; //+4 to account for WS_EX_CLIENTEDGE
			int height = rc.bottom - rc.top + 4;
			SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE);


			RECT rcClient;
			GetClientRect(hWnd, &rcClient);

			SCROLLINFO info;
			info.cbSize = sizeof(info);
			info.nMin = 0;
			info.nMax = data->frameData.contentWidth;
			info.nPos = 0;
			info.nPage = rcClient.right - rcClient.left + 1;
			info.nTrackPos = 0;
			info.fMask = SIF_POS | SIF_RANGE | SIF_POS | SIF_TRACKPOS | SIF_PAGE;
			SetScrollInfo(hWnd, SB_HORZ, &info, TRUE);

			info.nMax = data->frameData.contentHeight;
			info.nPage = rcClient.bottom - rcClient.top + 1;
			SetScrollInfo(hWnd, SB_VERT, &info, TRUE);
			return 1;
		}
		case WM_COMMAND:
		{
			if (HIWORD(wParam) == 0 && lParam == 0) {
				switch (LOWORD(wParam)) {
					case ID_FILE_EXPORT:
					{
						LPWSTR location = saveFileDialog(hWnd, L"Save Bitmap", L"PNG Files (*.png)\0*.png\0All Files\0*.*\0", L"png");
						if (!location) break;
						int width, height;

						HWND hWndMain = (HWND) GetWindowLong((HWND) GetWindowLong(hWnd, GWL_HWNDPARENT), GWL_HWNDPARENT);
						NITROPAINTSTRUCT *nitroPaintStruct = (NITROPAINTSTRUCT *) GetWindowLongPtr(hWndMain, 0);
						HWND hWndNclrViewer = nitroPaintStruct->hWndNclrViewer;
						HWND hWndNcgrViewer = nitroPaintStruct->hWndNcgrViewer;
						HWND hWndNscrViewer = hWnd;

						NCGR *ncgr = NULL;
						NCLR *nclr = NULL;
						NSCR *nscr = NULL;

						if (hWndNclrViewer) {
							NCLRVIEWERDATA *nclrViewerData = (NCLRVIEWERDATA *) GetWindowLongPtr(hWndNclrViewer, 0);
							nclr = &nclrViewerData->nclr;
						}
						if (hWndNcgrViewer) {
							NCGRVIEWERDATA *ncgrViewerData = (NCGRVIEWERDATA *) GetWindowLongPtr(hWndNcgrViewer, 0);
							ncgr = &ncgrViewerData->ncgr;
						}
						nscr = &data->nscr;

						DWORD * bits = renderNscrBits(nscr, ncgr, nclr, FALSE, FALSE, &width, &height, -1, -1);
						
						writeImage(bits, width, height, location);
						free(bits);
						free(location);
						break;
					}
					case ID_NSCRMENU_FLIPHORIZONTALLY:
					{
						int tileNo = data->contextHoverX + data->contextHoverY * (data->nscr.nWidth >> 3);
						WORD oldVal = data->nscr.data[tileNo];
						oldVal ^= (TILE_FLIPX << 10);
						data->nscr.data[tileNo] = oldVal;
						InvalidateRect(hWnd, NULL, FALSE);
						break;
					}
					case ID_NSCRMENU_FLIPVERTICALLY:
					{
						int tileNo = data->contextHoverX + data->contextHoverY * (data->nscr.nWidth >> 3);
						WORD oldVal = data->nscr.data[tileNo];
						oldVal ^= (TILE_FLIPY << 10);
						data->nscr.data[tileNo] = oldVal;
						InvalidateRect(hWnd, NULL, FALSE);
						break;
					}
					case ID_FILE_SAVE:
					{
						nscrWrite(&data->nscr, data->szOpenFile);
						break;
					}
					case ID_NSCRMENU_IMPORTBITMAPHERE:
					{
						HWND hWndMain = (HWND) GetWindowLong((HWND) GetWindowLong(hWnd, GWL_HWNDPARENT), GWL_HWNDPARENT);
						HWND h = CreateWindow(L"NscrBitmapImportClass", L"Import Bitmap", WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME), CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, hWndMain, NULL, NULL, NULL);
						WORD d = data->nscr.data[data->contextHoverX + data->contextHoverY * (data->nscr.nWidth >> 3)];
						SendMessage(h, NV_INITIMPORTDIALOG, d, data->contextHoverX | (data->contextHoverY << 16));
						ShowWindow(h, SW_SHOW);
						break;
					}
				}
			}
			break;
		}
		case WM_MOUSEMOVE:
		case WM_NCMOUSEMOVE:
		{
			TRACKMOUSEEVENT evt;
			evt.cbSize = sizeof(evt);
			evt.hwndTrack = hWnd;
			evt.dwHoverTime = 0;
			evt.dwFlags = TME_LEAVE;
			TrackMouseEvent(&evt);
		}
		case WM_MOUSELEAVE:
		{
			POINT mousePos;
			GetCursorPos(&mousePos);
			ScreenToClient(hWnd, &mousePos);

			SCROLLINFO horiz, vert;
			horiz.cbSize = sizeof(horiz);
			vert.cbSize = sizeof(vert);
			horiz.fMask = SIF_ALL;
			vert.fMask = SIF_ALL;
			GetScrollInfo(hWnd, SB_HORZ, &horiz);
			GetScrollInfo(hWnd, SB_VERT, &vert);
			mousePos.x += horiz.nPos;
			mousePos.y += vert.nPos;
			
			if (mousePos.x >= 0 && mousePos.y >= 0 && mousePos.x < data->nscr.nWidth && mousePos.y < data->nscr.nHeight) {
				int x = mousePos.x / 8;
				int y = mousePos.y / 8;
				if (x != data->hoverX || y != data->hoverY) {
					data->hoverX = x;
					data->hoverY = y;
					InvalidateRect(hWnd, NULL, FALSE);
				}
			} else {
				int x = -1, y = -1;
				if (x != data->hoverX || y != data->hoverY) {
					data->hoverX = -1;
					data->hoverY = -1;
					InvalidateRect(hWnd, NULL, FALSE);
				}
			}

			InvalidateRect(hWnd, NULL, FALSE);
			break;
		}
		case WM_LBUTTONDOWN:
		case WM_RBUTTONUP:
		{
			int hoverY = data->hoverY;
			int hoverX = data->hoverX;
			POINT mousePos;
			GetCursorPos(&mousePos);
			ScreenToClient(hWnd, &mousePos);
			//transform it by scroll position
			SCROLLINFO horiz, vert;
			horiz.cbSize = sizeof(horiz);
			vert.cbSize = sizeof(vert);
			horiz.fMask = SIF_ALL;
			vert.fMask = SIF_ALL;
			GetScrollInfo(hWnd, SB_HORZ, &horiz);
			GetScrollInfo(hWnd, SB_VERT, &vert);

			mousePos.x += horiz.nPos;
			mousePos.y += vert.nPos;
			if (msg == WM_RBUTTONUP) {
			//if it is within the colors area, open a color chooser
				if (mousePos.x >= 0 && mousePos.y >= 0 && mousePos.x < data->nscr.nWidth && mousePos.y < data->nscr.nHeight) {
					HMENU hPopup = GetSubMenu(LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU2)), 2);
					POINT mouse;
					GetCursorPos(&mouse);
					TrackPopupMenu(hPopup, TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, mouse.x, mouse.y, 0, hWnd, NULL);
					data->contextHoverY = hoverY;
					data->contextHoverX = hoverX;
				}
			} else {
				if (data->hWndTileEditor != NULL) DestroyWindow(data->hWndTileEditor);
				data->editingX = data->hoverX;
				data->editingY = data->hoverY;
				data->hWndTileEditor = CreateWindowEx(WS_EX_MDICHILD | WS_EX_CLIENTEDGE, L"NscrTileEditorClass", L"Tile Editor", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | WS_CAPTION | WS_CLIPCHILDREN,
													CW_USEDEFAULT, CW_USEDEFAULT, 200, 150, (HWND) GetWindowLong(hWnd, GWL_HWNDPARENT), NULL, NULL, NULL);
				data->hoverX = -1;
				data->hoverY = -1;
			}
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hWindowDC = BeginPaint(hWnd, &ps);

			SCROLLINFO horiz, vert;
			horiz.cbSize = sizeof(horiz);
			vert.cbSize = sizeof(vert);
			horiz.fMask = SIF_ALL;
			vert.fMask = SIF_ALL;
			GetScrollInfo(hWnd, SB_HORZ, &horiz);
			GetScrollInfo(hWnd, SB_VERT, &vert);

			NSCR *nscr = NULL;
			NCGR *ncgr = NULL;
			NCLR *nclr = NULL;

			HWND hWndMain = (HWND) GetWindowLong((HWND) GetWindowLong(hWnd, GWL_HWNDPARENT), GWL_HWNDPARENT);
			NITROPAINTSTRUCT *nitroPaintStruct = (NITROPAINTSTRUCT *) GetWindowLongPtr(hWndMain, 0);
			HWND hWndNclrViewer = nitroPaintStruct->hWndNclrViewer;
			if (hWndNclrViewer) {
				NCLRVIEWERDATA *nclrViewerData = (NCLRVIEWERDATA *) GetWindowLongPtr(hWndNclrViewer, 0);
				nclr = &nclrViewerData->nclr;
			}
			HWND hWndNcgrViewer = nitroPaintStruct->hWndNcgrViewer;
			int hoveredNcgrTile = -1, hoveredNscrTile = -1;
			if (data->hoverX != -1 && data->hoverY != -1) {
				hoveredNscrTile = data->hoverX + data->hoverY * (data->nscr.nWidth / 8);
			}
			if (hWndNcgrViewer) {
				NCGRVIEWERDATA *ncgrViewerData = (NCGRVIEWERDATA *) GetWindowLongPtr(hWndNcgrViewer, 0);
				ncgr = &ncgrViewerData->ncgr;
				hoveredNcgrTile = ncgrViewerData->hoverIndex;
			}
			
			nscr = &data->nscr;
			int bitmapWidth, bitmapHeight;
			
			HBITMAP hBitmap = renderNscr(nscr, ncgr, nclr, FALSE, &bitmapWidth, &bitmapHeight, hoveredNcgrTile, hoveredNscrTile);

			HDC hDC = CreateCompatibleDC(hWindowDC);
			SelectObject(hDC, hBitmap);
			BitBlt(hWindowDC, -horiz.nPos, -vert.nPos, bitmapWidth, bitmapHeight, hDC, 0, 0, SRCCOPY);
			DeleteObject(hDC);
			DeleteObject(hBitmap);

			EndPaint(hWnd, &ps);
			break;
		}
		case WM_DESTROY:
		{
			if (data->hWndTileEditor) DestroyWindow(data->hWndTileEditor);
			free(data->nscr.data);
			free(data);
			HWND hWndMain = (HWND) GetWindowLong((HWND) GetWindowLong(hWnd, GWL_HWNDPARENT), GWL_HWNDPARENT);
			NITROPAINTSTRUCT *nitroPaintStruct = (NITROPAINTSTRUCT *) GetWindowLongPtr(hWndMain, 0);
			nitroPaintStruct->hWndNscrViewer = NULL;
			break;
		}
		case NV_SETDATA:
		{
			int tile = data->editingX + data->editingY * (data->nscr.nWidth >> 3);
			int character = wParam & 0x3FF;
			int palette = lParam & 0xF; //0x0C00
			data->nscr.data[tile] = (data->nscr.data[tile] & 0xC00) | character | (palette << 12);
			InvalidateRect(hWnd, NULL, FALSE);

			break;
		}
	}
	return DefChildProc(hWnd, msg, wParam, lParam);
}


typedef struct {
	HWND hWndCharacterInput;
	HWND hWndPaletteInput;
	HWND hWndOk;
} NSCRTILEEDITORDATA;

LRESULT WINAPI NscrTileEditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	NSCRTILEEDITORDATA *data = (NSCRTILEEDITORDATA *) GetWindowLongPtr(hWnd, 0);
	if (!data) {
		data = (NSCRTILEEDITORDATA *) calloc(1, sizeof(NSCRTILEEDITORDATA));
		SetWindowLongPtr(hWnd, 0, (LONG_PTR) data);
	}
	switch (msg) {
		case WM_CREATE:
		{
			RECT rc = { 0 };
			rc.right = 200;
			rc.bottom = 96;
			AdjustWindowRect(&rc, WS_CAPTION | WS_THICKFRAME | WS_SYSMENU, FALSE);
			int width = rc.right - rc.left + 4; //+4 to account for WS_EX_CLIENTEDGE
			int height = rc.bottom - rc.top + 4;
			SetWindowPos(hWnd, hWnd, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);

			HWND hWndMain = (HWND) GetWindowLong((HWND) GetWindowLong(hWnd, GWL_HWNDPARENT), GWL_HWNDPARENT);
			NITROPAINTSTRUCT *nitroPaintStruct = (NITROPAINTSTRUCT *) GetWindowLongPtr(hWndMain, 0);
			HWND hWndNscrViewer = nitroPaintStruct->hWndNscrViewer;
			NSCRVIEWERDATA *nscrViewerData = (NSCRVIEWERDATA *) GetWindowLongPtr(hWndNscrViewer, 0);
			int editing = nscrViewerData->editingX + nscrViewerData->editingY * (nscrViewerData->nscr.nWidth >> 3);
			WORD cell = nscrViewerData->nscr.data[editing];

			/*
				Character: [______]
				Palette:   [_____v]
			*/
			CreateWindow(L"STATIC", L"Character:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 10, 10, 70, 22, hWnd, NULL, NULL, NULL);
			data->hWndCharacterInput = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER, 90, 10, 100, 22, hWnd, NULL, NULL, NULL);
			CreateWindow(L"STATIC", L"Palette:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 10, 37, 70, 22, hWnd, NULL, NULL, NULL);
			data->hWndPaletteInput = CreateWindow(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_HASSTRINGS | CBS_DROPDOWNLIST | WS_VSCROLL, 90, 37, 100, 300, hWnd, NULL, NULL, NULL);
			WCHAR bf[16];
			for (int i = 0; i < 16; i++) {
				wsprintf(bf, L"Palette %02d", i);
				SendMessage(data->hWndPaletteInput, CB_ADDSTRING, (WPARAM) wcslen(bf), (LPARAM) bf);
			}
			SendMessage(data->hWndPaletteInput, CB_SETCURSEL, (cell >> 12) & 0xF, 0);
			wsprintf(bf, L"%d", cell & 0x3FF);
			SendMessage(data->hWndCharacterInput, WM_SETTEXT, (WPARAM) wcslen(bf), (LPARAM) bf);
			data->hWndOk = CreateWindow(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 90, 64, 100, 22, hWnd, NULL, NULL, NULL);
			EnumChildWindows(hWnd, SetFontProc, GetStockObject(DEFAULT_GUI_FONT));
			break;
		}
		case WM_NCHITTEST:
		{
			int ht = DefMDIChildProc(hWnd, msg, wParam, lParam);
			if (ht == HTTOP || ht == HTBOTTOM || ht == HTLEFT || ht == HTRIGHT || ht == HTTOPLEFT || ht == HTTOPRIGHT || ht == HTBOTTOMLEFT || ht == HTBOTTOMRIGHT) return HTCAPTION;
			return ht;
		}
		case WM_DESTROY:
		{
			HWND hWndMain = (HWND) GetWindowLong((HWND) GetWindowLong(hWnd, GWL_HWNDPARENT), GWL_HWNDPARENT);
			NITROPAINTSTRUCT *nitroPaintStruct = (NITROPAINTSTRUCT *) GetWindowLongPtr(hWndMain, 0);
			HWND hWndNscrViewer = nitroPaintStruct->hWndNscrViewer;
			NSCRVIEWERDATA *nscrViewerData = (NSCRVIEWERDATA *) GetWindowLongPtr(hWndNscrViewer, 0);
			nscrViewerData->hWndTileEditor = NULL;
			free(data);
			break;
		}
		case WM_COMMAND:
		{
			if (lParam) {
				HWND hWndControl = (HWND) lParam;
				if (hWndControl == data->hWndOk) {
					WCHAR bf[16];
					SendMessage(data->hWndCharacterInput, WM_GETTEXT, 15, (LPARAM) bf);
					int character = _wtoi(bf);
					int palette = SendMessage(data->hWndPaletteInput, CB_GETCURSEL, 0, 0);
					HWND hWndMain = (HWND) GetWindowLong((HWND) GetWindowLong(hWnd, GWL_HWNDPARENT), GWL_HWNDPARENT);
					NITROPAINTSTRUCT *nitroPaintStruct = (NITROPAINTSTRUCT *) GetWindowLongPtr(hWndMain, 0);
					HWND hWndNscrViewer = nitroPaintStruct->hWndNscrViewer;
					SendMessage(hWndNscrViewer, NV_SETDATA, (WPARAM) character, (LPARAM) palette);
					DestroyWindow(hWnd);
					SetFocus(hWndNscrViewer);
				}
			}
			break;
		}
	}
	return DefMDIChildProc(hWnd, msg, wParam, lParam);
}

void createMultiPalettes(DWORD *px, int tilesX, int tilesY, int width, DWORD *pals, int nPalettes, int paletteSize, int *useCounts, int *closests) {
	//compute the palettes from how the tiles are divided.
	DWORD **groups = (DWORD **) calloc(nPalettes, sizeof(DWORD *));
	for (int i = 0; i < nPalettes; i++) {
		groups[i] = (DWORD *) calloc(useCounts[i] * 64, 4);
	}
	int written[16] = { 0 };
	for (int y = 0; y < tilesY; y++) {
		for (int x = 0; x < tilesX; x++) {
			int srcOffset = x * 8 + y * 8 * (width);
			int uses = closests[x + y * tilesX];
			DWORD *block = groups[uses] + written[uses] * 64;
			CopyMemory(block, px + srcOffset, 32);
			CopyMemory(block + 8, px + srcOffset + width, 32);
			CopyMemory(block + 16, px + srcOffset + width * 2, 32);
			CopyMemory(block + 24, px + srcOffset + width * 3, 32);
			CopyMemory(block + 32, px + srcOffset + width * 4, 32);
			CopyMemory(block + 40, px + srcOffset + width * 5, 32);
			CopyMemory(block + 48, px + srcOffset + width * 6, 32);
			CopyMemory(block + 56, px + srcOffset + width * 7, 32);
			written[uses]++;
		}
	}

	for (int i = 0; i < nPalettes; i++) {
		createPalette_(groups[i], 8, written[i] * 8, pals + i * paletteSize, paletteSize);
	}
	free(groups);
}

int computeMultiPaletteError(int *closests, DWORD *blocks, int tilesX, int tilesY, int width, DWORD *pals, int nPalettes, int paletteSize) {
	int error = 0;
	for (int i = 0; i < tilesX * tilesY; i++) {
		int x = i % tilesX;
		int y = i / tilesX;
		DWORD *block = blocks + 64 * (x + y * tilesX);
		error += getPaletteError(block, 64, pals + closests[i] * paletteSize, paletteSize);
	}
	return error;
}

typedef struct {
	HWND hWndBitmapName;
	HWND hWndBrowseButton;
	HWND hWndPaletteInput;
	HWND hWndPalettesInput;
	HWND hWndImportButton;
	HWND hWndDitherCheckbox;

	int nscrTileX;
	int nscrTileY;
	int characterOrigin;
} NSCRBITMAPIMPORTDATA;

LRESULT WINAPI NscrBitmapImportWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	NSCRBITMAPIMPORTDATA *data = GetWindowLongPtr(hWnd, 0);
	if (data == NULL) {
		data = calloc(1, sizeof(NSCRBITMAPIMPORTDATA));
		SetWindowLongPtr(hWnd, 0, (LONG_PTR) data);
	}
	switch (msg) {
		case WM_CREATE:
		{
			HWND hWndParent = (HWND) GetWindowLong(hWnd, GWL_HWNDPARENT);
			SetWindowLong(hWndParent, GWL_STYLE, GetWindowLong(hWndParent, GWL_STYLE) | WS_DISABLED);
			/*

			Bitmap:   [__________] [...]
			Palette:  [_____]
			Palettes: [_____]
			          [Import]
			
			*/

			CreateWindow(L"STATIC", L"Bitmap:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 10, 10, 100, 22, hWnd, NULL, NULL, NULL);
			CreateWindow(L"STATIC", L"Palette:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 10, 37, 100, 22, hWnd, NULL, NULL, NULL);
			CreateWindow(L"STATIC", L"Palettes:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 10, 64, 100, 22, hWnd, NULL, NULL, NULL);
			CreateWindow(L"STATIC", L"Dither:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 10, 91, 100, 22, hWnd, NULL, NULL, NULL);

			data->hWndBitmapName = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 120, 10, 200, 22, hWnd, NULL, NULL, NULL);
			data->hWndBrowseButton = CreateWindow(L"BUTTON", L"...", WS_VISIBLE | WS_CHILD, 320, 10, 25, 22, hWnd, NULL, NULL, NULL);
			data->hWndPaletteInput = CreateWindow(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_HASSTRINGS | CBS_DROPDOWNLIST, 120, 37, 100, 200, hWnd, NULL, NULL, NULL);
			data->hWndPalettesInput = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"1", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER, 120, 64, 100, 22, hWnd, NULL, NULL, NULL);
			data->hWndDitherCheckbox = CreateWindow(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 120, 91, 22, 22, hWnd, NULL, NULL, NULL);
			data->hWndImportButton = CreateWindow(L"BUTTON", L"Import", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 120, 118, 100, 22, hWnd, NULL, NULL, NULL);

			for (int i = 0; i < 16; i++) {
				WCHAR textBuffer[4];
				wsprintf(textBuffer, L"%d", i);
				SendMessage(data->hWndPaletteInput, CB_ADDSTRING, wcslen(textBuffer), (LPARAM) textBuffer);
			}
			SendMessage(data->hWndDitherCheckbox, BM_SETCHECK, 1, 0);

			SetWindowSize(hWnd, 355, 150);
			EnumChildWindows(hWnd, SetFontProc, GetStockObject(DEFAULT_GUI_FONT));
			break;
		}
		case NV_INITIMPORTDIALOG:
		{
			WORD d = wParam;
			int palette = (d >> 12) & 0xF;
			int charOrigin = d & 0x3FF;
			int nscrTileX = LOWORD(lParam);
			int nscrTileY = HIWORD(lParam);

			data->nscrTileX = nscrTileX;
			data->nscrTileY = nscrTileY;
			data->characterOrigin = charOrigin;

			SendMessage(data->hWndPaletteInput, CB_SETCURSEL, palette, 0);
			break;
		}
		case WM_COMMAND:
		{
			if (lParam) {
				HWND hWndControl = (HWND) lParam;
				if (hWndControl == data->hWndBrowseButton) {
					LPWSTR location = openFileDialog(hWnd, L"Select Bitmap", L"Supported Image Files\0*.png;*.bmp;*.gif;*.jpg;*.jpeg\0All Files\0*.*\0", L"");
					if (!location) break;

					SendMessage(data->hWndBitmapName, WM_SETTEXT, wcslen(location), (LPARAM) location);

					free(location);
				} else if (hWndControl == data->hWndImportButton) {
					WCHAR textBuffer[MAX_PATH + 1];
					SendMessage(data->hWndBitmapName, WM_GETTEXT, (WPARAM) MAX_PATH, (LPARAM) textBuffer);
					int width, height;
					DWORD *px = gdipReadImage(textBuffer, &width, &height);
					int tilesX = width / 8;
					int tilesY = height / 8;

					SendMessage(data->hWndPalettesInput, WM_GETTEXT, (WPARAM) MAX_PATH, (LPARAM) textBuffer);
					int nPalettes = _wtoi(textBuffer);
					if (nPalettes > 16) nPalettes = 16;
					int paletteNumber = SendMessage(data->hWndPaletteInput, CB_GETCURSEL, 0, 0);
					int diffuse = SendMessage(data->hWndDitherCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;

					HWND hWndMain = (HWND) GetWindowLong(hWnd, GWL_HWNDPARENT);
					NITROPAINTSTRUCT *nitroPaintStruct = (NITROPAINTSTRUCT *) GetWindowLongPtr(hWndMain, 0);
					HWND hWndNcgrViewer = nitroPaintStruct->hWndNcgrViewer;
					NCGRVIEWERDATA *ncgrViewerData = (NCGRVIEWERDATA *) GetWindowLongPtr(hWndNcgrViewer, 0);
					NCGR *ncgr = &ncgrViewerData->ncgr;
					HWND hWndNscrViewer = nitroPaintStruct->hWndNscrViewer;
					NSCRVIEWERDATA *nscrViewerData = (NSCRVIEWERDATA *) GetWindowLongPtr(hWndNscrViewer, 0);
					NSCR *nscr = &nscrViewerData->nscr;
					int paletteSize = ncgr->nBits == 4 ? 16 : 256;
					int maxTilesX = (nscr->nWidth / 8) - data->nscrTileX;
					int maxTilesY = (nscr->nHeight / 8) - data->nscrTileY;
					if (tilesX > maxTilesX) tilesX = maxTilesX;
					if (tilesY > maxTilesY) tilesY = maxTilesY;

					DWORD *blocks = (DWORD *) calloc(tilesX * tilesY, 64 * 4);

					//split image into 8x8 chunks, and find the average color in each.
					DWORD *avgs = calloc(tilesX * tilesY, 4);
					for (int y = 0; y < tilesY; y++) {
						for (int x = 0; x < tilesX; x++) {
							int srcOffset = x * 8 + y * 8 * (width);
							DWORD *block = blocks + 64 * (x + y * tilesX);
							CopyMemory(block, px + srcOffset, 32);
							CopyMemory(block + 8, px + srcOffset + width, 32);
							CopyMemory(block + 16, px + srcOffset + width * 2, 32);
							CopyMemory(block + 24, px + srcOffset + width * 3, 32);
							CopyMemory(block + 32, px + srcOffset + width * 4, 32);
							CopyMemory(block + 40, px + srcOffset + width * 5, 32);
							CopyMemory(block + 48, px + srcOffset + width * 6, 32);
							CopyMemory(block + 56, px + srcOffset + width * 7, 32);
							DWORD avg = averageColor(block, 64);
							avgs[x + y * tilesX] = avg;
						}
					}

					//generate an nPalettes color palette
					DWORD *avgPals = (DWORD *) calloc(nPalettes + 1, 4);
					createPalette_(avgs, tilesX, tilesY, avgPals, nPalettes + 1); //+1 because 1 color is reserved

					int useCounts[16] = { 0 };
					int *closests = calloc(tilesX * tilesY, sizeof(int));

					//form a best guess of how to divide the tiles amonng the palettes.
					//for each tile, see which color in avgPals (excluding entry 0) matches a tile's average.
					for (int y = 0; y < tilesY; y++) {
						for (int x = 0; x < tilesX; x++) {
							DWORD *block = blocks + 64 * (x + y * tilesX);
							DWORD avg = averageColor(block, 64);
							int closest = 0;
							if (avg & 0xFF000000) closest = closestpalette(*(RGB *) &avg, (RGB*) (avgPals + 1), nPalettes, NULL);
							useCounts[closest]++;
							closests[x + y * tilesX] = closest;
						}
					}

					//refine the choice of palettes.
					DWORD *pals = calloc(nPalettes * paletteSize, 4);
					{
						//create an array for temporary work.
						int *tempClosests = calloc(tilesX * tilesY, sizeof(int));
						int tempUseCounts[16];
						int bestError = computeMultiPaletteError(closests, blocks, tilesX, tilesY, width, pals, nPalettes, paletteSize);

						while (1) {
							int nChanged = 0;
							for (int i = 0; i < tilesX * tilesY; i++) {
								int x = i % tilesX;
								int y = i / tilesX;
								DWORD *block = blocks + 64 * (x + y * tilesX);

								//go over each group to see if this tile works better in another.
								for (int j = 0; j < nPalettes; j++) {
									if (j == closests[i]) continue;
									memcpy(tempClosests, closests, tilesX * tilesY * sizeof(int));
									memcpy(tempUseCounts, useCounts, sizeof(useCounts));

									tempClosests[i] = j;
									tempUseCounts[j]++;
									tempUseCounts[closests[i]]--;
									createMultiPalettes(px, tilesX, tilesY, width, pals, nPalettes, paletteSize, useCounts, closests);

									//compute total error
									int error = computeMultiPaletteError(tempClosests, blocks, tilesX, tilesY, width, pals, nPalettes, paletteSize);
									if (error < bestError) {
										bestError = error;
										int oldClosest = closests[i];
										closests[i] = j;
										useCounts[j]++;
										useCounts[oldClosest]--;
										nChanged++;
									}
								}
							}
							if (nChanged == 0) break;
						}

						free(tempClosests);
					}
					
					//now, create a new bitmap for each set of tiles that share a palette.
					createMultiPalettes(px, tilesX, tilesY, width, pals, nPalettes, paletteSize, useCounts, closests);

					int charBase = 0;
					if (nscr->nHighestIndex >= ncgr->nTiles) {
						charBase = nscr->nHighestIndex + 1 - ncgr->nTiles;
					}

					//write to NCLR
					HWND hWndNclrViewer = nitroPaintStruct->hWndNclrViewer;
					NCLRVIEWERDATA *nclrViewerData = (NCLRVIEWERDATA *) GetWindowLongPtr(hWndNclrViewer, 0);
					NCLR *nclr = &nclrViewerData->nclr;
					WORD *destPalette = nclr->colors + paletteNumber * paletteSize;
					for (int i = 0; i < nPalettes; i++) {
						WORD *dest = destPalette + i * paletteSize;
						for (int j = 0; j < paletteSize; j++) {
							DWORD col = (pals + i * paletteSize)[j];
							int r = col & 0xFF;
							int g = (col >> 8) & 0xFF;
							int b = (col >> 16) & 0xFF;
							r = r * 31 / 255;
							g = g * 31 / 255;
							b = b * 31 / 255;
							dest[j] = r | (g << 5) | (b << 10);
						}
					}

					//next, start palette matching. See which palette best fits a tile, set it in the NSCR, then write the bits to the NCGR.
					WORD *nscrData = nscr->data;
					for (int y = 0; y < tilesY; y++) {
						for (int x = 0; x < tilesX; x++) {
							DWORD *block = blocks + 64 * (x + y * tilesX);
							
							int leastError = 0x7FFFFFFF;
							int leastIndex = 0;
							for (int i = 0; i < nPalettes; i++) {
								int err = getPaletteError((RGB*) block, 64, pals + i * paletteSize, paletteSize);
								if (err < leastError) {
									leastError = err;
									leastIndex = i;
								}
							}

							int nscrX = x + data->nscrTileX;
							int nscrY = y + data->nscrTileY;

							WORD d = nscrData[nscrX + nscrY * (nscr->nWidth >> 3)];
							d = d & 0xFFF;
							d |= (leastIndex + paletteNumber) << 12;
							nscrData[nscrX + nscrY * (nscr->nWidth >> 3)] = d;

							int charOrigin = d & 0x3FF;
							int ncgrX = charOrigin % ncgr->tilesX;
							int ncgrY = charOrigin / ncgr->tilesX;
							if (charOrigin - charBase < 0) continue;
							BYTE *ncgrTile = ncgr->tiles[charOrigin - charBase];
							for (int i = 0; i < 64; i++) {
								if ((block[i] & 0xFF000000) == 0) ncgrTile[i] = 0;
								else {
									int index = 1 + closestpalette(*(RGB *) &block[i], pals + leastIndex * paletteSize + 1, paletteSize - 1, NULL);
									if (diffuse) {
										RGB original = *(RGB *) &block[i];
										RGB closest = ((RGB *) (pals + leastIndex * paletteSize))[index];
										int er = closest.r - original.r;
										int eg = closest.g - original.g;
										int eb = closest.b - original.b;
										doDiffuse(i, 8, 8, block, -er, -eg, -eb, 0, 1.0f);
									}
									ncgrTile[i] = index;
								}
							}
						}
					}

					InvalidateRect(hWndNclrViewer, NULL, FALSE);
					InvalidateRect(hWndNscrViewer, NULL, FALSE);
					InvalidateRect(hWndNcgrViewer, NULL, FALSE);

					free(blocks);
					free(pals);
					free(closests);
					free(avgPals);
					free(px);
					free(avgs);
					PostMessage(hWnd, WM_CLOSE, 0, 0);
				}
			}
			break;
		}
		case WM_CLOSE:
		{
			HWND hWndParent = (HWND) GetWindowLong(hWnd, GWL_HWNDPARENT);
			SetWindowLong(hWndParent, GWL_STYLE, GetWindowLong(hWndParent, GWL_STYLE) & ~WS_DISABLED);
			SetFocus(hWndParent);
			break;
		}
		case WM_DESTROY:
		{
			free(data);
			break;
		}
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

VOID RegisterNscrTileEditorClass(VOID) {
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(wcex);
	wcex.hbrBackground = g_useDarkTheme? CreateSolidBrush(RGB(32, 32, 32)): (HBRUSH) COLOR_WINDOW;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpszClassName = L"NscrTileEditorClass";
	wcex.lpfnWndProc = NscrTileEditorWndProc;
	wcex.cbWndExtra = sizeof(LPVOID);
	wcex.hIcon = g_appIcon;
	wcex.hIconSm = g_appIcon;
	RegisterClassEx(&wcex);
}

VOID RegisterNscrBitmapImportClass(VOID) {
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(wcex);
	wcex.hbrBackground = g_useDarkTheme? CreateSolidBrush(RGB(32, 32, 32)): (HBRUSH) COLOR_WINDOW;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpszClassName = L"NscrBitmapImportClass";
	wcex.lpfnWndProc = NscrBitmapImportWndProc;
	wcex.cbWndExtra = sizeof(LPVOID);
	wcex.hIcon = g_appIcon;
	wcex.hIconSm = g_appIcon;
	RegisterClassEx(&wcex);
}

VOID RegisterNscrViewerClass(VOID) {
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(wcex);
	wcex.hbrBackground = g_useDarkTheme? CreateSolidBrush(RGB(32, 32, 32)): (HBRUSH) COLOR_WINDOW;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpszClassName = L"NscrViewerClass";
	wcex.lpfnWndProc = NscrViewerWndProc;
	wcex.cbWndExtra = sizeof(LPVOID);
	wcex.hIcon = g_appIcon;
	wcex.hIconSm = g_appIcon;
	RegisterClassEx(&wcex);
	RegisterNscrTileEditorClass();
	RegisterNscrBitmapImportClass();
}

HWND CreateNscrViewer(int x, int y, int width, int height, HWND hWndParent, LPWSTR path) {
	NSCR nscr;
	int n = nscrReadFile(&nscr, path);
	if (n) {
		MessageBox(hWndParent, L"Invalid file.", L"Invalid file", MB_ICONERROR);
		return NULL;
	}
	width = nscr.nWidth;
	height = nscr.nHeight;

	if (width != CW_USEDEFAULT && height != CW_USEDEFAULT) {
		RECT rc = { 0 };
		rc.right = width;
		rc.bottom = height;
		AdjustWindowRect(&rc, WS_CAPTION | WS_THICKFRAME | WS_SYSMENU, FALSE);
		width = rc.right - rc.left + 4; //+4 to account for WS_EX_CLIENTEDGE
		height = rc.bottom - rc.top + 4;
		HWND h = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_MDICHILD, L"NscrViewerClass", L"NSCR Viewer", WS_VISIBLE | WS_CLIPSIBLINGS | WS_CAPTION | WS_CLIPCHILDREN, x, y, width, height, hWndParent, NULL, NULL, NULL);
		SendMessage(h, NV_INITIALIZE, (WPARAM) path, (LPARAM) &nscr);

		if (nscr.isHudson) {
			SendMessage(h, WM_SETICON, ICON_BIG, (LPARAM) LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON2)));
		}
		return h;
	}
	HWND h = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_MDICHILD, L"NscrViewerClass", L"NSCR Viewer", WS_VISIBLE | WS_CLIPSIBLINGS | WS_HSCROLL | WS_VSCROLL | WS_CAPTION | WS_CLIPCHILDREN, x, y, width, height, hWndParent, NULL, NULL, NULL);
	SendMessage(h, NV_INITIALIZE, (WPARAM) path, (LPARAM) &nscr);
	return h;
}