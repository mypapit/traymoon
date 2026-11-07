/*
	main.c - main module

	Copyright © 2004 Mohammad Hafiz bin Ismail

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


	Report problems and direct all questions to:
	<mypapit@gmail.com>

	For latest version of this software, please visit :

	http://traymoon.googlecode.com
	http://blog.mypapit.net

*/

#define _WIN32_LEAN_AND_MEAN_
#ifndef _WIN32_IE
#define _WIN32_IE 0x0500
#endif
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include "resource.h"
#include "traymoon.h"

#ifndef NIF_INFO
#define NIF_INFO 0x00000010
#endif
#ifndef NIIF_INFO
#define NIIF_INFO 0x00000001
#endif

typedef struct {
	int wYear;
	int wMonth; /* 1..12 */
	int wDay;   /* 1..30 */
} Haribulan;

typedef struct {
	int month;  /* 1..12 */
	int day;    /* 1..30 */
	const char *nameMalay;
	const char *nameIntl;
} HijriEvent;

static const HijriEvent g_HijriEvents[] = {
	{ 1,  1, "Awal Muharram (Maal Hijrah)", "Islamic New Year (1 Muharram)" },
	{ 1, 10, "Hari Asyura",                 "Day of Ashura" },
	{ 3, 12, "Maulidur Rasul",              "Mawlid al-Nabi (Prophet's Birthday)" },
	{ 7, 27, "Israk dan Mikraj",            "Isra' and Mi'raj" },
	{ 8, 15, "Nisfu Syaaban",               "Mid-Sha'ban (Nisfu Sha'ban)" },
	{ 9,  1, "Awal Ramadan",                "First of Ramadan" },
	{ 9, 17, "Nuzul Al-Quran",              "Nuzul Al-Quran" },
	{ 10, 1, "Hari Raya Aidilfitri",        "Eid al-Fitr (1 Shawwal)" },
	{ 12, 1, "Awal Zulhijjah",              "First of Dhu'l-Hijja" },
	{ 12, 9, "Hari Arafah",                 "Day of Arafah" },
	{ 12, 10, "Hari Raya Aidiladha",        "Eid al-Adha" }
};
#define NUM_HIJRI_EVENTS (sizeof(g_HijriEvents) / sizeof(g_HijriEvents[0]))

static char **strMonth;
static struct tmData tmdata;

static char *BulanMelayu[] = {
	"Muharam", "Safar", "RabiulAwal", "RabiulAkhir", "JamadilAwal", "JamadilAkhir",
	"Rejab", "Syaaban", "Ramadan", "Syawal", "Zulkaedah", "Zulhijjah"
};
static char *BulanInternational[] = {
	"Muharam", "Safar", "Rabiul I", "Rabiul II", "Jumada I", "Jumada II",
	"Rajab", "Sha'ban", "Ramadan", "Shawwal", "Dhu'l-Qa'dah", "Dhu'l-Hijja"
};

static const char *RotateOptions[] = {
	"0 deg (Northern)",
	"90 deg (Equatorial)",
	"180 deg (Southern)",
	"270 deg (Equatorial Flip)"
};

static const char *StyleOptions[] = {
	"Classic Icons",
	"Smooth Silver Moon",
	"Warm Gold Moon"
};

static Haribulan hb;
static HINSTANCE hInstance;
static HICON g_hTrayIcon = NULL;
static HICON g_hDialogIcon = NULL;
static int g_reminded8AMKey = 0;
static int g_reminded12PMKey = 0;
static WORD g_lastCheckedDay = 0;

int toHijrah(SYSTEMTIME hbTime, Haribulan *hijrah);
int moon_phase(int y, int m, int d);
int updateTray(NOTIFYICONDATA *NotifyIconData, HWND hwnd, struct tmData tmdata, int fasa, BOOL checkReminder);
HICON getMoonIcon(int phase, int size, int rotate, int style);
const char* getMonthName(int month1Based, int useMalay);
const HijriEvent* findTodayHijriEvent(Haribulan h);
const HijriEvent* findNextHijriEvent(Haribulan h, int *daysUntilOut);
void formatEventStatus(Haribulan h, int useMalay, char *buf, int bufSize);
void showHijriEventsDialog(HWND hwnd, Haribulan h, int useMalay);

static int isqrt_int(int n)
{
	int x = n;
	int y = (x + 1) / 2;
	if (n <= 0) return 0;
	while (y < x) {
		x = y;
		y = (x + n / x) / 2;
	}
	return x;
}

static void rotateCoords(int dx, int dy, int rotate, int *rx, int *ry)
{
	switch (rotate & 3) {
		case 0: *rx = dx;  *ry = dy;  break; /* 0 deg */
		case 1: *rx = dy;  *ry = -dx; break; /* 90 deg CW */
		case 2: *rx = -dx; *ry = -dy; break; /* 180 deg */
		case 3: *rx = -dy; *ry = dx;  break; /* 270 deg CW */
	}
}

/*
	Creates a 32-bit ARGB procedural lunar phase icon with rotation & style,
	or rotates one of the classic resource icons (bulan1.ico - bulan8.ico).
*/
static HICON createProceduralMoonIcon(int phase, int size, int rotate, int style)
{
	BITMAPV5HEADER bi;
	HDC hdc;
	void *lpBits = NULL;
	HBITMAP hBitmap, hMonoBitmap;
	ICONINFO ii;
	HICON hIcon = NULL;
	DWORD *pixels;
	int x, y, sx, sy;
	int scale = 2; /* 2x2 supersampling for anti-aliased edges */
	int subSize = size * scale;
	int center = subSize / 2;
	int radius = (subSize / 2) - scale;
	int r2 = radius * radius;

	if (radius < 4) radius = 4;
	r2 = radius * radius;

	ZeroMemory(&bi, sizeof(BITMAPV5HEADER));
	bi.bV5Size = sizeof(BITMAPV5HEADER);
	bi.bV5Width = size;
	bi.bV5Height = -size; /* top-down DIB */
	bi.bV5Planes = 1;
	bi.bV5BitCount = 32;
	bi.bV5Compression = BI_BITFIELDS;
	bi.bV5RedMask   = 0x00FF0000;
	bi.bV5GreenMask = 0x0000FF00;
	bi.bV5BlueMask  = 0x000000FF;
	bi.bV5AlphaMask = 0xFF000000;

	hdc = GetDC(NULL);
	hBitmap = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &lpBits, NULL, 0);
	ReleaseDC(NULL, hdc);

	if (!hBitmap || !lpBits) {
		if (hBitmap) DeleteObject(hBitmap);
		return NULL;
	}

	pixels = (DWORD *)lpBits;

	for (y = 0; y < size; y++) {
		for (x = 0; x < size; x++) {
			int totalA = 0, totalR = 0, totalG = 0, totalB = 0;

			for (sy = 0; sy < scale; sy++) {
				for (sx = 0; sx < scale; sx++) {
					int px = x * scale + sx - center + (scale / 2);
					int py = y * scale + sy - center + (scale / 2);
					int dist2 = px * px + py * py;

					if (dist2 <= r2) {
						int rx, ry;
						int chordW;
						int isLit = 0;
						int z, shade;
						int r, g, b;
						int craterPattern;

						rotateCoords(px, py, rotate, &rx, &ry);
						chordW = isqrt_int(r2 - ry * ry);

						switch (phase & 7) {
							case 0: isLit = 0; break;                         /* New Moon */
							case 1: isLit = (rx > (chordW / 2)) ? 1 : 0; break;  /* Waxing Crescent */
							case 2: isLit = (rx >= 0) ? 1 : 0; break;            /* 1st Quarter */
							case 3: isLit = (rx >= -(chordW / 2)) ? 1 : 0; break;/* Waxing Gibbous */
							case 4: isLit = 1; break;                         /* Full Moon */
							case 5: isLit = (rx <= (chordW / 2)) ? 1 : 0; break; /* Waning Gibbous */
							case 6: isLit = (rx <= 0) ? 1 : 0; break;            /* 3rd Quarter */
							case 7: isLit = (rx < -(chordW / 2)) ? 1 : 0; break; /* Waning Crescent */
						}

						/* 3D sphere shading + subtle lunar maria texture */
						z = isqrt_int(r2 - dist2);
						shade = (z * 100) / radius; /* 0..100 */
						craterPattern = ((rx * 3 + ry * 5) ^ (rx * 7 - ry * 3)) & 15;

						if (isLit) {
							if (style == 2) {
								/* Warm Gold Moon */
								r = 215 + (shade * 40) / 100 - (craterPattern > 11 ? 14 : 0);
								g = 175 + (shade * 55) / 100 - (craterPattern > 11 ? 12 : 0);
								b = 70  + (shade * 45) / 100 - (craterPattern > 11 ? 8 : 0);
							} else {
								/* Smooth Silver Moon */
								r = 185 + (shade * 65) / 100 - (craterPattern > 11 ? 16 : 0);
								g = 192 + (shade * 60) / 100 - (craterPattern > 11 ? 16 : 0);
								b = 205 + (shade * 50) / 100 - (craterPattern > 11 ? 12 : 0);
							}
						} else {
							/* Dark side (earthshine + subtle rim) */
							int rim = (dist2 > (r2 * 78) / 100) ? 28 : 0;
							if (style == 2) {
								r = 38 + (shade * 18) / 100 + rim;
								g = 34 + (shade * 15) / 100 + rim;
								b = 52 + (shade * 22) / 100 + rim;
							} else {
								r = 32 + (shade * 20) / 100 + rim;
								g = 40 + (shade * 24) / 100 + rim;
								b = 58 + (shade * 30) / 100 + rim;
							}
						}

						if (r > 255) r = 255;
						if (r < 0) r = 0;
						if (g > 255) g = 255;
						if (g < 0) g = 0;
						if (b > 255) b = 255;
						if (b < 0) b = 0;

						totalA += 255;
						totalR += r;
						totalG += g;
						totalB += b;
					}
				}
			}

			{
				int samples = scale * scale;
				DWORD a = (DWORD)(totalA / samples);
				DWORD r = (DWORD)(totalR / samples);
				DWORD g = (DWORD)(totalG / samples);
				DWORD b = (DWORD)(totalB / samples);
				pixels[y * size + x] = (a << 24) | (r << 16) | (g << 8) | b;
			}
		}
	}

	hMonoBitmap = CreateBitmap(size, size, 1, 1, NULL);
	ii.fIcon = TRUE;
	ii.xHotspot = 0;
	ii.yHotspot = 0;
	ii.hbmMask = hMonoBitmap;
	ii.hbmColor = hBitmap;

	hIcon = CreateIconIndirect(&ii);

	DeleteObject(hBitmap);
	DeleteObject(hMonoBitmap);
	return hIcon;
}

/*
	Rotates a classic resource icon by 0, 90, 180, or 270 degrees CW.
*/
static HICON loadAndRotateClassicIcon(int phase, int size, int rotate)
{
	HICON hSrcIcon;
	BITMAPINFO bi;
	HDC hdcScreen, hdcSrc, hdcMask;
	void *srcBits = NULL, *maskBits = NULL, *dstBits = NULL;
	HBITMAP hbmSrc = NULL, hbmMask = NULL, hbmOldSrc, hbmOldMask;
	HBITMAP hbmDst = NULL, hbmDstMask = NULL;
	ICONINFO ii;
	HICON hRotIcon = NULL;
	int x, y;

	hSrcIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(900 + (phase & 7)),
		IMAGE_ICON, size, size, LR_DEFAULTCOLOR);
	if (!hSrcIcon) {
		hSrcIcon = LoadIcon(hInstance, MAKEINTRESOURCE(900 + (phase & 7)));
	}
	if (!hSrcIcon || (rotate & 3) == 0) {
		return hSrcIcon;
	}

	ZeroMemory(&bi, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = size;
	bi.bmiHeader.biHeight = -size; /* top-down */
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	hdcScreen = GetDC(NULL);
	hdcSrc = CreateCompatibleDC(hdcScreen);
	hdcMask = CreateCompatibleDC(hdcScreen);

	hbmSrc = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &srcBits, NULL, 0);
	hbmMask = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &maskBits, NULL, 0);
	hbmDst = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &dstBits, NULL, 0);
	ReleaseDC(NULL, hdcScreen);

	if (hbmSrc && hbmMask && hbmDst && srcBits && maskBits && dstBits) {
		DWORD *pSrc = (DWORD *)srcBits;
		DWORD *pMask = (DWORD *)maskBits;
		DWORD *pDst = (DWORD *)dstBits;

		/* Draw icon onto black background (color) and white background (to extract mask) */
		ZeroMemory(srcBits, size * size * 4);
		FillMemory(maskBits, size * size * 4, 0xFF);

		hbmOldSrc = (HBITMAP)SelectObject(hdcSrc, hbmSrc);
		DrawIconEx(hdcSrc, 0, 0, hSrcIcon, size, size, 0, NULL, DI_NORMAL);
		SelectObject(hdcSrc, hbmOldSrc);

		hbmOldMask = (HBITMAP)SelectObject(hdcMask, hbmMask);
		DrawIconEx(hdcMask, 0, 0, hSrcIcon, size, size, 0, NULL, DI_NORMAL);
		SelectObject(hdcMask, hbmOldMask);

		for (y = 0; y < size; y++) {
			for (x = 0; x < size; x++) {
				int sx = x, sy = y;
				DWORD c, m;
				switch (rotate & 3) {
					case 1: sx = y; sy = size - 1 - x; break;            /* 90 deg CW */
					case 2: sx = size - 1 - x; sy = size - 1 - y; break; /* 180 deg */
					case 3: sx = size - 1 - y; sy = x; break;            /* 270 deg CW */
				}
				c = pSrc[sy * size + sx] & 0x00FFFFFF;
				m = pMask[sy * size + sx] & 0x00FFFFFF;
				/* If pixel changed between black and white backbuffer, it's transparent */
				if (c == 0x000000 && m == 0xFFFFFF) {
					pDst[y * size + x] = 0x00000000;
				} else {
					pDst[y * size + x] = 0xFF000000 | c;
				}
			}
		}

		hbmDstMask = CreateBitmap(size, size, 1, 1, NULL);
		ii.fIcon = TRUE;
		ii.xHotspot = 0;
		ii.yHotspot = 0;
		ii.hbmMask = hbmDstMask;
		ii.hbmColor = hbmDst;
		hRotIcon = CreateIconIndirect(&ii);
		DeleteObject(hbmDstMask);
	}

	if (hbmSrc) DeleteObject(hbmSrc);
	if (hbmMask) DeleteObject(hbmMask);
	if (hbmDst) DeleteObject(hbmDst);
	DeleteDC(hdcSrc);
	DeleteDC(hdcMask);
	DestroyIcon(hSrcIcon);

	return hRotIcon;
}

HICON getMoonIcon(int phase, int size, int rotate, int style)
{
	if (style == 1 || style == 2) {
		HICON hGenerated = createProceduralMoonIcon(phase, size, rotate, style);
		if (hGenerated) return hGenerated;
	}
	return loadAndRotateClassicIcon(phase, size, rotate);
}

const char* getMonthName(int month1Based, int useMalay)
{
	int idx = month1Based - 1;
	if (idx < 0 || idx > 11) idx = 0;
	return useMalay ? BulanMelayu[idx] : BulanInternational[idx];
}

const HijriEvent* findTodayHijriEvent(Haribulan h)
{
	int i;
	for (i = 0; i < (int)NUM_HIJRI_EVENTS; i++) {
		if (g_HijriEvents[i].month == h.wMonth && g_HijriEvents[i].day == h.wDay) {
			return &g_HijriEvents[i];
		}
	}
	return NULL;
}

const HijriEvent* findNextHijriEvent(Haribulan h, int *daysUntilOut)
{
	int i;
	int curDayOfYear = (h.wMonth - 1) * 30 + h.wDay;
	int bestDiff = 9999;
	const HijriEvent *bestEvent = &g_HijriEvents[0];

	for (i = 0; i < (int)NUM_HIJRI_EVENTS; i++) {
		int evDayOfYear = (g_HijriEvents[i].month - 1) * 30 + g_HijriEvents[i].day;
		int diff = evDayOfYear - curDayOfYear;
		if (diff < 0) {
			diff += 354;
		}
		if (diff < bestDiff) {
			bestDiff = diff;
			bestEvent = &g_HijriEvents[i];
		}
	}
	if (daysUntilOut) *daysUntilOut = bestDiff;
	return bestEvent;
}

void formatEventStatus(Haribulan h, int useMalay, char *buf, int bufSize)
{
	int daysUntil = 0;
	const HijriEvent *ev = findNextHijriEvent(h, &daysUntil);
	const char *evName = useMalay ? ev->nameMalay : ev->nameIntl;
	const char *evMonth = getMonthName(ev->month, useMalay);
	(void)bufSize;

	if (daysUntil == 0) {
		wsprintf(buf, "%s: %s!", useMalay ? "Hari ini" : "Today", evName);
	} else if (daysUntil == 1) {
		wsprintf(buf, "%s: %s (1 %s)", useMalay ? "Esok" : "Tomorrow", evName, useMalay ? "hari" : "day");
	} else if ((h.wDay >= 13 && h.wDay <= 15) && !(h.wMonth == 12 && h.wDay == 13)) {
		wsprintf(buf, "%s | %s: %s (%d %s)",
			useMalay ? "Hari Putih (13-15)" : "White Days (13-15)",
			useMalay ? "Akan datang" : "Next",
			evName, ev->day, evMonth);
	} else {
		wsprintf(buf, "%s: %s (%d %s)",
			useMalay ? "Berikutnya" : "Next",
			evName, ev->day, evMonth);
	}
}

void showHijriEventsDialog(HWND hwnd, Haribulan h, int useMalay)
{
	char msg[1024];
	char line[128];
	int i;
	int daysUntil = 0;
	const HijriEvent *nextEv = findNextHijriEvent(h, &daysUntil);

	wsprintf(msg, "%s (%d H):\r\n\r\n",
		useMalay ? "Tarikh Penting Kalendar Hijrah" : "Important Hijri Calendar Dates",
		h.wYear);

	for (i = 0; i < (int)NUM_HIJRI_EVENTS; i++) {
		const char *mName = getMonthName(g_HijriEvents[i].month, useMalay);
		const char *eName = useMalay ? g_HijriEvents[i].nameMalay : g_HijriEvents[i].nameIntl;
		const char *marker = "   ";
		if (&g_HijriEvents[i] == nextEv) {
			marker = (daysUntil == 0) ? "[TODAY] " : "[NEXT]  ";
		}
		wsprintf(line, "%s%d %s - %s\r\n", marker, g_HijriEvents[i].day, mName, eName);
		lstrcat(msg, line);
	}

	lstrcat(msg, "\r\n* 13, 14, 15 ");
	lstrcat(msg, useMalay ? "setiap bulan: Hari Putih (Puasa Sunat Ayyamul Bidh)" : "of each month: White Days (Ayyamul Bidh)");

	MessageBox(hwnd, msg, useMalay ? "Peristiwa Penting Hijrah" : "Important Hijri Dates", MB_OK | MB_ICONINFORMATION);
}

static void refreshDialogPreview(HWND hwndDlg, int fasa)
{
	int previewRotate = (int)SendDlgItemMessage(hwndDlg, IDC_ROTATE, CB_GETCURSEL, 0, 0);
	int previewStyle  = (int)SendDlgItemMessage(hwndDlg, IDC_STYLE, CB_GETCURSEL, 0, 0);
	int previewMalay  = (int)SendDlgItemMessage(hwndDlg, IDC_MALAY, BM_GETCHECK, 0, 0);
	char strDate[64];
	char strEvent[128];

	if (previewRotate == CB_ERR) previewRotate = tmdata.rotate;
	if (previewStyle == CB_ERR)  previewStyle = tmdata.style;

	if (g_hDialogIcon) {
		DestroyIcon(g_hDialogIcon);
		g_hDialogIcon = NULL;
	}
	g_hDialogIcon = getMoonIcon(fasa, 32, previewRotate, previewStyle);
	SendDlgItemMessage(hwndDlg, ID_IMEJBULAN, STM_SETICON, (WPARAM)g_hDialogIcon, 0);

	wsprintf(strDate, "%d %s %d H", hb.wDay, getMonthName(hb.wMonth, previewMalay), hb.wYear);
	SetDlgItemText(hwndDlg, IDC_HIJRAH, strDate);

	formatEventStatus(hb, previewMalay, strEvent, sizeof(strEvent));
	SetDlgItemText(hwndDlg, IDC_EVENT_INFO, strEvent);
}

VOID CALLBACK periodicUpdate(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD dwTime)
{
	SYSTEMTIME now;
	int todayKey;
	(void)msg;
	(void)idEvent;
	(void)dwTime;

	GetLocalTime(&now);
	todayKey = (hb.wYear * 10000) + (hb.wMonth * 100) + hb.wDay;

	/* Refresh when the day rolls over, or at 8:00 AM / 12:00 PM if today's slot hasn't fired yet */
	if (now.wDay != g_lastCheckedDay ||
	    (now.wHour >= 8 && now.wHour < 12 && g_reminded8AMKey != todayKey) ||
	    (now.wHour >= 12 && g_reminded12PMKey != todayKey)) {
		g_lastCheckedDay = now.wDay;
		SendMessage(hwnd, WM_COMMAND, ID_FILE_REFRESH, 0);
	}
}

static INT_PTR CALLBACK DialogFunc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HMENU hMenu;
	static SYSTEMTIME systime;
	int idEvent = 90;
	static char ayyat[64];
	static NOTIFYICONDATA NotifyIconData = {0};

	static char *MesejBulan[] = {
		"New Moon (Bulan Baru)",
		"Waxing Crescent (Bulan Mengambang Suku)",
		"1st Quarter Moon (Bulan Mengambang Separuh)",
		"Waxing Gibbous (Bulan Mengambang 3/4)",
		"Full Moon (Bulan Penuh)",
		"Waning Gibbous (Bulan Menurun 3/4)",
		"3rd Quarter Moon (Bulan Menurun Separuh)",
		"Waning Crescent (Bulan Menurun Suku)"
	};
	POINT MouseCoordinates;
	RECT rc;
	int i = 0;
	static int fasa;

	switch (msg) {

		case WM_INITDIALOG:
			SetTimer(hwndDlg, idEvent, 60000L, periodicUpdate); /* check time every 1 minute for 8 AM & 12 PM reminders */

			strMonth = tmdata.malay ? BulanMelayu : BulanInternational;

			/* Populate Rotate and Style ComboBoxes */
			for (i = 0; i < 4; i++) {
				SendDlgItemMessage(hwndDlg, IDC_ROTATE, CB_ADDSTRING, 0, (LPARAM)RotateOptions[i]);
			}
			SendDlgItemMessage(hwndDlg, IDC_ROTATE, CB_SETCURSEL, tmdata.rotate & 3, 0);

			for (i = 0; i < 3; i++) {
				SendDlgItemMessage(hwndDlg, IDC_STYLE, CB_ADDSTRING, 0, (LPARAM)StyleOptions[i]);
			}
			SendDlgItemMessage(hwndDlg, IDC_STYLE, CB_SETCURSEL, (tmdata.style >= 0 && tmdata.style <= 2) ? tmdata.style : 1, 0);

			/* Center dialog */
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL,
				(LPARAM)LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON5)));
			GetWindowRect(hwndDlg, &rc);
			SetWindowPos(hwndDlg, NULL,
				(GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2,
				(GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2,
				0, 0, SWP_NOSIZE | SWP_NOACTIVATE);

			/* Create tray menu */
			hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU));

			/* Get local time and moon phase */
			GetLocalTime(&systime);
			g_lastCheckedDay = systime.wDay;
			fasa = moon_phase(systime.wYear, systime.wMonth, systime.wDay);
			toHijrah(systime, &hb);

			wsprintf(ayyat, "%d %s %d H", hb.wDay, getMonthName(hb.wMonth, tmdata.malay), hb.wYear);
			SetDlgItemText(hwndDlg, IDC_HIJRAH, ayyat);
			SetDlgItemText(hwndDlg, IDC_BULAN, MesejBulan[fasa & 7]);

			/* Put icon in system tray */
			NotifyIconData.cbSize = sizeof(NOTIFYICONDATA);
			NotifyIconData.hWnd = hwndDlg;
			NotifyIconData.uID = 0;
			NotifyIconData.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
			NotifyIconData.uCallbackMessage = WM_USER;
			g_hTrayIcon = getMoonIcon(fasa, GetSystemMetrics(SM_CXSMICON), tmdata.rotate, tmdata.style);
			NotifyIconData.hIcon = g_hTrayIcon;
			lstrcpyn(NotifyIconData.szTip, ayyat, sizeof(NotifyIconData.szTip));
			Shell_NotifyIcon(NIM_ADD, &NotifyIconData);

			/* Update tooltip and check Hijri date reminder on startup */
			updateTray(&NotifyIconData, hwndDlg, tmdata, fasa, TRUE);
			break;

		case WM_USER:
			switch (lParam) {
				case WM_RBUTTONUP:
					SetForegroundWindow(hwndDlg);
					GetCursorPos(&MouseCoordinates);
					TrackPopupMenu(GetSubMenu(hMenu, 0), TPM_RIGHTALIGN | TPM_LEFTBUTTON,
						MouseCoordinates.x, MouseCoordinates.y, 0, hwndDlg, NULL);
					PostMessage(hwndDlg, WM_NULL, 0, 0);
					break;

				case WM_LBUTTONDBLCLK:
					SendMessage(hwndDlg, WM_COMMAND, ID_FILE_ABOUT, 0);
					break;
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {

				case IDC_ROTATE:
				case IDC_STYLE:
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						refreshDialogPreview(hwndDlg, fasa);
					}
					break;

				case IDC_MALAY:
					if (HIWORD(wParam) == BN_CLICKED) {
						refreshDialogPreview(hwndDlg, fasa);
					}
					break;

				case IDOK:
					/* Read settings from dialog and write to registry */
					tmdata.startup  = (int)SendDlgItemMessage(hwndDlg, IDC_STARTUP, BM_GETCHECK, 0, 0);
					tmdata.malay    = (int)SendDlgItemMessage(hwndDlg, IDC_MALAY, BM_GETCHECK, 0, 0);
					tmdata.reminder = (int)SendDlgItemMessage(hwndDlg, IDC_REMINDER, BM_GETCHECK, 0, 0);
					tmdata.rotate   = (int)SendDlgItemMessage(hwndDlg, IDC_ROTATE, CB_GETCURSEL, 0, 0);
					tmdata.style    = (int)SendDlgItemMessage(hwndDlg, IDC_STYLE, CB_GETCURSEL, 0, 0);
					if (tmdata.rotate == CB_ERR) tmdata.rotate = 0;
					if (tmdata.style == CB_ERR)  tmdata.style = 1;

					strMonth = tmdata.malay ? BulanMelayu : BulanInternational;

					save_registry(&tmdata);
					enable_autorun(tmdata.startup ? TRUE : FALSE, hInstance);
					ShowWindow(hwndDlg, SW_HIDE);
					updateTray(&NotifyIconData, hwndDlg, tmdata, fasa, TRUE);
					return 1;

				case ID_FILE_ABOUT:
					/* Initialize settings box with current state */
					SendDlgItemMessage(hwndDlg, IDC_STARTUP, BM_SETCHECK, tmdata.startup, 0);
					SendDlgItemMessage(hwndDlg, IDC_MALAY, BM_SETCHECK, tmdata.malay, 0);
					SendDlgItemMessage(hwndDlg, IDC_REMINDER, BM_SETCHECK, tmdata.reminder, 0);
					SendDlgItemMessage(hwndDlg, IDC_ROTATE, CB_SETCURSEL, tmdata.rotate & 3, 0);
					SendDlgItemMessage(hwndDlg, IDC_STYLE, CB_SETCURSEL, (tmdata.style >= 0 && tmdata.style <= 2) ? tmdata.style : 1, 0);
					refreshDialogPreview(hwndDlg, fasa);
					ShowWindow(hwndDlg, SW_SHOW);
					SetForegroundWindow(hwndDlg);
					break;

				case ID_FILE_REMINDERS:
					showHijriEventsDialog(hwndDlg, hb, tmdata.malay);
					break;

				case ID_FILE_REFRESH:
					GetLocalTime(&systime);
					fasa = moon_phase(systime.wYear, systime.wMonth, systime.wDay);
					toHijrah(systime, &hb);
					SetDlgItemText(hwndDlg, IDC_BULAN, MesejBulan[fasa & 7]);
					updateTray(&NotifyIconData, hwndDlg, tmdata, fasa, TRUE);
					break;

				case ID_FILE_EXIT:
					SendMessage(hwndDlg, WM_DESTROY, 0, 0);
					break;
			}
			break;

		case WM_CLOSE:
			ShowWindow(hwndDlg, SW_HIDE);
			return TRUE;

		case WM_DESTROY:
		case WM_ENDSESSION:
			Shell_NotifyIcon(NIM_DELETE, &NotifyIconData);
			if (g_hTrayIcon) {
				DestroyIcon(g_hTrayIcon);
				g_hTrayIcon = NULL;
			}
			if (g_hDialogIcon) {
				DestroyIcon(g_hDialogIcon);
				g_hDialogIcon = NULL;
			}
			DestroyMenu(hMenu);
			PostQuitMessage(0);
			break;
	}
	return 0;
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hinstPrev, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASS wc;
	MSG msg;
	HWND hwndMain;

	hInstance = hInst;
	InitCommonControls();

	memset(&wc, 0, sizeof(wc));
	wc.lpfnWndProc = DefDlgProc;
	wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = "TrayMoon";
	wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON5));
	wc.style = CS_DBLCLKS;

	/* Read registry settings with sensible defaults */
	ZeroMemory(&tmdata, sizeof(tmdata));
	tmdata.startup = 1;
	tmdata.malay = 1;
	tmdata.rotate = 0;
	tmdata.style = 1;    /* Smooth Silver Moon by default */
	tmdata.reminder = 1; /* Hijri date reminders enabled by default */

	if (read_registry(&tmdata) != ERROR_SUCCESS) {
		MessageBox(NULL, "TrayMoon have been started for the first time !", "TrayMoon Information", MB_ICONINFORMATION);
		if (save_registry(&tmdata) != ERROR_SUCCESS)
			MessageBox(NULL, "Can't init registry !", "TrayMoon Information", MB_ICONINFORMATION);
	}

	if (tmdata.startup)
		enable_autorun(TRUE, hInst);
	else
		enable_autorun(FALSE, hInst);

	if (!RegisterClass(&wc))
		return 0;

	/* Create modeless dialog window */
	if (!(hwndMain = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOG), NULL, (DLGPROC)DialogFunc)))
		return 0;

	/* Process Windows messages */
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!IsDialogMessage(hwndMain, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

int moon_phase(int y, int m, int d)
{
	/*
	  Calculates the moon phase (0-7), accurate to 1 segment.
	  0 => new moon.
	  4 => full moon.
	*/
	int c, e;
	double jd;
	int b;

	if (m < 3) {
		y--;
		m += 12;
	}
	++m;
	c = (int)(365.25 * y);
	e = (int)(30.6 * m);
	jd = c + e + d - 694039.09; /* jd is total days elapsed */
	jd /= 29.53;                /* divide by the moon cycle (29.53 days) */
	b = (int)jd;                /* take integer part of jd */
	jd -= b;                    /* leave fractional part */
	b = (int)(jd * 8 + 0.5);    /* scale fraction from 0-8 and round */
	b = b & 7;                  /* 0 and 8 are the same */
	return b;
}

int toHijrah(SYSTEMTIME hbTime, Haribulan *hijrah)
{
	int d, m, y;
	int jd, l, j, n;

	d = hbTime.wDay;
	m = hbTime.wMonth;
	y = hbTime.wYear;
	if ((y > 1582) || ((y == 1582) && (m > 10)) || ((y == 1582) && (m == 10) && (d > 14))) {
		jd = (int)((1461 * (y + 4800 + (int)((m - 14) / 12))) / 4) +
		     (int)((367 * (m - 2 - 12 * ((int)((m - 14) / 12)))) / 12) -
		     (int)((3 * ((int)((y + 4900 + (int)((m - 14) / 12)) / 100))) / 4) + d - 32075;
	} else {
		jd = 367 * y - (int)((7 * (y + 5001 + (int)((m - 9) / 7))) / 4) +
		     (int)((275 * m) / 9) + d + 1729777;
	}

	l = jd - 1948440 + 10632;
	n = (int)((l - 1) / 10631);
	l = l - 10631 * n + 354;
	j = ((int)((10985 - l) / 5316)) * ((int)((50 * l) / 17719)) +
	    ((int)(l / 5670)) * ((int)((43 * l) / 15238));
	l = l - ((int)((30 - j) / 15)) * ((int)((17719 * j) / 50)) -
	    ((int)(j / 16)) * ((int)((15238 * j) / 43)) + 29;
	m = (int)((24 * l) / 709);
	d = l - (int)((709 * m) / 24);
	y = 30 * n + j - 30;

	hijrah->wDay = d;
	hijrah->wMonth = m;
	hijrah->wYear = y;
	return 0;
}

int updateTray(NOTIFYICONDATA *NotifyIconData, HWND hwnd, struct tmData tmdata, int fasa, BOOL checkReminder)
{
	char strDate[64];
	char strTip[64];
	char strEvent[128];
	int daysUntil = 0;
	const HijriEvent *ev = findNextHijriEvent(hb, &daysUntil);
	const char *monthStr = getMonthName(hb.wMonth, tmdata.malay);
	HICON hNewIcon;

	wsprintf(strDate, "%d %s %d H", hb.wDay, monthStr, hb.wYear);
	SetDlgItemText(hwnd, IDC_HIJRAH, strDate);

	formatEventStatus(hb, tmdata.malay, strEvent, sizeof(strEvent));
	SetDlgItemText(hwnd, IDC_EVENT_INFO, strEvent);

	/* Build tray tooltip (up to 63 chars for standard NOTIFYICONDATA) */
	if (ev && daysUntil == 0) {
		wsprintf(strTip, "%s - %s", strDate, tmdata.malay ? ev->nameMalay : ev->nameIntl);
	} else {
		lstrcpyn(strTip, strDate, sizeof(strTip));
	}
	lstrcpyn(NotifyIconData->szTip, strTip, sizeof(NotifyIconData->szTip));

	/* Update tray icon with selected rotation & graphics style */
	hNewIcon = getMoonIcon(fasa, GetSystemMetrics(SM_CXSMICON), tmdata.rotate, tmdata.style);
	if (hNewIcon) {
		if (g_hTrayIcon) DestroyIcon(g_hTrayIcon);
		g_hTrayIcon = hNewIcon;
		NotifyIconData->hIcon = g_hTrayIcon;
	}

	NotifyIconData->uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;

	/*
	  Trigger balloon notification twice on an important Hijri date:
	  1st notification at 8:00 AM (08:00 - 11:59 window)
	  2nd notification at 12:00 PM (12:00+ window)
	*/
	if (checkReminder && tmdata.reminder && ev && daysUntil == 0) {
		SYSTEMTIME now;
		int todayKey = (hb.wYear * 10000) + (hb.wMonth * 100) + hb.wDay;
		BOOL shouldNotify = FALSE;
		const char *slotLabel = "";

		GetLocalTime(&now);
		if (now.wHour >= 8 && now.wHour < 12 && g_reminded8AMKey != todayKey) {
			g_reminded8AMKey = todayKey;
			shouldNotify = TRUE;
			slotLabel = "8:00 AM";
		} else if (now.wHour >= 12 && g_reminded12PMKey != todayKey) {
			g_reminded12PMKey = todayKey;
			shouldNotify = TRUE;
			slotLabel = "12:00 PM";
		}

		if (shouldNotify) {
			const char *evName = tmdata.malay ? ev->nameMalay : ev->nameIntl;
			NotifyIconData->uFlags |= NIF_INFO;
			NotifyIconData->dwInfoFlags = NIIF_INFO;
			wsprintf(NotifyIconData->szInfoTitle, "%s (%s)",
				tmdata.malay ? "Peringatan Tarikh Hijrah" : "Hijri Date Reminder",
				slotLabel);
			wsprintf(NotifyIconData->szInfo, "%s: %s (%s)",
				tmdata.malay ? "Hari ini" : "Today", evName, strDate);
		}
	}

	Shell_NotifyIcon(NIM_MODIFY, NotifyIconData);
	return 0;
}
