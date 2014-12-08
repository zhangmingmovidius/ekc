// iplugin_impl.h
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
#pragma once

#ifndef __PLCL_IPLUGIN_IMPL_H
#define __PLCL_IPLUGIN_IMPL_H

#include <memory>
#include "plugin.h"

#include "plugin_interface.h"

class IPluginImpl : public CUnknownImp, public IPlugin {
	std::auto_ptr<plcl::Plugin> plugin;

	IPluginImpl();
	IPluginImpl(IPluginImpl const&);
	void operator=(IPluginImpl const&);
public:
	IPluginImpl(plcl::Plugin *plugin_);
	virtual ~IPluginImpl();

	STDMETHOD(DocFileExtension)(wchar_t const **r, DWORD *r_size);
	STDMETHOD(CopyDocFileExtension)(wchar_t *v, DWORD v_size);
	STDMETHOD(DocFileFormat)(wchar_t const **r, DWORD *r_size);
	STDMETHOD(CopyDocFileFormat)(wchar_t *v, DWORD v_size);
	STDMETHOD(CheckHeader)(IStream *stream);
	STDMETHOD(GetSignatures)(int *signatures_ptr, DWORD *n_signatures);
	STDMETHOD(LoadDocument)(IStream *input, IPluginDocument **r);

	CPCL_UNKNOWN_IMP1_MT(IPlugin)
};

#endif // __PLCL_IPLUGIN_IMPL_H