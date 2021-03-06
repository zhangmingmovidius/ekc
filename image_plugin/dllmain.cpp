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

#include <vector>

#include <error_handler.h>
#include <trace.h>
#include <dumbassert.h>
#include <com_ptr.hpp>

#include "bmp_plugin.h"

#define WIN32_LEAN_AND_MEAN
#define INC_OLE2
#include <windows.h>

#include <iplugin_impl.h>
#include <impl_exception_helper.hpp>

static void __stdcall NullHandler(char const* /*s*/, unsigned int /*s_length*/)
{}
static void __stdcall AssertHandler(char const *s, char const *file, unsigned int line) {
	cpcl::Trace(CPCL_TRACE_LEVEL_ERROR, "%s at %s:%d epic fail", s, file, line);
	
	throw std::runtime_error(s);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		cpcl::SetErrorHandler(NullHandler);
		cpcl::SetAssertHandler(AssertHandler);
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

template<class T>
inline plcl::Plugin* ctor() {
	return new T();
}
STDAPI CreatePlugins(DWORD *count, IPlugin **r) {
	typedef plcl::Plugin* (*Ctor)();
	Ctor ctors[] = {
		ctor<BmpPlugin>
	};

	if (!count)
		return E_INVALIDARG;
	if (!r) {
		*count = static_cast<DWORD>(arraysize(ctors));
		return S_OK;
	}

	std::vector<cpcl::ComPtr<IPlugin> > plugins_ptr;
	try {
	plugins_ptr.reserve(arraysize(ctors));
	for (size_t i = 0, n = (std::min)(arraysize(ctors), static_cast<size_t>(*count)); i < n; ++i)
		plugins_ptr.push_back(new IPluginImpl(ctors[i]()));
	} CATCH_EXCEPTION("image_plugin::CreatePlugins()")

	for (size_t i = 0, n = plugins_ptr.size(); i < n; ++i)
		r[i] = plugins_ptr[i].Detach();
	
	*count = static_cast<DWORD>(plugins_ptr.size());
	return S_OK;
}

STDAPI _InitTrace(wchar_t const *log_path, cpcl::ErrorHandlerPtr error_handler) {
	cpcl::SetTraceFilePath(log_path);
	cpcl::SetErrorHandler(error_handler);
	return S_OK;
}
