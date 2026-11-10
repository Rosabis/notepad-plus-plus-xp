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

// GDI+-backed PNG icon loader for Windows XP
// ------------------------------------------
// Windows XP's user32 cannot decode PNG-compressed icon resources:
// LoadImage(IMAGE_ICON) and CreateIconFromResourceEx() only understand the
// legacy BMP (DIB) payload inside an RT_ICON entry.  PNG-in-ico support was
// introduced with Windows Vista.
//
// Windows XP DOES however ship gdiplus.dll (GDI+ 1.0 since XP SP1), which can
// decode PNG just fine.  This header loads GDI+ lazily at run time through its
// flat C API (no import library, no static dependency) and turns the PNG bytes
// of an RT_ICON resource into an HICON.
//
// Everything here is optional: if gdiplus.dll is missing (bare XP RTM) or any
// step fails, the loader returns nullptr and the caller falls back to its
// usual LoadImage path.

#include <windows.h>
#include <objidl.h>

namespace NppGdiPlus
{

	// ---------------------------------------------------------------------
	// Minimal read-only in-memory IStream.
	//
	// GDI+ only accepts an IStream (or a file) as a decoding source, and the
	// icon data lives in the resource section, so we need a stream over it.
	// SHCreateMemStream would do the job but lives in shlwapi and its export
	// varies across versions, so the few dozen lines below are self-contained.
	//
	// The object is always used on the stack: AddRef/Release are stubs so that
	// GDI+ releasing the interface never deletes memory it does not own.
	// ---------------------------------------------------------------------
	class MemoryStream final : public IStream
	{
	public:
		MemoryStream(const void* data, ULONG size) noexcept
			: _data(static_cast<const BYTE*>(data)), _size(size), _pos(0)
		{
		}

		MemoryStream(const MemoryStream&) = delete;
		MemoryStream& operator=(const MemoryStream&) = delete;

		// --- IUnknown -----------------------------------------------------
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
		{
			if (ppv == nullptr)
				return E_POINTER;

			if (riid == IID_IUnknown || riid == IID_IStream || riid == IID_ISequentialStream)
			{
				*ppv = static_cast<IStream*>(this);
				return S_OK;
			}

			*ppv = nullptr;
			return E_NOINTERFACE;
		}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return 2; // never let the caller free a stack object
		}

		ULONG STDMETHODCALLTYPE Release() override
		{
			return 1; // ditto
		}

		// --- ISequentialStream --------------------------------------------
		HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override
		{
			if (pv == nullptr)
				return E_POINTER;

			const ULONG remaining = (_pos < _size) ? (_size - _pos) : 0;
			const ULONG toRead = (cb < remaining) ? cb : remaining;

			if (toRead != 0)
				::memcpy(pv, _data + _pos, toRead);

			_pos += toRead;
			if (pcbRead != nullptr)
				*pcbRead = toRead;

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE Write(const void* pv, ULONG cb, ULONG* pcbWritten) override
		{
			(void)pv;
			(void)cb;
			if (pcbWritten != nullptr)
				*pcbWritten = 0;
			return STG_E_ACCESSDENIED; // read-only by design
		}

		// --- IStream -------------------------------------------------------
		HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) override
		{
			LONGLONG newPos = static_cast<LONGLONG>(_pos);

			switch (dwOrigin)
			{
				case STREAM_SEEK_SET:
					newPos = dlibMove.QuadPart;
					break;
				case STREAM_SEEK_CUR:
					newPos += dlibMove.QuadPart;
					break;
				case STREAM_SEEK_END:
					newPos = static_cast<LONGLONG>(_size) + dlibMove.QuadPart;
					break;
				default:
					return STG_E_INVALIDFUNCTION;
			}

			if (newPos < 0)
				return STG_E_INVALIDFUNCTION;

			_pos = (newPos > static_cast<LONGLONG>(_size)) ? _size : static_cast<ULONG>(newPos);

			if (plibNewPosition != nullptr)
				plibNewPosition->QuadPart = static_cast<ULONGLONG>(_pos);

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize) override
		{
			(void)libNewSize;
			return STG_E_ACCESSDENIED;
		}

		HRESULT STDMETHODCALLTYPE CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) override
		{
			if (pstm == nullptr)
				return E_POINTER;

			ULONG toCopy = (_pos < _size) ? (_size - _pos) : 0;
			if (cb.QuadPart < static_cast<ULONGLONG>(toCopy))
				toCopy = static_cast<ULONG>(cb.QuadPart);

			ULONG written = 0;
			HRESULT hr = (toCopy != 0) ? pstm->Write(_data + _pos, toCopy, &written) : S_OK;

			_pos += written;
			if (pcbRead != nullptr)
				pcbRead->QuadPart = static_cast<ULONGLONG>(written);
			if (pcbWritten != nullptr)
				pcbWritten->QuadPart = static_cast<ULONGLONG>(written);

			return hr;
		}

		HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags) override
		{
			(void)grfCommitFlags;
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE Revert() override
		{
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override
		{
			(void)libOffset;
			(void)cb;
			(void)dwLockType;
			return STG_E_INVALIDFUNCTION;
		}

		HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override
		{
			(void)libOffset;
			(void)cb;
			(void)dwLockType;
			return STG_E_INVALIDFUNCTION;
		}

		HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstatstg, DWORD grfStatFlag) override
		{
			(void)grfStatFlag;
			if (pstatstg == nullptr)
				return E_POINTER;

			::ZeroMemory(pstatstg, sizeof(*pstatstg));
			pstatstg->type = STGTY_STREAM;
			pstatstg->cbSize.QuadPart = static_cast<ULONGLONG>(_size);
			pstatstg->grfMode = STGM_READ;

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE Clone(IStream** ppstm) override
		{
			if (ppstm == nullptr)
				return E_POINTER;
			*ppstm = nullptr;
			return E_NOTIMPL;
		}

	private:
		const BYTE* _data = nullptr;
		ULONG _size = 0;
		ULONG _pos = 0;
	};


	// ---------------------------------------------------------------------
	// GDI+ flat C API.
	//
	// GDI+ exports its flat API as plain __stdcall symbols (no C++ name
	// mangling), so GetProcAddress by name works and no import library is
	// needed.  That keeps gdiplus.dll out of the import table entirely, which
	// matters because a static dependency would break the loader on a system
	// without GDI+.
	// ---------------------------------------------------------------------
	namespace detail
	{
		// GDI+ returns 0 (Ok) on success.
		enum class Status : int
		{
			Ok = 0,
			GenericError = 1,
			InvalidParameter = 2,
			OutOfMemory = 3,
			ObjectBusy = 4,
			InsufficientBuffer = 5,
			NotImplemented = 6,
			Win32Error = 7,
			WrongState = 8,
			Aborted = 9,
			FileNotFound = 10,
			ValueOverflow = 11,
			AccessDenied = 12,
			UnknownImageFormat = 13,
			FontFamilyNotFound = 14,
			FontStyleNotFound = 15,
			NotTrueTypeFont = 16,
			UnsupportedGdiplusVersion = 17,
			GdiplusNotInitialized = 18,
			PropertyNotFound = 19,
			PropertyNotSupported = 20
		};

		// Opaque GDI+ handles.
		using GpBitmap = void;
		using GpImage = void;

		// Must be kept in sync with Gdiplus::GdiplusStartupInput (gdiplus.h).
		// Only the four fields GDI+ actually reads before we hand it over.
		struct StartupInput
		{
			UINT32 GdiplusVersion = 1;      // GDI+ 1.x -- the only version on XP
			void* DebugEventCallback = nullptr;
			BOOL SuppressBackgroundThread = FALSE;
			BOOL SuppressExternalCodecs = FALSE;
		};

		using FnStartup = int (WINAPI*)(ULONG_PTR*, const StartupInput*, void*);
		using FnShutdown = void (WINAPI*)(ULONG_PTR);
		using FnCreateBitmapFromStream = int (WINAPI*)(IStream*, GpBitmap**);
		using FnCreateHICONFromBitmap = int (WINAPI*)(GpBitmap*, HICON*);
		using FnDisposeImage = int (WINAPI*)(GpImage*);
		using FnGetImageWidth = int (WINAPI*)(GpImage*, UINT*);
		using FnGetImageHeight = int (WINAPI*)(GpImage*, UINT*);

		// Cached entry points.  A null `startup` means GDI+ is unavailable on
		// this machine and every helper must bail out immediately.
		inline FnStartup& startupProc() noexcept
		{
			static FnStartup proc = nullptr;
			return proc;
		}

		inline FnShutdown& shutdownProc() noexcept
		{
			static FnShutdown proc = nullptr;
			return proc;
		}

		inline FnCreateBitmapFromStream& createBitmapProc() noexcept
		{
			static FnCreateBitmapFromStream proc = nullptr;
			return proc;
		}

		inline FnCreateHICONFromBitmap& createHIconProc() noexcept
		{
			static FnCreateHICONFromBitmap proc = nullptr;
			return proc;
		}

		inline FnDisposeImage& disposeImageProc() noexcept
		{
			static FnDisposeImage proc = nullptr;
			return proc;
		}

		inline bool& probeDone() noexcept
		{
			static bool done = false;
			return done;
		}

		// Resolve all entry points once.  Cheap after the first call.
		inline void probe() noexcept
		{
			if (probeDone())
				return;

			probeDone() = true;

			HMODULE mod = ::GetModuleHandleW(L"gdiplus.dll");
			if (mod == nullptr)
				mod = ::LoadLibraryW(L"gdiplus.dll");

			if (mod == nullptr)
				return; // no GDI+ on this system -- fall back to LoadImage

			startupProc() = reinterpret_cast<FnStartup>(::GetProcAddress(mod, "GdiplusStartup"));
			shutdownProc() = reinterpret_cast<FnShutdown>(::GetProcAddress(mod, "GdiplusShutdown"));
			createBitmapProc() = reinterpret_cast<FnCreateBitmapFromStream>(::GetProcAddress(mod, "GdipCreateBitmapFromStream"));
			createHIconProc() = reinterpret_cast<FnCreateHICONFromBitmap>(::GetProcAddress(mod, "GdipCreateHICONFromBitmap"));
			disposeImageProc() = reinterpret_cast<FnDisposeImage>(::GetProcAddress(mod, "GdipDisposeImage"));

			// If any required entry point is missing, treat GDI+ as unusable.
			if (startupProc() == nullptr || shutdownProc() == nullptr ||
			    createBitmapProc() == nullptr || createHIconProc() == nullptr ||
			    disposeImageProc() == nullptr)
			{
				startupProc() = nullptr;
			}
		}

		inline bool available() noexcept
		{
			probe();
			return startupProc() != nullptr;
		}
	}


	// ---------------------------------------------------------------------
	// Public entry point: decode raw PNG bytes into an HICON.
	//
	// `pngData`/`pngSize` describe one RT_ICON entry (a PNG image, i.e. one
	// that begins with 89 50 4E 47).  Returns nullptr when GDI+ is missing or
	// the payload cannot be decoded.
	//
	// `cx`/`cy` are the size the caller asked for.  GDI+ always builds the icon
	// at the PNG's native size, which can differ from the request when the best
	// matching resource entry is only a near fit (a 32x32 image chosen for a
	// 25x25 request at 150% DPI, say).  When they differ the icon is rescaled
	// with CopyImage so the caller gets the size it expects.
	// ---------------------------------------------------------------------
	inline HICON createIconFromPngData(const void* pngData, ULONG pngSize, int cx = 0, int cy = 0) noexcept
	{
		if (pngData == nullptr || pngSize == 0)
			return nullptr;

		if (!detail::available())
			return nullptr;

		// GDI+ startup token.  Kept for the lifetime of the process: shutting
		// GDI+ down and restarting it around every icon would be pure waste,
		// and a leaked token is harmless for a GUI app.
		static ULONG_PTR token = 0;
		static bool started = false;

		if (!started)
		{
			detail::StartupInput input;
			if (detail::startupProc()(&token, &input, nullptr) != static_cast<int>(detail::Status::Ok))
				return nullptr;
			started = true;
		}

		MemoryStream stream(pngData, pngSize);

		detail::GpBitmap* bitmap = nullptr;
		if (detail::createBitmapProc()(&stream, &bitmap) != static_cast<int>(detail::Status::Ok) || bitmap == nullptr)
			return nullptr;

		HICON hIcon = nullptr;
		const int hr = detail::createHIconProc()(bitmap, &hIcon);

		detail::disposeImageProc()(bitmap);

		if (hr != static_cast<int>(detail::Status::Ok) || hIcon == nullptr)
			return nullptr;

		// Rescale to the requested size when the resource was only a near match.
		if (cx > 0 && cy > 0)
		{
			ICONINFO ii{};
			if (::GetIconInfo(hIcon, &ii))
			{
				BITMAP bm{};
				if (ii.hbmColor != nullptr && ::GetObjectW(ii.hbmColor, sizeof(bm), &bm) != 0)
				{
					if (bm.bmWidth != cx || bm.bmHeight != cy)
					{
						const HICON hScaled = static_cast<HICON>(
							::CopyImage(hIcon, IMAGE_ICON, cx, cy, 0));
						if (hScaled != nullptr)
						{
							::DestroyIcon(hIcon);
							hIcon = hScaled;
						}
					}
				}

				if (ii.hbmColor != nullptr)
					::DeleteObject(ii.hbmColor);
				if (ii.hbmMask != nullptr)
					::DeleteObject(ii.hbmMask);
			}
		}

		return hIcon;
	}

} // namespace NppGdiPlus
