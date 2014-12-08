// Copyright (C) 2012 Yuri Agafonov
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#include "stdafx.h"

#include "string_util.h"
#include "file_util.h"
#include "file_istream.h"
#include "trace.h"

namespace cpcl {

FileIStream::FileIStream() : stream_impl(NULL)
{}
FileIStream::~FileIStream() {
	delete stream_impl;
}

STDMETHODIMP FileIStream::Read(void* pv, ULONG cb, ULONG* pcbRead) {
	unsigned long const processed = stream_impl->Read(pv, cb);
	if (pcbRead)
		*pcbRead = processed;
	if (!(*stream_impl))
		return E_FAIL;
	return (processed == cb) ? S_OK : S_FALSE;
}

STDMETHODIMP FileIStream::Write(const void* pv, ULONG cb, ULONG* pcbWritten) {
	unsigned long const processed = stream_impl->Write(pv, cb);
	if (pcbWritten)
		*pcbWritten = processed;
	if (!(*stream_impl))
		return E_FAIL;
	return (processed == cb) ? S_OK : S_FALSE;
}

STDMETHODIMP FileIStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) {
	if (dwOrigin > 2)
		return STG_E_INVALIDFUNCTION; // switch(dwOrigin) { case STREAM_SEEK_SET: ... break; case STREAM_SEEK_CUR: ... break; case STREAM_SEEK_END: ... break; default: return STG_E_INVALIDFUNCTION; }

	__int64 position;
	if (!stream_impl->Seek(dlibMove.QuadPart, dwOrigin, &position))
		return E_FAIL;
	if (plibNewPosition)
		(*plibNewPosition).QuadPart = position;
	return S_OK;
}

#ifndef INT64_MAX
static __int64 const INT64_MAX = (~(unsigned __int64)0) >> 1;
#endif
STDMETHODIMP FileIStream::SetSize(ULARGE_INTEGER libNewSize) {
	__int64 position, move_to(libNewSize.QuadPart & INT64_MAX);
	if (!stream_impl->Seek(move_to, STREAM_SEEK_SET, &position))
		return STG_E_INVALIDFUNCTION;
	if (position != move_to)
		return E_FAIL;

	if (::SetEndOfFile(stream_impl->hFile) == 0) {
		ErrorSystem(::GetLastError(), "FileIStream::SetSize('%s', %I64u): SetEndOfFile fails:",
			ConvertUTF16_CP1251(stream_impl->Path()).c_str(), move_to);
		return STG_E_MEDIUMFULL;
	}
	return S_OK;
}

STDMETHODIMP FileIStream::CopyTo(IStream *pstm, // A pointer to the destination stream. The stream pointed to by pstm can be a new stream or a clone of the source stream.
	ULARGE_INTEGER cb, // The number of bytes to copy from the source stream.
	ULARGE_INTEGER* pcbRead, // A pointer to the location where this method writes the actual number of bytes read from the source. You can set this pointer to NULL. In this case, this method does not provide the actual number of bytes read.
	ULARGE_INTEGER* pcbWritten) { // A pointer to the location where this method writes the actual number of bytes written to the destination. You can set this pointer to NULL. In this case, this method does not provide the actual number of bytes written.
	/*
	This method is equivalent to reading cb bytes into memory using ISequentialStream::Read and then immediately 
	writing them to the destination stream using ISequentialStream::Write, although IStream::CopyTo will be more efficient.
	The destination stream can be a clone of the source stream created by calling the IStream::Clone method.
	*/
	HRESULT hr(S_OK);
	unsigned char buf[0x1000];
	unsigned __int64 to_read = cb.QuadPart;
	unsigned long read = min((unsigned long)to_read, sizeof(buf));
	unsigned long readed, written;
	unsigned __int64 total_read(0), total_written(0);
	while ((readed = stream_impl->Read(buf, read)) > 0) {
		total_read += readed;
		
		if ((hr = pstm->Write(buf, readed, &written)) != S_OK)
			break;
		total_written += written;
		
		if ((to_read -= readed) == 0)
			break;
		read = min((unsigned long)to_read, sizeof(buf));
	}
	if (pcbWritten)
		pcbWritten->QuadPart = total_written;
	if (pcbRead)
		pcbRead->QuadPart = total_read;
	
	return hr;
	/*ULONG written;
    ULONG read = min(cb.LowPart, (ULONG)(m_buffer->size()-m_pos));
    HRESULT hr = pstm->Write(m_buffer->data()+m_pos, read, &written);
    if (pcbWritten) {
        pcbWritten->HighPart = 0;
        pcbWritten->LowPart = written;
    }
    if (pcbRead) {
        pcbRead->HighPart = 0;
        pcbRead->LowPart = read;
    }

    return hr;
	*/
}

STDMETHODIMP FileIStream::Commit(DWORD /*grfCommitFlags*/) { return S_OK; }

STDMETHODIMP FileIStream::Revert(void) { return S_OK; }

STDMETHODIMP FileIStream::LockRegion(/* [in] */ ULARGE_INTEGER /*libOffset*/,
        /* [in] */ ULARGE_INTEGER /*cb*/,
        /* [in] */ DWORD /*dwLockType*/) { return STG_E_INVALIDFUNCTION; }

STDMETHODIMP FileIStream::UnlockRegion(/* [in] */ ULARGE_INTEGER /*libOffset*/,
        /* [in] */ ULARGE_INTEGER /*cb*/,
        /* [in] */ DWORD /*dwLockType*/) { return STG_E_INVALIDFUNCTION; }

STDMETHODIMP FileIStream::Stat(/* [out] */ STATSTG* pstatstg,
        /* [in] */ DWORD grfStatFlag) {
	if (!pstatstg)
		return STG_E_INVALIDPOINTER;
	
	memset(pstatstg, 0, sizeof(STATSTG));
	if (STATFLAG_DEFAULT == grfStatFlag) {
		WStringPiece path = stream_impl->Path();
		if (!path.empty()) {
			pstatstg->pwcsName = (wchar_t*)::CoTaskMemAlloc((path.size() + 1) * sizeof(wchar_t));
			memcpy(pstatstg->pwcsName, path.data(), path.size() * sizeof(wchar_t));
			*(pstatstg->pwcsName + path.size()) = 0;
		}
	}
	pstatstg->type = STGTY_STREAM;
	pstatstg->cbSize.QuadPart = stream_impl->Size();
	pstatstg->clsid = CLSID_NULL;
	pstatstg->grfLocksSupported = 0;/* LOCK_WRITE; */
	
	return S_OK;
}

STDMETHODIMP FileIStream::Clone(/* [out] */ IStream** ppstm) {
	if (!ppstm)
		return STG_E_INVALIDPOINTER;
	// file open with FILE_ATTRIBUTE_TEMPORARY flag - all data can be not flushed to disk
	//::FlushFileBuffers(handle_);

	/* if clone_(shared / access)_mode == 0
	return STG_E_INSUFFICIENTMEMORY The stream was not cloned due to a lack of memory. */

	ScopedComPtr<FileIStream> stream(new FileIStream());
	if (!FileStream::FileStreamCreate(stream_impl->Path(), clone_access_mode, clone_share_mode,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, &stream->stream_impl))
		return E_FAIL;
	//HANDLE handle = ::CreateFileW(path.c_str(),
	//	cloneAccessMode_,
	//	cloneShareMode_,
	//	(LPSECURITY_ATTRIBUTES)NULL,
	//	OPEN_EXISTING, // When opening an existing file(dwCreationDisposition set to OPEN_EXISTING)
	//	FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL); // all FILE_ATTRIBUTE_* specified by dwFlagsAndAttributes ignored
	//if (INVALID_HANDLE_VALUE == handle)
	//	return E_FAIL;
	
	stream->clone_access_mode = clone_access_mode;
	stream->clone_share_mode = clone_share_mode;
	stream->stream_impl->Seek(stream_impl->Tell(), STREAM_SEEK_SET, NULL);

	//{
	//	unsigned char data[0x1024], data_[0x1024];
	//	unsigned __int64 p;
	//	seek(0, FILE_BEGIN, p);
	//	stream->seek(0, FILE_BEGIN, p);
	//	unsigned int a,b;
	//	read(data, sizeof(data), a);
	//	stream->read(data_, sizeof(data), b);
	//	while (a > 0) {
	//		if (a == b) {
	//			if (memcmp(data, data_, a) != 0) {
	//				std::ofstream out("C:\\Source\\tmp\\comp-sdt-rc2.0exe\\bin\\Release\\gdip.log", std::ios_base::out | std::ios_base::app);
	//				out << "\tdata in cloned file stream not equal" << std::endl;
	//			}
	//		} else {
	//			std::ofstream out("C:\\Source\\tmp\\comp-sdt-rc2.0exe\\bin\\Release\\gdip.log", std::ios_base::out | std::ios_base::app);
	//			out << "\tcloned file stream not equal" << std::endl;
	//		}

	//		read(data, sizeof(data), a);
	//		stream->read(data_, sizeof(data), b);
	//	}

	//	std::ofstream out("C:\\Source\\tmp\\comp-sdt-rc2.0exe\\bin\\Release\\gdip.log", std::ios_base::out | std::ios_base::app);
	//	out << "\tfile streams equal" << std::endl;

	//	seek(static_cast<__int64>(newPosition), FILE_BEGIN, p);
	//	stream->seek(static_cast<__int64>(newPosition), FILE_BEGIN, p);
	//}

	*ppstm = stream.Detach();
	return S_OK;
}

bool FileIStream::Create(cpcl::WStringPiece const &path, FileIStream **v) {
	ScopedComPtr<FileIStream> r(new FileIStream());
	if (!FileStream::Create(path, &r->stream_impl))
		return false;

	r->clone_access_mode = GENERIC_READ;
	r->clone_share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
	if (v)
		*v = r.Detach();
	return true;
}

bool FileIStream::Read(cpcl::WStringPiece const &path, FileIStream **v) {
	ScopedComPtr<FileIStream> r(new FileIStream());
	if (!FileStream::Read(path, &r->stream_impl))
		return false;

	r->clone_access_mode = GENERIC_READ;
	r->clone_share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	if (v)
		*v = r.Detach();
	return true;
}

bool FileIStream::ReadWrite(cpcl::WStringPiece const &path, FileIStream **v) {
	ScopedComPtr<FileIStream> r(new FileIStream());
	if (!FileStream::ReadWrite(path, &r->stream_impl))
		return false;

	r->clone_access_mode = GENERIC_READ | GENERIC_WRITE;
	r->clone_share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	if (v)
		*v = r.Detach();
	return true;
}

bool FileIStream::CreateTemporary(FileIStream **v) {
	ScopedComPtr<FileIStream> r(new FileIStream());
	if (!FileStream::CreateTemporary(&r->stream_impl))
		return false;

	r->clone_access_mode = GENERIC_READ;
	r->clone_share_mode = FILE_SHARE_READ | FILE_SHARE_DELETE;
	if (v)
		*v = r.Detach();
	return true;
}

#if 0

bool FileIStream::readPart(void *data, unsigned int size, unsigned int &processedSize) {
	if (size > CHUNK_SIZE_MAX)
		size = CHUNK_SIZE_MAX;
	DWORD processedLoc = 0;
	bool res = (::ReadFile(handle_, data, size, &processedLoc, NULL) != FALSE);
	processedSize = (unsigned int)processedLoc;
	return res;
}

bool FileIStream::read(void *data, unsigned int size, unsigned int &processedSize) {
	processedSize = 0;
	do {
		unsigned int processedLoc = 0;
		bool res = readPart(data, size, processedLoc);
		processedSize += processedLoc;
		if (!res)
			return false;
		if (processedLoc == 0)
			return true;
		data = (void *)((unsigned char *)data + processedLoc);
		size -= processedLoc;
	} while (size > 0);
	return true;
}

bool FileIStream::writePart(const void *data, unsigned int size, unsigned int &processedSize) {
	if (size > CHUNK_SIZE_MAX)
		size = CHUNK_SIZE_MAX;
	DWORD processedLoc = 0;
	bool res = (::WriteFile(handle_, data, size, &processedLoc, NULL) != FALSE);
	processedSize = (unsigned int)processedLoc;
	return res;
}

bool FileIStream::write(const void *data, unsigned int size, unsigned int &processedSize) {
	processedSize = 0;
	do {
		unsigned int processedLoc = 0;
		bool res = writePart(data, size, processedLoc);
		processedSize += processedLoc;
		if (!res)
			return false;
		if (processedLoc == 0)
			return true;
		data = (const void *)((const unsigned char *)data + processedLoc);
		size -= processedLoc;
	} while (size > 0);
	return true;
}

bool FileIStream::seek(__int64 distanceToMove, DWORD moveMethod, unsigned __int64 &newPosition) const {
	LARGE_INTEGER value;
	value.QuadPart = distanceToMove;
	value.LowPart = ::SetFilePointer(handle_, value.LowPart, &value.HighPart, moveMethod);
	if (value.LowPart == 0xFFFFFFFF)
		if (::GetLastError() != NO_ERROR)
			return false;

	newPosition = value.QuadPart;
	return true;
}

bool FileIStream::getLength(unsigned __int64 &length) const {
	DWORD sizeHigh;
	DWORD sizeLow = ::GetFileSize(handle_, &sizeHigh);
	if (sizeLow == 0xFFFFFFFF)
		if (::GetLastError() != NO_ERROR)
			return false;

	length = (((unsigned __int64)sizeHigh) << 32) + sizeLow;
	return true;
}

bool FileIStream::setLength(unsigned __int64 length) {
	unsigned __int64 newPosition;
	if (!seek(length, FILE_BEGIN, newPosition))
		return false;
	if (newPosition != length)
		return false;

	return (::SetEndOfFile(handle_) != FALSE);
}

HRESULT fileStream(const wchar_t *path, IStream **v) {
	cloneAccessMode_ = GENERIC_READ | GENERIC_WRITE; cloneShareMode_ = FILE_SHARE_READ | FILE_SHARE_WRITE;
		handle_ = ::CreateFileW(path_, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			(LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
		/*if (INVALID_HANDLE_VALUE == handle_)
			if (::GetLastError() == ERROR_FILE_NOT_FOUND) log(Error) << "temporary file not found";*/
		return (handle_ != INVALID_HANDLE_VALUE);

	CMyComPtr<basicFileStream> stream(new basicFileStream());
	if (!stream->open(path))
		return E_FAIL;
	*v = stream.Detach();
	return S_OK;
}

HRESULT fileStreamOpen(const wchar_t *path, IStream **v) {
	cloneAccessMode_ = GENERIC_READ | GENERIC_WRITE; cloneShareMode_ = FILE_SHARE_READ | FILE_SHARE_WRITE;
		handle_ = ::CreateFileW(path_, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			(LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
		/*if (INVALID_HANDLE_VALUE == handle_)
			if (::GetLastError() == ERROR_FILE_NOT_FOUND) log(Error) << "temporary file not found";*/
		return (handle_ != INVALID_HANDLE_VALUE);

	CMyComPtr<basicFileStream> stream(new basicFileStream());
	if (!stream->open(path))
		return E_FAIL;
	*v = stream.Detach();
	return S_OK;
}

HRESULT fileStreamRead(const wchar_t *path, IStream **v) {
	cloneAccessMode_ = GENERIC_READ; cloneShareMode_ = FILE_SHARE_READ;
		handle_ = ::CreateFileW(path_, GENERIC_READ, FILE_SHARE_READ,
			(LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
		return (handle_ != INVALID_HANDLE_VALUE);

	CMyComPtr<basicFileStream> stream(new basicFileStream());
	if (!stream->openRead(path))
		return E_FAIL;
	*v = stream.Detach();
	return S_OK;
}

HRESULT fileStreamCreate(const wchar_t *path, IStream **v) {
	cloneAccessMode_ = GENERIC_READ | GENERIC_WRITE; cloneShareMode_ = FILE_SHARE_READ | FILE_SHARE_WRITE;
		handle_ = ::CreateFileW(path_, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			(LPSECURITY_ATTRIBUTES)NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
	return (handle_ != INVALID_HANDLE_VALUE);

	CMyComPtr<basicFileStream> stream(new basicFileStream());
	if (!stream->create(path))
		return E_FAIL;
	*v = stream.Detach();
	return S_OK;
}

HRESULT temporaryFileStream(IStream **v) {
	std::wstring folder, path;
	if (!getTemporaryDirectory(&folder))
		return E_FAIL;
	/*unsigned __int64 estimatedSizeByte;
	if (estimatedSizeByte > 0) {
		const size_t estimatedSizeMiB = size_t(estimatedSizeByte >> 20);
		if ((estimatedSizeMiB + 256) > availableDiskSpaceMib(folder))
			return STG_E_MEDIUMFULL;
	}*/
	cloneAccessMode_ = GENERIC_READ | GENERIC_WRITE; cloneShareMode_ = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		handle_ = ::CreateFileW(path_, GENERIC_READ | GENERIC_WRITE, cloneShareMode_,
			(LPSECURITY_ATTRIBUTES)NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, (HANDLE)NULL);
		/*if (INVALID_HANDLE_VALUE == handle_)
			if (::GetLastError() == ERROR_FILE_NOT_FOUND) log(Error) << "temporary file not found";*/
		return (handle_ != INVALID_HANDLE_VALUE);

	if (!createTemporaryFile(folder, &path))
		return E_FAIL;
	CMyComPtr<basicFileStream> stream(new basicFileStream());
	if (!stream->createTemporaryAndSwapPath(path))
		return E_FAIL;
	*v = stream.Detach();
	return S_OK;
}

#endif

} // namespace cpcl
