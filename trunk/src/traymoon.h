/*
	traymoon.h - main header definition file

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

	http://traymoon.googlecode.com/
	http://blog.mypapit.net/


*/
struct tmData{
	int splash;
	int malay;
	int startup;
	char appPath[280];
};

int read_registry( struct tmData *data);
int save_registry( struct tmData *data);
int enable_autorun(int flag, HINSTANCE hInstance);
