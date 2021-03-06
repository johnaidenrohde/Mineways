/*
Copyright (c) 2010, Sean Kasun
All rights reserved.
Modified by Eric Haines, copyright (c) 2011.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "stdafx.h"
#include <CommDlg.h>
#include "Resource.h"
#include "../MinewaysMap/blockInfo.h"
#include "ColorSchemes.h"

#define NO_COLOR_ENTRY 0x000100FF
// magenta: tips off user that his color scheme is out of date
#define MISSING_COLOR_ENTRY 0xFF00FFFF

static wchar_t gLastItemSelected[255];

static int gLocalCountForNames=1;

#define COLORKEY L"Software\\Eric Haines\\Mineways\\ColorSchemes"

ColorManager::ColorManager()
{
    RegCreateKeyEx(HKEY_CURRENT_USER,COLORKEY,0,NULL,
        REG_OPTION_NON_VOLATILE,KEY_READ|KEY_WRITE,NULL,&key,NULL);
}
ColorManager::~ColorManager()
{
    RegCloseKey(key);
}

void ColorManager::Init(ColorScheme *cs)
{
	int i;
    for (i=0;i<NUM_BLOCKS;i++)
    {
        cs->colors[i]=blockColor(i);
    }
	for (int i=NUM_BLOCKS; i < 256; i++)
	{
		// fill the rest with almost-black; if we detect this color on loading
		// a color scheme in a place that should have been a normal color, then
		// we know the old color scheme is out of date and should be fixed on load.
		cs->colors[i]=NO_COLOR_ENTRY;
	}
}
void ColorManager::create(ColorScheme *cs)
{
    DWORD schemeId,len;
    len=sizeof(schemeId);
    long result=RegQueryValueEx(key,L"schemeId",NULL,NULL,(LPBYTE)&schemeId,&len);
    if (result==ERROR_FILE_NOT_FOUND)
        schemeId=0;
    schemeId++;
    RegSetValueEx(key,L"schemeId",NULL,REG_DWORD,(LPBYTE)&schemeId,sizeof(schemeId));
    ColorManager::Init(cs);
    cs->id=schemeId;
    save(cs);
}
int ColorManager::next(int id,ColorScheme *cs)
{
    TCHAR name[50];
    DWORD nameLen;
    DWORD type,csLen;
    for ( ;; )
    {
        nameLen=50;
        csLen=sizeof(ColorScheme);
        LONG result=RegEnumValue(key,id,name,&nameLen,NULL,&type,(LPBYTE)cs,&csLen);
        id++;
        if (result==ERROR_NO_MORE_ITEMS)
            return 0;
        if (type==REG_BINARY)
            return id;
    }
}
void ColorManager::save(ColorScheme *cs)
{
    wchar_t keyname[50];
    swprintf(keyname,50,L"scheme %d",cs->id);
    RegSetValueEx(key,keyname,NULL,REG_BINARY,(LPBYTE)cs,sizeof(ColorScheme));
}
void ColorManager::load(ColorScheme *cs)
{
    wchar_t keyname[50];
    swprintf(keyname,50,L"scheme %d",cs->id);
    DWORD csLen=sizeof(ColorScheme);
    RegQueryValueEx(key,keyname,NULL,NULL,(LPBYTE)cs,&csLen);

	// find out how many entries are invalid and fix these by shifting up
	int woolLoc = BLOCK_BLACK_WOOL;
	// Find black wool entry (for backwards compatibility)
	unsigned int color=blockColor(BLOCK_BLACK_WOOL);

	// go back through list until wool is found. Don't go back past say snow
	while ( woolLoc > 78 && cs->colors[woolLoc] != color )
	{
		woolLoc--;
	}

	// woolLoc is the location of black wool in the saved color scheme
	if ( woolLoc < BLOCK_BLACK_WOOL && woolLoc > 78 )
	{
		// Mind the gap! Move the 16 wools up some slots, fill in these slots
		// with missing entries
		int gap = BLOCK_BLACK_WOOL - woolLoc;
		int i;
		// set unknown block attribute, one past wool
		cs->colors[NUM_BLOCKS-1] = blockColor(NUM_BLOCKS-1);
		for ( i = 0; i > -16; i-- )
		{
			// correct black wool set to where black wool is currently, on back down
			cs->colors[woolLoc+gap+i] = cs->colors[woolLoc+i];
		}
		for ( i = 0; i < gap; i++ )
		{
			// set where white wool was, on up, to missing color
			cs->colors[woolLoc-15+i] = blockColor(woolLoc-15+i);
		}
		save(cs);
	}
}
void ColorManager::remove(int id)
{
    wchar_t keyname[50];
    swprintf(keyname,50,L"scheme %d",id);
    RegDeleteValue(key,keyname);
}
unsigned int ColorManager::blockColor(int type)
{
	unsigned int color=gBlockDefinitions[type].color;
	unsigned char r,g,b,a;
	r=(unsigned char)(color>>16);
	g=(unsigned char)(color>>8);
	b=(unsigned char)(color);
	float alpha=gBlockDefinitions[type].alpha;
	// we used to unmultiply. Now we store the unmultiplied color in gBlockDefinitions
	//r=(unsigned char)(r/alpha);
	//g=(unsigned char)(g/alpha);
	//b=(unsigned char)(b/alpha);
	a=(unsigned char)(alpha*255);
	color=(r<<24)|(g<<16)|(b<<8)|a;

	return color;
}

INT_PTR CALLBACK ColorSchemes(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK ColorSchemeEdit(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam);

void doColorSchemes(HINSTANCE hInst,HWND hWnd)
{
    DialogBox(hInst,MAKEINTRESOURCE(IDD_COLORSCHEMES),hWnd,ColorSchemes);
}

wchar_t *getSelectedColorScheme()
{
    return gLastItemSelected;
}

static void validateButtons(HWND hDlg);
static ColorScheme curCS;

INT_PTR CALLBACK ColorSchemes(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam)
{
    HWND list;
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        {
            ColorManager cm;
            list=GetDlgItem(hDlg,IDC_SCHEMELIST);
            ColorScheme cs;
            int id=cm.next(0,&cs);
            while (id)
            {
                int pos=(int)SendMessage(list,LB_ADDSTRING,0,(LPARAM)cs.name);
                SendMessage(list,LB_SETITEMDATA,pos,cs.id);
                id=cm.next(id,&cs);
            }
            validateButtons(hDlg);
            wcsncpy_s(gLastItemSelected,255,L"",1);
        }
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_SCHEMELIST:
            switch (HIWORD(wParam))
            {
            case LBN_SELCHANGE:
            case LBN_SELCANCEL:
                validateButtons(hDlg);
                break;
            }
            break;
        case IDC_EDIT:
            {
                list=GetDlgItem(hDlg,IDC_SCHEMELIST);
                int item=(int)SendMessage(list,LB_GETCURSEL,0,0);
                curCS.id=(int)SendMessage(list,LB_GETITEMDATA,item,0);
                ColorManager cm;
                cm.load(&curCS);
                DialogBox(NULL,MAKEINTRESOURCE(IDD_COLORSCHEME),hDlg,ColorSchemeEdit);
                cm.save(&curCS);
                SendMessage(list,LB_DELETESTRING,item,0);
                int pos=(int)SendMessage(list,LB_INSERTSTRING,item,(LPARAM)curCS.name);
                SendMessage(list,LB_SETITEMDATA,pos,curCS.id);
                // keep the item selected
                SendMessage(list,LB_SETCURSEL,item,0);
            }
            break;
        case IDC_ADD:
            {
                ColorManager cm;
                ColorScheme cs;
                // goofy, all color schemes use the same name by default.
                swprintf_s(cs.name,255,L"Color Scheme %d",gLocalCountForNames);
                cm.create(&cs);
                list=GetDlgItem(hDlg,IDC_SCHEMELIST);
                int pos=(int)SendMessage(list,LB_ADDSTRING,0,(LPARAM)cs.name);
                SendMessage(list,LB_SETITEMDATA,pos,cs.id);
                validateButtons(hDlg);
            }
            break;
        case IDC_REMOVE:
            {
                ColorManager cm;
                list=GetDlgItem(hDlg,IDC_SCHEMELIST);
                int item=(int)SendMessage(list,LB_GETCURSEL,0,0);
                if (item!=LB_ERR)
                {
                    int id=(int)SendMessage(list,LB_GETITEMDATA,item,0);
                    cm.remove(id);
                    SendMessage(list,LB_DELETESTRING,item,0);
                }
                validateButtons(hDlg);
            }
            break;
        case IDOK:
            // TODO: add an OK button. It's unnerving.
        case IDCANCEL:
            {
                list=GetDlgItem(hDlg,IDC_SCHEMELIST);
                int item =(int)SendMessage(list,LB_GETCURSEL,0,0);
                if ( item == LB_ERR )
                {
                    wcsncpy_s(gLastItemSelected,255,L"",1);
                }
                else
                {
                    curCS.id=(int)SendMessage(list,LB_GETITEMDATA,item,0);
                    ColorManager cm;
                    cm.load(&curCS);
                    wcsncpy_s(gLastItemSelected,255,curCS.name,255);
                }
            }
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
static void validateButtons(HWND hDlg)
{
    HWND edit=GetDlgItem(hDlg,IDC_EDIT);
    HWND remove=GetDlgItem(hDlg,IDC_REMOVE);
    HWND list=GetDlgItem(hDlg,IDC_SCHEMELIST);
    int item=(int)SendMessage(list,LB_GETCURSEL,0,0);
    if (item==LB_ERR)
    {
        EnableWindow(edit,FALSE);
        EnableWindow(remove,FALSE);
    }
    else
    {
        EnableWindow(edit,TRUE);
        EnableWindow(remove,TRUE);
    }
}

INT_PTR CALLBACK ColorSchemeEdit(HWND hDlg,UINT message,WPARAM wParam,LPARAM lParam)
{
    static int curSel;
    static wchar_t row[255];
    NMLVDISPINFO *info;
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        {
            SetDlgItemText(hDlg,IDC_SCHEMENAME,curCS.name);
            
            HWND ctl=GetDlgItem(hDlg,IDC_CURCOLOR);
            EnableWindow(ctl,FALSE);
            ctl=GetDlgItem(hDlg,IDC_CURALPHA);
            EnableWindow(ctl,FALSE);

            HWND lv=GetDlgItem(hDlg,IDC_COLORLIST);
            ListView_SetExtendedListViewStyle(lv,LVS_EX_FULLROWSELECT);
            
            LVCOLUMN lvc;
            lvc.mask=LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;
            for (int i=0;i<4;i++)
            {
                lvc.iSubItem=i;
                switch (i)
                {
                case 0:
                    lvc.pszText=L"Id";
                    lvc.cx=40;
                    break;
                case 1:
                    lvc.pszText=L"Name";
                    lvc.cx=200;
                    break;
                case 2:
                    lvc.pszText=L"Color";
                    lvc.cx=80;
                    break;
                case 3:
                    lvc.pszText=L"Alpha";
                    lvc.cx=50;
                    break;
                }
                lvc.fmt=LVCFMT_LEFT;
                ListView_InsertColumn(lv,i,&lvc);
            }
            LVITEM item;
            item.mask=LVIF_TEXT|LVIF_STATE;
            item.iSubItem=0;
            item.state=0;
            item.stateMask=0;
            item.pszText=LPSTR_TEXTCALLBACK;
            for (int i=0;i<NUM_BLOCKS;i++)
            {
                item.iItem=i;
                ListView_InsertItem(lv,&item);
            }
        }
        return (INT_PTR)TRUE;
    case WM_NOTIFY:
        switch (((LPNMHDR)lParam)->code)
        {
        case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW item=(LPNMLISTVIEW)lParam;
                if (item->uNewState&LVIS_SELECTED)
                {
                    curSel=item->iItem;
                    swprintf(row,255,L"#%06x",curCS.colors[item->iItem]>>8);
                    SetDlgItemText(hDlg,IDC_CURCOLOR,row);
                    swprintf(row,255,L"%d",curCS.colors[item->iItem]&0xff);
                    SetDlgItemText(hDlg,IDC_CURALPHA,row);
                    HWND ctl=GetDlgItem(hDlg,IDC_CURCOLOR);
                    EnableWindow(ctl,TRUE);
                    ctl=GetDlgItem(hDlg,IDC_CURALPHA);
                    EnableWindow(ctl,TRUE);
                }
            }
            break;
        case LVN_GETDISPINFO:
            info=(NMLVDISPINFO*)lParam;
            switch (info->item.iSubItem)
            {
            case 0:
                swprintf(row,255,L"%d.",info->item.iItem);
                break;
            case 1:
                swprintf(row,255,L"%S",gBlockDefinitions[info->item.iItem].name);
                break;
            case 2:
                swprintf(row,255,L"#%06x",curCS.colors[info->item.iItem]>>8);
                break;
            case 3:
                swprintf(row,255,L"%d",curCS.colors[info->item.iItem]&0xff);
                break;
            }
            info->item.pszText=row;
            break;
        }
        break;
    case WM_COMMAND:
        switch (HIWORD(wParam))
        {
        case EN_CHANGE:
            switch (LOWORD(wParam))
            {
            case IDC_SCHEMENAME:
                GetDlgItemText(hDlg,IDC_SCHEMENAME,curCS.name,255);
                break;
            case IDC_CURCOLOR:
                {
                    HWND lv=GetDlgItem(hDlg,IDC_COLORLIST);
                    GetDlgItemText(hDlg,IDC_CURCOLOR,row,255);
                    unsigned int color=0;
                    for (int i=0;row[i];i++)
                    {
                        if (row[i]>='0' && row[i]<='9')
                        {
                            color<<=4;
                            color|=row[i]-'0';
                        }
                        if (row[i]>='a' && row[i]<='f')
                        {
                            color<<=4;
                            color|=row[i]+10-'a';
                        }
                        if (row[i]>='A' && row[i]<='F')
                        {
                            color<<=4;
                            color|=row[i]+10-'A';
                        }
                    }
                    curCS.colors[curSel]&=0xff;
                    curCS.colors[curSel]|=color<<8;
                    ListView_RedrawItems(lv,curSel,curSel);
                }
                break;
            case IDC_CURALPHA:
                {
                    HWND lv=GetDlgItem(hDlg,IDC_COLORLIST);
                    curCS.colors[curSel]&=~0xff;
                    curCS.colors[curSel]|=GetDlgItemInt(hDlg,IDC_CURALPHA,NULL,FALSE)&0xff;
                    ListView_RedrawItems(lv,curSel,curSel);
                }
            }
            break;
        }
        switch (LOWORD(wParam))
        {
        case IDC_HIDE_ALL_BLOCKS:
            {
                // set all blocks' alphas to zero. Useful for printing out just one material
                for (int i=0;i<NUM_BLOCKS;i++)
                {
                    curCS.colors[i] &= ~0xff;
                }
                HWND lv=GetDlgItem(hDlg,IDC_COLORLIST);
                ListView_RedrawItems(lv,0,NUM_BLOCKS-1);
            }
            break;
        case IDCANCEL:
        case IDOK:
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
        }
        break;
    }
    return (INT_PTR)FALSE;
}