#define _WIN32_WINNT 0x0501
#include <Windows.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <tchar.h>
#include <windowsx.h>
#include <stdio.h>
#include "resource.h"

#pragma comment(lib,"shlwapi.lib")

#define IDM_APP_ABOUT 2000

#define TTI_ERROR_LARGE         6
#define ARRCOUNT(x) (sizeof(x)/sizeof(*(x)))

BOOL browseDlgInitOk=FALSE;

int CALLBACK ChooseDirectoryClassicCbk(HWND hwnd, UINT msg, LPARAM lParam, LPARAM lpData)
{
	switch (msg)
	{
	case BFFM_INITIALIZED:
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		browseDlgInitOk=TRUE;
		break;
	case BFFM_SELCHANGED:
		if(browseDlgInitOk)
			SendMessage(hwnd, BFFM_SETSELECTION, FALSE, lParam);
		break;
	}
	return 0;
}

BOOL ChooseDirectoryClassic(HWND hWndParent, TCHAR* fullPath, PCTSTR pcszDefaultPath, PCTSTR pcszInstruction)
{
	BROWSEINFO bi;
	LPITEMIDLIST pi;
	bi.hwndOwner = hWndParent;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = fullPath;
	bi.lpszTitle = pcszInstruction;
	bi.ulFlags = BIF_USENEWUI | BIF_UAHINT;
	bi.lParam = (LPARAM)pcszDefaultPath;
	//https://www.arclab.com/en/kb/cppmfc/select-folder-shbrowseforfolder.html
	bi.lpfn = ChooseDirectoryClassicCbk;
	bi.iImage = 0;
	pi = SHBrowseForFolder(&bi);

	if (pi)
	{
		if (SHGetPathFromIDList(pi, fullPath))
			return TRUE;
	}
	return FALSE;
}

void OnInitDialog(HWND hwnd)
{
	HMENU hSysMenu;
	CheckDlgButton(hwnd, IDC_CHECK_PROGRESS, BST_CHECKED);
	CheckDlgButton(hwnd, IDC_CHECK_LOCATE, BST_CHECKED);
	CheckDlgButton(hwnd, IDC_CHECK_ADMIN, BST_UNCHECKED);
	switch (__argc)
	{
	case 2:
		SetDlgItemText(hwnd, IDC_EDIT_SOURCE, __targv[1]);
		break;
	case 5:
		if (lstrcmpi(__targv[4], TEXT("admin")) == 0)
			CheckDlgButton(hwnd, IDC_CHECK_ADMIN, BST_CHECKED);
	case 4:
		SetDlgItemText(hwnd, IDC_EDIT_RUN_PATH, __targv[3]);
	case 3:
		SetDlgItemText(hwnd, IDC_EDIT_SOURCE, __targv[1]);
		ComboBox_SetText(GetDlgItem(hwnd, IDC_COMBO_RUN), __targv[2]);
		SendMessage(hwnd, WM_COMMAND, IDOK, 0);
		break;
	}
	hSysMenu = GetSystemMenu(hwnd, FALSE);
	AppendMenu(hSysMenu, MF_SEPARATOR, 0, 0);
	AppendMenu(hSysMenu, MF_STRING, IDM_APP_ABOUT, TEXT("关于(&A)..."));
}

BOOL IsExecutableFile(LPCTSTR filename)
{
	LPCTSTR ext[] = { TEXT(".exe"),TEXT(".com") };
	int i;
	for (i = 0; i < ARRCOUNT(ext); i++)
	{
		if (StrCmpI(_tcsrchr(filename, '.'), ext[i]) == 0)
			return TRUE;
	}
	return FALSE;
}

void ShowBalloonTip(HWND hwnd, int id, LPCWSTR msg,LPCWSTR title)
{
	EDITBALLOONTIP tip = { sizeof(EDITBALLOONTIP), title,msg,TTI_ERROR_LARGE };
	if(Edit_ShowBalloonTip(GetDlgItem(hwnd, id), &tip))
		MessageBeep(0);
	else
		MessageBoxW(hwnd,msg,title,MB_ICONERROR);
}

BOOL IsDirectoryEmpty(LPCTSTR path)
{
	WIN32_FIND_DATA fd;
	TCHAR checkpath[MAX_PATH] = TEXT("");
	HANDLE fh;
	int c;
	BOOL success;
	PathCombine(checkpath, path, TEXT("*"));
	fh = FindFirstFile(checkpath, &fd);
	if (fh == INVALID_HANDLE_VALUE)
		return TRUE;
	c = 0;
	for (success = TRUE; success; success = FindNextFile(fh, &fd))
		if (StrCmp(fd.cFileName, TEXT(".")) != 0 && StrCmp(fd.cFileName, TEXT("..")) != 0)
			c++;
	return c == 0;
}

int TCharToUtf8(LPCTSTR src,int slen,char *dst,int dlen)
{
	if(sizeof(*src)==2)
	{
		WCHAR*wsrc=(WCHAR*)src;
		if (dlen < WideCharToMultiByte(CP_UTF8, 0, wsrc, -1, NULL, 0, NULL, NULL))
			return 0;
		return WideCharToMultiByte(CP_UTF8, 0, wsrc, -1, dst, dlen, NULL, NULL);
	}
	else
	{
		CHAR*asrc=(CHAR*)src;
		strncpy(dst,asrc,slen);
		return slen+1;
	}
}

BOOL WriteProperty(FILE* fp, LPCSTR key, LPCTSTR val)
{
	char buf[400];
	int bc = TCharToUtf8(val, lstrlen(val), buf, 400);
	fprintf(fp, "%s=\"", key);
	if (fwrite(buf, 1, bc - 1, fp) == 0)
		return FALSE;
	fprintf(fp, "\"\n");
	return TRUE;
}

void OnOK(HWND hwnd)
{
	TCHAR buf[MAX_PATH] = TEXT("");
	TCHAR fullpath[MAX_PATH] = TEXT(""), archivepath[MAX_PATH];
	FILE *fp,*fpRun;
	TCHAR cmdline[400];
	LPCTSTR sfxFile;
	GetDlgItemText(hwnd, IDC_EDIT_SOURCE, buf, MAX_PATH - 1);
	if (lstrlen(buf) == 0)
	{
		ShowBalloonTip(hwnd, IDC_EDIT_SOURCE, L"请输入路径。",L"错误");
		return;
	}
	else if(IsDirectoryEmpty(buf))
	{
		ShowBalloonTip(hwnd, IDC_EDIT_SOURCE, L"这是一个无效路径或空文件夹，请选择其他路径。", L"错误");
		return;
	}
	GetFullPathName(buf, MAX_PATH - 1, fullpath, NULL);
	wsprintf(archivepath, TEXT("%s.exe"), fullpath);
	PathCombine(fullpath, fullpath, TEXT("*"));

	fp = _tfopen(TEXT("config.txt"), TEXT("wb+"));
	if (fp)
	{
		fprintf(fp, ";!@Install@!UTF-8!\n");
		if (GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_TITLE)))
		{
			GetDlgItemText(hwnd, IDC_EDIT_TITLE, buf, MAX_PATH - 1);
			WriteProperty(fp, "Title", buf);
		}
		if (GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_MESSAGE)))
		{
			GetDlgItemText(hwnd, IDC_EDIT_MESSAGE, buf, MAX_PATH - 1);
			WriteProperty(fp, "BeginPrompt", buf);
		}
		WriteProperty(fp, "Progress", IsDlgButtonChecked(hwnd, IDC_CHECK_PROGRESS) ? TEXT("yes") : TEXT("no"));
		ComboBox_GetText(GetDlgItem(hwnd, IDC_COMBO_RUN), buf, MAX_PATH - 1);
		if (lstrlen(buf))
		{
			if (IsExecutableFile(buf))
			{
				WriteProperty(fp, "RunProgram", buf);
				if (GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_RUN_PATH)))
				{
					GetDlgItemText(hwnd, IDC_EDIT_RUN_PATH, buf, MAX_PATH - 1);
					WriteProperty(fp, "Directory", buf);
				}
			}
			else
			{
				WriteProperty(fp, "ExecuteFile", buf);
				if (GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT_RUN_PATH)))
				{
					GetDlgItemText(hwnd, IDC_EDIT_RUN_PATH, buf, MAX_PATH - 1);
					WriteProperty(fp, "ExecuteParameters", buf);
				}
			}
		}
		fprintf(fp, ";!@InstallEnd@!\n");
		fclose(fp);
	}

	sfxFile = IsDlgButtonChecked(hwnd, IDC_CHECK_ADMIN) ? TEXT("7zSA.sfx") : TEXT("7zS.sfx");
	wsprintf(cmdline, TEXT("@echo off\n7za a archive.7z \"%s\"\ncopy/b/y %s+config.txt+archive.7z \"%s\"\ndel config.txt\ndel archive.7z"), fullpath, sfxFile, archivepath);
	if (IsDlgButtonChecked(hwnd, IDC_CHECK_LOCATE))
	{
		lstrcat(cmdline, TEXT("\nexplorer/select,\""));
		lstrcat(cmdline, archivepath);
		lstrcat(cmdline, TEXT("\""));
	}
	lstrcat(cmdline, TEXT("\ndel run.bat"));

	EndDialog(hwnd, 0);
	fpRun=_tfopen(TEXT("run.bat"),TEXT("w"));
	if(fpRun)
	{
		_ftprintf(fpRun,cmdline);
		fclose(fpRun);
		_tsystem(TEXT("run.bat"));
	}
}

void RefreshCombobox(HWND hwnd,LPCTSTR path)
{
	HWND hCombobox = GetDlgItem(hwnd, IDC_COMBO_RUN);
	WIN32_FIND_DATA fd;
	TCHAR checkpath[MAX_PATH] = TEXT("");
	HANDLE fh;
	BOOL success;
	BOOL foundExe = FALSE, haveFiles = FALSE;
	int i;
	while (ComboBox_GetCount(hCombobox))
		ComboBox_DeleteString(hCombobox, 0);

	PathCombine(checkpath, path, TEXT("*"));
	fh = FindFirstFile(checkpath, &fd);
	if (fh == INVALID_HANDLE_VALUE)
		return;
	for (success = TRUE; success; success = FindNextFile(fh, &fd))
	{
		TCHAR fullpath[MAX_PATH];
		PathCombine(fullpath, path, fd.cFileName);
		if (PathIsDirectory(fullpath)==FALSE)
			ComboBox_AddString(hCombobox, fd.cFileName);
	}

	for (i = ComboBox_GetCount(hCombobox) - 1; i >= 0; i--)
	{
		TCHAR filename[MAX_PATH];
		ComboBox_GetLBText(hCombobox, i, filename);
		if (IsExecutableFile(filename))
		{
			ComboBox_SetCurSel(hCombobox, i);
			foundExe = TRUE;
		}
		haveFiles = TRUE;
	}
	if (!foundExe && haveFiles)
		ComboBox_SetCurSel(hCombobox, 0);
}

void OnBrowse(HWND hwnd)
{
	TCHAR path[MAX_PATH] = TEXT("");
	browseDlgInitOk=FALSE;
	GetDlgItemText(hwnd, IDC_EDIT_SOURCE, path, MAX_PATH - 1);
	if (lstrlen(path) == 0)
		GetCurrentDirectory(MAX_PATH, path);
	if (ChooseDirectoryClassic(hwnd, path, path, TEXT("选择要压缩的文件目录。")))
		SetDlgItemText(hwnd, IDC_EDIT_SOURCE, path);
}

void OnEditSourceChange(HWND hwnd)
{
	TCHAR text[MAX_PATH] = TEXT("");
	GetDlgItemText(hwnd, IDC_EDIT_SOURCE, text, MAX_PATH - 1);
	RefreshCombobox(hwnd, text);
}

void OnComboRunChange(HWND hwnd)
{
	TCHAR buf[MAX_PATH];
	HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_RUN);
	int sel = ComboBox_GetCurSel(hCombo);
	if (sel == -1)
		ComboBox_GetText(GetDlgItem(hwnd, IDC_COMBO_RUN), buf, MAX_PATH - 1);
	else
		ComboBox_GetLBText(hCombo, sel, buf);
	if (IsExecutableFile(buf))
		SetDlgItemText(hwnd, IDC_STATIC_PARAMETER, TEXT("程序路径(&P)"));
	else
		SetDlgItemText(hwnd, IDC_STATIC_PARAMETER, TEXT("运行参数(&P)"));
}

INT_PTR CALLBACK DialogFunc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		OnInitDialog(hwnd);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
			EndDialog(hwnd, 1);
			break;
		case IDOK:
			OnOK(hwnd);
			break;
		case IDC_BUTTON_BROWSE:
			OnBrowse(hwnd);
			break;
		case IDC_EDIT_SOURCE:
			if (HIWORD(wParam) == EN_CHANGE)
				OnEditSourceChange(hwnd);
			break;
		case IDC_COMBO_RUN:
			switch (HIWORD(wParam))
			{
			case CBN_EDITCHANGE:
			case CBN_SELCHANGE:
				OnComboRunChange(hwnd);
				break;
			}
			break;
		}
		break;
	case WM_SYSCOMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_APP_ABOUT:
			MessageBox(hwnd, TEXT("7z 自解压格式压缩文件创建工具\n\n制作：lxfly2000\nhttps://github.com/lxfly2000/7zsfx"), TEXT("关于"), 0);
			break;
		}
		break;
	case WM_DROPFILES:
		{
			TCHAR filepath[MAX_PATH] = TEXT("");
			DragQueryFile((HDROP)wParam, 0, filepath, MAX_PATH);
			DragFinish((HDROP)wParam);
			SetDlgItemText(hwnd, IDC_EDIT_SOURCE, filepath);
			break;
		}
	}
	return 0;
}

int WINAPI _tWinMain(HINSTANCE hI, HINSTANCE hPrevI, LPTSTR param, int iShowWindow)
{
	return (int)DialogBox(hI, MAKEINTRESOURCE(IDD_DIALOG_MAIN), NULL, DialogFunc);
}
