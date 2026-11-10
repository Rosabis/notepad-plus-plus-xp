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


#include "LegacyFileDialog.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>

#include <algorithm>
#include <cwchar>

namespace
{
	// Control ids for the checkboxes we inject into the common dialog.
	// They must not collide with the ids used by the dialog itself
	// (edt1 = 0x480, cmb1 = 0x470, stc3 = 0x442, ...).
	constexpr int legacyCheckboxId = 0x4001;
	constexpr int legacyFileTypeCheckboxId = 0x4002;

	constexpr int checkboxHeight = 18;
	constexpr int checkboxGap = 4;

	// Enough room for a multi-selection of a few hundred files.
	constexpr size_t fileBufferLength = 32 * 1024;

	// Backs up the current directory in constructor and restores it in destructor.
	// This is needed in case the dialog changes the current directory.
	class CurrentDirBackup
	{
	public:
		CurrentDirBackup()
		{
			::GetCurrentDirectory(MAX_PATH, _dir);
		}

		~CurrentDirBackup()
		{
			::SetCurrentDirectory(_dir);
		}
	private:
		wchar_t _dir[MAX_PATH]{};
	};
}

LegacyFileDialog* LegacyFileDialog::s_current = nullptr;

LegacyFileDialog::LegacyFileDialog(HWND hwnd) : _hwndOwner{ hwnd }
{
}

void LegacyFileDialog::setTitle(const wchar_t* title)
{
	_title = (title != nullptr) ? title : L"";
}

void LegacyFileDialog::setExtFilter(const wchar_t* extText, const wchar_t* exts)
{
	// Add an asterisk before each dot in file patterns, like the Vista dialog does
	std::wstring newExts{ (exts != nullptr) ? exts : L"" };
	for (size_t pos = 0; pos < newExts.size(); ++pos)
	{
		pos = newExts.find(L'.', pos);
		if (pos == std::wstring::npos)
			break;
		if (pos == 0 || newExts[pos - 1] != L'*')
		{
			newExts.insert(pos, 1, L'*');
			++pos;
		}
	}

	if (newExts.empty())
		newExts = L"*.*";

	_filters.emplace_back((extText != nullptr) ? extText : L"", newExts);
}

void LegacyFileDialog::setDefExt(const wchar_t* ext)
{
	_defExt = (ext != nullptr) ? ext : L"";
}

void LegacyFileDialog::setDefFileName(const wchar_t* fn)
{
	_initialFileName = (fn != nullptr) ? fn : L"";
}

void LegacyFileDialog::setFolder(const wchar_t* folder)
{
	_initialFolder = (folder != nullptr) ? folder : L"";
}

void LegacyFileDialog::setCheckbox(const wchar_t* text, bool isActive)
{
	_checkboxLabel = (text != nullptr) ? text : L"";
	_checkboxState = isActive;
}

void LegacyFileDialog::setExtIndex(int extTypeIndex)
{
	_extTypeIndex = extTypeIndex;
}

void LegacyFileDialog::setSaveAsCopy(bool isSavingAsCopy)
{
	_isSavingAsCopy = isSavingAsCopy;
}

void LegacyFileDialog::enableFileTypeCheckbox(const std::wstring& text, bool value)
{
	_fileTypeCheckboxLabel = text;
	_fileTypeCheckboxValue = value;
}

///////////////////////////////////////////////////////////////////////////////

void LegacyFileDialog::buildFilterString()
{
	_filterString.clear();
	for (const auto& filter : _filters)
	{
		_filterString += filter.first;
		_filterString += L" (";
		_filterString += filter.second;
		_filterString += L")";
		_filterString.push_back(L'\0');
		_filterString += filter.second;
		_filterString.push_back(L'\0');
	}
	_filterString.push_back(L'\0');	// the filter string is double null terminated
}

void LegacyFileDialog::setupOfn(DWORD extraFlags)
{
	_fileBuffer.assign(fileBufferLength, L'\0');
	if (!_initialFileName.empty())
	{
		const size_t count = std::min(_initialFileName.size(), fileBufferLength - 1);
		std::copy_n(_initialFileName.cbegin(), count, _fileBuffer.begin());
	}

	buildFilterString();

	_ofn = {};
	_ofn.lStructSize = sizeof(OPENFILENAME);
	_ofn.hwndOwner = _hwndOwner;
	_ofn.lpstrFilter = _filters.empty() ? nullptr : _filterString.c_str();
	_ofn.nFilterIndex = (_extTypeIndex >= 0) ? static_cast<DWORD>(_extTypeIndex + 1) : 1;
	_ofn.lpstrFile = _fileBuffer.data();
	_ofn.nMaxFile = static_cast<DWORD>(_fileBuffer.size());
	_ofn.lpstrInitialDir = _initialFolder.empty() ? nullptr : _initialFolder.c_str();
	_ofn.lpstrTitle = _title.empty() ? nullptr : _title.c_str();
	_ofn.lpstrDefExt = _defExt.empty() ? nullptr : _defExt.c_str();
	_ofn.lpfnHook = LegacyFileDialog::ofnHookProc;
	_ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_LONGNAMES | OFN_ENABLESIZING | OFN_ENABLEHOOK | extraFlags;
}

bool LegacyFileDialog::showOpenDlg()
{
	CurrentDirBackup backup;

	s_current = this;
	_isReadOnly = false;
	const BOOL res = ::GetOpenFileName(&_ofn);
	s_current = nullptr;

	if (res != FALSE)
		_isReadOnly = (_ofn.Flags & OFN_READONLY) != 0;

	return res != FALSE;
}

bool LegacyFileDialog::showSaveDlg()
{
	CurrentDirBackup backup;

	s_current = this;
	_isReadOnly = false;
	const BOOL res = ::GetSaveFileName(&_ofn);
	s_current = nullptr;

	return res != FALSE;
}

std::wstring LegacyFileDialog::doOpenSingleFileDlg()
{
	setupOfn(OFN_FILEMUSTEXIST);
	return showOpenDlg() ? std::wstring{ _fileBuffer.data() } : std::wstring{};
}

std::vector<std::wstring> LegacyFileDialog::doOpenMultiFilesDlg()
{
	setupOfn(OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT);
	if (!showOpenDlg())
		return {};

	std::vector<std::wstring> filenames;

	// Single selection: the buffer simply holds one full path.
	// Multi selection: the first string is the directory, the following ones the file names.
	const wchar_t* cursor = _fileBuffer.data();
	const size_t firstLength = std::wcslen(cursor);
	if (cursor[firstLength + 1] == L'\0')
	{
		filenames.emplace_back(cursor);
		return filenames;
	}

	std::wstring directory{ cursor, firstLength };
	if (directory.back() != L'\\')
		directory.push_back(L'\\');

	for (cursor += firstLength + 1; *cursor != L'\0'; cursor += std::wcslen(cursor) + 1)
	{
		filenames.emplace_back(directory + cursor);
	}

	return filenames;
}

std::wstring LegacyFileDialog::doSaveDlg()
{
	setupOfn(OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY);

	_openTheCopyAfterSaveAsCopy = false;
	if (!showSaveDlg())
		return {};

	// The Vista dialog exposes this through a shift-click on the OK button
	if (_isSavingAsCopy && (::GetKeyState(VK_SHIFT) & 0x8000) != 0)
		_openTheCopyAfterSaveAsCopy = true;

	return std::wstring{ _fileBuffer.data() };
}

std::wstring LegacyFileDialog::pickFolder()
{
	wchar_t displayName[MAX_PATH]{};

	BROWSEINFO browseInfo{};
	browseInfo.hwndOwner = _hwndOwner;
	browseInfo.pszDisplayName = displayName;
	browseInfo.lpszTitle = _title.empty() ? nullptr : _title.c_str();
	browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	browseInfo.lpfn = LegacyFileDialog::browseCallbackProc;
	browseInfo.lParam = reinterpret_cast<LPARAM>(this);

	PIDLIST_ABSOLUTE pidl = ::SHBrowseForFolder(&browseInfo);
	if (pidl == nullptr)
		return {};

	std::wstring folder;
	wchar_t path[MAX_PATH]{};
	if (::SHGetPathFromIDList(pidl, path))
		folder = path;

	::CoTaskMemFree(pidl);
	return folder;
}

///////////////////////////////////////////////////////////////////////////////

int CALLBACK LegacyFileDialog::browseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*lParam*/, LPARAM lpData)
{
	if (uMsg != BFFM_INITIALIZED)
		return 0;

	auto* self = reinterpret_cast<LegacyFileDialog*>(lpData);
	if (self == nullptr || self->_initialFolder.empty())
		return 0;

	::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, reinterpret_cast<LPARAM>(self->_initialFolder.c_str()));
	return 0;
}

UINT_PTR CALLBACK LegacyFileDialog::ofnHookProc(HWND hWnd, UINT uMsg, WPARAM /*wParam*/, LPARAM lParam)
{
	if (uMsg == WM_INITDIALOG)
	{
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s_current));
		return FALSE;
	}

	auto* self = reinterpret_cast<LegacyFileDialog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if (self != nullptr)
		return self->run(hWnd, uMsg, lParam);

	return FALSE;
}

BOOL LegacyFileDialog::run(HWND hWnd, UINT uMsg, LPARAM lParam)
{
	if (uMsg != WM_NOTIFY)
		return FALSE;

	const NMHDR* nmhdr = reinterpret_cast<LPNMHDR>(lParam);
	switch (nmhdr->code)
	{
		case CDN_INITDONE:
			createExtraControls(::GetParent(hWnd));
			return TRUE;

		case CDN_FILEOK:
		{
			// Grab the state of the injected checkboxes before the dialog closes.
			const HWND hDlg = ::GetParent(hWnd);
			const HWND hCheckbox = ::GetDlgItem(hDlg, legacyCheckboxId);
			if (hCheckbox != nullptr)
				_checkboxState = (::SendMessage(hCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);

			const HWND hFileTypeCheckbox = ::GetDlgItem(hDlg, legacyFileTypeCheckboxId);
			if (hFileTypeCheckbox != nullptr)
				_fileTypeCheckboxValue = (::SendMessage(hFileTypeCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);

			return FALSE;	// FALSE = let the shell validate the file name
		}

		default:
			return FALSE;
	}
}

void LegacyFileDialog::createExtraControls(HWND hDlg)
{
	const bool needCheckbox = !_checkboxLabel.empty();
	const bool needFileTypeCheckbox = !_fileTypeCheckboxLabel.empty();
	if (!needCheckbox && !needFileTypeCheckbox)
		return;

	const int rows = (needCheckbox ? 1 : 0) + (needFileTypeCheckbox ? 1 : 0);
	const int extraHeight = rows * (checkboxHeight + checkboxGap);

	// Make room at the bottom of the dialog for the checkboxes.
	RECT rcWindow{};
	::GetWindowRect(hDlg, &rcWindow);
	::SetWindowPos(hDlg, nullptr, 0, 0,
		rcWindow.right - rcWindow.left,
		(rcWindow.bottom - rcWindow.top) + extraHeight,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

	RECT rcClient{};
	::GetClientRect(hDlg, &rcClient);
	const int left = 12;
	const int width = (rcClient.right - rcClient.left) - 2 * left;
	int top = (rcClient.bottom - rcClient.top) - extraHeight + 4;

	const HFONT hFont = reinterpret_cast<HFONT>(::SendMessage(hDlg, WM_GETFONT, 0, 0));

	const auto createCheckbox = [hDlg, hFont, left, width](int id, const std::wstring& text, bool checked, int y)
	{
		const HWND hControl = ::CreateWindowExW(0, L"Button", text.c_str(),
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
			left, y, width, checkboxHeight,
			hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
			::GetModuleHandle(nullptr), nullptr);

		if (hControl != nullptr)
		{
			if (hFont != nullptr)
				::SendMessage(hControl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
			::SendMessage(hControl, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
		}
		return hControl;
	};

	if (needCheckbox)
	{
		createCheckbox(legacyCheckboxId, _checkboxLabel, _checkboxState, top);
		top += checkboxHeight + checkboxGap;
	}

	if (needFileTypeCheckbox)
	{
		createCheckbox(legacyFileTypeCheckboxId, _fileTypeCheckboxLabel, _fileTypeCheckboxValue, top);
	}
}
