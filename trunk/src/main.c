/*
	main.c - main module

	Copyright (C) 2004 Mohammad Hafiz bin Ismail

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
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include "resource.h"
#include "traymoon.h"
typedef struct {
	int wYear;
	int wMonth;
	int wDay;
} Haribulan;
static char **strMonth;

static struct tmData tmdata;
static char *BulanMelayu[]={"Muharam","Safar","RabiulAwal","RabiulAkhir","JamadilAwal","JamadilAkhir",
						"Rejab","Syaaban","Ramadan","Syawal","Zulkaedah","Zulhijjah"};
static char *BulanInternational[]={"Muharam","Safar","Rabiul I","Rabiul II","Jumada I","Jumada II",
						"Rajab","Sha'ban","Ramadan","Shawwal","Dhu'l-Qa'dah","Dhu'l-Hijja"};

static Haribulan hb;
static HINSTANCE hInstance;

int toHijrah(SYSTEMTIME hb,Haribulan *hijrah);
int moon_phase(int y, int m, int d);

int updateTray(NOTIFYICONDATA *NotifyIconData, HWND hwnd,struct tmData tmdata);
static BOOL CALLBACK DialogSetting (HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

VOID CALLBACK periodicUpdate(HWND hwnd, UINT msg, UINT idEvent, DWORD dwTime)
	{
		SendMessage(hwnd,WM_COMMAND,ID_FILE_REFRESH,0);

	}

static BOOL CALLBACK DialogFunc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{

	static HMENU hMenu,hSubMenu,hCDROMMenu;
	static SYSTEMTIME systime;
	static MENUITEMINFO menuitem;
	int idEvent=90;
	static char ayyat[64];
static NOTIFYICONDATA NotifyIconData={0};

	static char *MesejBulan[] = {"New Moon (Bulan Baru)", "Waxing crescent (Bulan Mengambang Suku)", "1st Quarter moon (Bulan Mengambang Separuh)",
	"Waxing Gibbous (Bulan Mengambang 3/4)", "Full Moon (Bulan Penuh)", "Waning Gibbous (Bulan Menurun 3/4)", "3rd Quarter moon (Bulan Menurun Separuh)","Waning crescent (Bulan Menurun Suku)"};
	POINT MouseCoordinates;
	RECT rc;
	int i=0;
	static int fasa;






	switch (msg) {

		case WM_INITDIALOG:
			SetTimer(hwndDlg,idEvent,21600000L,periodicUpdate); //updates

			if (tmdata.malay) {
				strMonth=BulanMelayu;
			} else {
				strMonth=BulanInternational;
			}


			/* center dialog */
			SendMessage(hwndDlg, WM_SETICON, 905,
                        (LPARAM) LoadIcon(GetModuleHandle(0),
                                          MAKEINTRESOURCE(IDD_DIALOG)));
			GetWindowRect(hwndDlg, &rc);
			SetWindowPos(hwndDlg, NULL, (  ( GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left) ) / 2  ), (  ( GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top) ) / 2  ), 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);


			/* buat tray menu*/
			hMenu = LoadMenu (hInstance, MAKEINTRESOURCE (IDR_MENU));

			/*get system time and moon phase*/
			GetSystemTime(&systime);
			fasa=moon_phase(systime.wYear,systime.wMonth,systime.wDay);
			//fasa=moon_phase(2004,11,9);
			fasa+=900;
			//wsprintf(ayyat,"%d",fasa);
			toHijrah(systime,&hb);
			wsprintf(ayyat,"%d %s %d",hb.wDay,strMonth[hb.wMonth],hb.wYear);
			SetDlgItemText(hwndDlg,IDC_HIJRAH,ayyat);
			SetDlgItemText(hwndDlg,IDC_BULAN,MesejBulan[fasa-900]);




			/* letak icon dalam system tray*/
			NotifyIconData.cbSize = sizeof (NOTIFYICONDATA);
    		NotifyIconData.hWnd = hwndDlg;
    		NotifyIconData.uID = 0;
    		NotifyIconData.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE| 0x00000010;
    		NotifyIconData.uCallbackMessage = WM_USER; // tray events will generate WM_USER event
		    NotifyIconData.hIcon = LoadImage (hInstance, MAKEINTRESOURCE (fasa), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR); // load 16 x 16 pixels icon
			lstrcpy (NotifyIconData.szTip, ayyat); // max 64 characters
			Shell_NotifyIcon (NIM_ADD, &NotifyIconData);

		case WM_USER:

			switch (lParam)	{

				case WM_RBUTTONUP:
					/*workaround utk menu*/
					SetForegroundWindow(hwndDlg);
					GetCursorPos (&MouseCoordinates);
					TrackPopupMenu (GetSubMenu (hMenu, 0), TPM_RIGHTALIGN | TPM_LEFTBUTTON, MouseCoordinates.x, MouseCoordinates.y, 0, hwndDlg, NULL);
					PostMessage(hwndDlg, WM_NULL, 0, 0);
					break;

			case WM_LBUTTONDBLCLK:
			   SendMessage(hwndDlg,WM_COMMAND,ID_FILE_ABOUT,0);
			break;


			}
			break;


		case WM_COMMAND:
			switch (LOWORD(wParam)) {

				case IDOK:
					/*dapatkan setting,write ke registry*/
					 tmdata.startup=(int) SendDlgItemMessage(hwndDlg,IDC_STARTUP,BM_GETCHECK,0,0);
					 tmdata.malay=(int) SendDlgItemMessage(hwndDlg,IDC_MALAY,BM_GETCHECK,0,0);
					/*save data tadi*/
					 save_registry(&tmdata);
					ShowWindow (hwndDlg, SW_HIDE);
					updateTray(&NotifyIconData,hwndDlg,tmdata);
					return 1;

				case ID_FILE_ABOUT:	/*about*/
					/*init setting box - in case ada perubahan*/
					SendDlgItemMessage(hwndDlg,ID_IMEJBULAN,STM_SETICON,(WPARAM) LoadIcon(hInstance,MAKEINTRESOURCE(fasa)),0);
					SendDlgItemMessage(hwndDlg,IDC_STARTUP,BM_SETCHECK,tmdata.startup,0);
					SendDlgItemMessage(hwndDlg,IDC_MALAY,BM_SETCHECK,tmdata.malay,0);
					ShowWindow (hwndDlg, SW_SHOW);


					break;
				case ID_FILE_REFRESH:
					GetSystemTime(&systime);
					fasa=moon_phase(systime.wYear,systime.wMonth,systime.wDay);
					fasa+=900;
					toHijrah(systime,&hb);
					wsprintf(ayyat,"%d %s %d",hb.wDay,strMonth[hb.wMonth],hb.wYear);
					SetDlgItemText(hwndDlg,IDC_HIJRAH,ayyat);
					SetDlgItemText(hwndDlg,IDC_BULAN,MesejBulan[fasa-900]);

					NotifyIconData.hIcon = LoadImage (hInstance, MAKEINTRESOURCE (fasa), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR); // load 16 x 16 pixels icon
					updateTray(&NotifyIconData,hwndDlg,tmdata);



				break;
				case ID_FILE_EXIT:
					SendMessage(hwndDlg,WM_DESTROY,0,0);
				break;

			}
		break;




		case WM_CLOSE: /*ignore close message*/
			//SendMessage(hwndDlg,WM_COMMAND,IDOK,0);
			return TRUE;
			break;



		case WM_DESTROY:
		case WM_ENDSESSION: /*log-off atau shutdown, clean up mess*/
			Shell_NotifyIcon (NIM_DELETE, &NotifyIconData);
			DestroyMenu (hMenu);
			PostQuitMessage (0);
			break;

	}
	return 0;
}



int APIENTRY WinMain (HINSTANCE hInst, HINSTANCE hinstPrev, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASS wc;
	MSG msg;
	HWND hwndMain;
	int idEvent = 90;


	hInstance = hInst;
	InitCommonControls();




	memset (&wc, 0, sizeof (wc));
	wc.lpfnWndProc = DefDlgProc;
	wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszClassName = "TrayMoon";
	wc.hIcon = LoadIcon (hInst, MAKEINTRESOURCE (IDI_ICON5));
	wc.style = CS_DBLCLKS;


	/*read registry !*/
	ZeroMemory(&tmdata,sizeof(tmdata));
	tmdata.startup=1;
	tmdata.malay=1;
	if (read_registry(&tmdata)!=ERROR_SUCCESS)	{
		MessageBox(NULL,"TrayMoon have been started for the first time !","TrayMoon Information",MB_ICONINFORMATION);
		if (save_registry( &tmdata)!=ERROR_SUCCESS)
			MessageBox(NULL,"Can't init registry !","TrayMoon Information",MB_ICONINFORMATION);
	}

	if (tmdata.startup)  /*kalu autorun flag on*/
		enable_autorun(1,hInst);
	else
		enable_autorun(0,hInst);

	if (!RegisterClass (&wc))
		return 0;


	/*attempting utk read registry !
	  ni guna utk simpan settings dlm pc !*/


	// create modeless dialog window
    if (!(hwndMain = CreateDialog (hInst, MAKEINTRESOURCE (IDD_DIALOG), NULL, (DLGPROC) DialogFunc)))
		return 0;



	// process Windows messages
	while (GetMessage (&msg, NULL, 0, 0)) {
		if (IsDialogMessage (hwndMain, &msg)) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}

	return (int) msg.wParam;
}


int moon_phase(int y, int m, int d)
{
    /*
      calculates the moon phase (0-7), accurate to 1 segment.
      0 = > new moon.
      4 => full moon.
      */

    int c,e;
    double jd;
    int b;

    if (m < 3) {
        y--;
        m += 12;
    }
    ++m;
    c = (int) (365.25*y);
    e = (int) (30.6*m);
    jd = c+e+d-694039.09;  /* jd is total days elapsed */
    jd /= 29.53;           /* divide by the moon cycle (29.53 days) */
    b = (int) jd;		   /* int(jd) -> b, take integer part of jd */
    jd -= b;		   /* subtract integer part to leave fractional part of original jd */
    b = (int) (jd*8 + 0.5);	   /* scale fraction from 0-8 and round by adding 0.5 */
    b = b & 7;		   /* 0 and 8 are the same so turn 8 into 0 */
    return b;
}

int toHijrah(SYSTEMTIME hb,Haribulan *hijrah) {
#define intPart ((int)(x))

	int d,m,y;
	int jd,l,j,n;

	d=hb.wDay;
	m=hb.wMonth;
	y=hb.wYear;
	if ((y>1582)||((y==1582)&&(m>10))||((y==1582)&&(m==10)&&(d>14))) {
		jd=(int)((1461*(y+4800+(int)((m-14)/12)))/4)+(int)((367*(m-2-12*((int)((m-14)/12))))/12)-
	    (int)( (3* ((int)(  (y+4900+    (int)( (m-14)/12)     )/100)    )   ) /4)+d-32075;
						}
						else
						{
						jd = 367*y-(int)((7*(y+5001+(int)((m-9)/7)))/4)+(int)((275*m)/9)+d+1729777;
						}
//					arg.JD.value=jd
//					arg.wd.value=weekDay(jd%7)
					l=jd-1948440+10632;
					n=(int)((l-1)/10631);
					l=l-10631*n+354;
					j=((int)((10985-l)/5316))*((int)((50*l)/17719))+((int)(l/5670))*((int)((43*l)/15238));
					l=l-((int)((30-j)/15))*((int)((17719*j)/50))-((int)(j/16))*((int)((15238*j)/43))+29;
					m=(int)((24*l)/709);
					d=l-(int)((709*m)/24);
					y=30*n+j-30;

	hijrah->wDay=d;
	hijrah->wMonth=m;
	hijrah->wYear=y;
	return 0;
}

int updateTray(NOTIFYICONDATA *NotifyIconData, HWND hwnd,struct tmData tmdata)
{
	char strDate[64];
	if (tmdata.malay)
		wsprintf(strDate,"%d %s %d",hb.wDay,BulanMelayu[hb.wMonth],hb.wYear);
	else
		wsprintf(strDate,"%d %s %d",hb.wDay,BulanInternational[hb.wMonth],hb.wYear);
	SetDlgItemText(hwnd,IDC_HIJRAH,strDate);
	lstrcpy (NotifyIconData->szTip, strDate); // max 64 characters
	Shell_NotifyIcon (NIM_MODIFY, NotifyIconData);

	return 0;

}
