/*
	registry.c - registry functions

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
#include "traymoon.h"


int read_registry( struct tmData *data)
{
	HKEY key1;
	char keyname[64];
	DWORD error;
	DWORD datasize = sizeof(int);
	DWORD len = 280;

	lstrcpy (keyname, "Software\\mypapit.net\\TrayMoon");
	error = RegOpenKeyEx( HKEY_CURRENT_USER,
		keyname,
		0,
		KEY_ALL_ACCESS,
		&key1 );

	if (error!=ERROR_SUCCESS)
		return error;

	RegQueryValueEx( key1, "appPath", NULL, NULL, (BYTE*) &data->appPath, &len );
	RegQueryValueEx( key1, "useMalay", NULL, NULL, (BYTE*) &data->malay, &datasize );
	//RegQueryValueEx( key1, "CascadeMenu", NULL, NULL, (BYTE*) &data->cascade, &datasize );
	RegQueryValueEx( key1, "Splash", NULL, NULL, (BYTE*) &data->splash, &datasize );
	RegQueryValueEx( key1, "Startup", NULL, NULL, (BYTE*) &data->startup, &datasize );

	RegCloseKey( key1 );




	return error;
}

int save_registry( struct tmData *data)
{
	HKEY key1;
	char keyname[64];
	DWORD error;


	lstrcpy (keyname, "Software\\mypapit.net\\TrayMoon");


	error = RegCreateKey( HKEY_CURRENT_USER, keyname, &key1 );


	if (error!=ERROR_SUCCESS)
		return error;


	RegSetValueEx( key1, "Startup", 0, REG_DWORD, (CONST BYTE*) &data->startup, sizeof( int ) );
	RegSetValueEx( key1, "useMalay", 0, REG_DWORD, (CONST BYTE*) &data->malay, sizeof( int ) );
//	RegQueryValueEx( key1, "useMalay", NULL, NULL, (BYTE*) &data->malay, &datasize );
	RegSetValueEx( key1, "Splash", 0, REG_DWORD, (CONST BYTE*) &data->splash, sizeof( int ) );
	RegSetValueEx( key1, "appPath", 0, REG_SZ, (CONST BYTE*) &data->appPath, 280 );


	RegCloseKey( key1 );


	return error;
}


int enable_autorun(int flag, HINSTANCE hInstance)
{
	HKEY key1;
	char keyname[256];
	int error;

	char szFileName[512];



	lstrcpy (keyname, "Software\\Microsoft\\Windows\\CurrentVersion\\Run");
	error = RegCreateKey( HKEY_CURRENT_USER, keyname, &key1 );
	if (error!=ERROR_SUCCESS)
		return error;


	if (flag==TRUE){
		GetModuleFileName(hInstance, szFileName, MAX_PATH);
		RegSetValueEx( key1, "TrayMoon(mypapit)", 0, REG_SZ, (CONST BYTE*) &szFileName, strlen(szFileName) );
	}

	else {
		lstrcpy(szFileName," ");
		RegSetValueEx( key1, "TrayMoon(mypapit)", 0, REG_SZ, (CONST BYTE*) &szFileName, strlen(szFileName) );
	}

	RegCloseKey( key1 );



	return error;
}
