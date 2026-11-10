// This file is part of Notepad++ project
// Copyright (C)2026 Arthur <Rosabis>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include "Common.h"

#include <string>
#include <vector>

// Down-level implementation of the open/save dialogs, built on the classic
// GetOpenFileName/GetSaveFileName common dialogs.
//
// CustomFileDialog is based on IFileDialog (the "Common Item Dialog"), which only exists
// from Windows Vista on.  Notepad++ XP edition therefore keeps this Win32 fallback and
// uses it whenever the running system is older than Vista.
class LegacyFileDialog
{
public:
	explicit LegacyFileDialog(HWND hwnd);
	~LegacyFileDialog() = default;

	LegacyFileDialog(const LegacyFileDialog&) = delete;
	LegacyFileDialog& operator=(const LegacyFileDialog&) = delete;

	void setTitle(const wchar_t* title);
	void setExtFilter(const wchar_t* extText, const wchar_t* exts);
	void setDefExt(const wchar_t* ext);
	void setDefFileName(const wchar_t* fn);
	void setFolder(const wchar_t* folder);
	void setCheckbox(const wchar_t* text, bool isActive = true);
	void setExtIndex(int extTypeIndex);
	void setSaveAsCopy(bool isSavingAsCopy);
	bool getOpenTheCopyAfterSaveAsCopy() const { return _openTheCopyAfterSaveAsCopy; }

	void enableFileTypeCheckbox(const std::wstring& text, bool value);
	bool getFileTypeCheckboxValue() const { return _fileTypeCheckboxValue; }

	// Empty string is not a valid file name and may signal that the dialog was canceled.
	std::wstring doSaveDlg();
	std::wstring pickFolder();
	std::wstring doOpenSingleFileDlg();
	std::vector<std::wstring> doOpenMultiFilesDlg();

	bool getCheckboxState() const { return _checkboxState; }
	bool isReadOnly() const { return _isReadOnly; }

private:
	static UINT_PTR CALLBACK ofnHookProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static int CALLBACK browseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);

	BOOL run(HWND hWnd, UINT uMsg, LPARAM lParam);
	void createExtraControls(HWND hDlg);

	void buildFilterString();
	void setupOfn(DWORD extraFlags);
	bool showOpenDlg();
	bool showSaveDlg();

	HWND _hwndOwner = nullptr;

	std::wstring _title;
	std::wstring _defExt;
	std::wstring _initialFolder;
	std::wstring _initialFileName;

	std::vector<std::pair<std::wstring, std::wstring>> _filters;
	int _extTypeIndex = -1;

	std::wstring _checkboxLabel;
	bool _checkboxState = false;
	std::wstring _fileTypeCheckboxLabel;
	bool _fileTypeCheckboxValue = false;

	bool _isSavingAsCopy = false;
	bool _openTheCopyAfterSaveAsCopy = false;
	bool _isReadOnly = false;

	std::wstring _filterString;		// double null terminated OPENFILENAME filter
	std::vector<wchar_t> _fileBuffer;
	OPENFILENAME _ofn{};

	static LegacyFileDialog* s_current;	// the instance the current hook proc belongs to
};
