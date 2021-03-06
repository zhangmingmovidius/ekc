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

#define WIN32_LEAN_AND_MEAN
#define INC_OLE2
#include "irenderingdevice_impl.h"

#include "impl_exception_helper.hpp"

IRenderingDeviceImpl::IRenderingDeviceImpl(plcl::RenderingDevice *output_) : output(output_)
{}
IRenderingDeviceImpl::~IRenderingDeviceImpl()
{}

STDMETHODIMP IRenderingDeviceImpl::get_SupportedPixfmt(DWORD *v) {
	try {
		if (!v)
			return E_INVALIDARG;
		
		*v = static_cast<DWORD>(output->SupportedPixfmt());
		return S_OK;
	} CATCH_EXCEPTION("IRenderingDeviceImpl::get_SupportedPixfmt()")
}

STDMETHODIMP IRenderingDeviceImpl::get_PixelFormat(DWORD *v) {
	try {
		if (!v)
			return E_INVALIDARG;

		*v = static_cast<DWORD>(output->Pixfmt());
		return S_OK;
	} CATCH_EXCEPTION("IRenderingDeviceImpl::get_PixelFormat()")
}

STDMETHODIMP IRenderingDeviceImpl::put_PixelFormat(DWORD v) {
	try {
		output->Pixfmt(static_cast<unsigned int>(v));
		return S_OK;
	} CATCH_EXCEPTION("IRenderingDeviceImpl::put_PixelFormat()")
}

STDMETHODIMP IRenderingDeviceImpl::SetViewport(DWORD x1, DWORD y1, DWORD x2, DWORD y2) {
	try {
		return (output->SetViewport(static_cast<unsigned int>(x1),
			static_cast<unsigned int>(y1),
			static_cast<unsigned int>(x2),
			static_cast<unsigned int>(y2))) ? S_OK : S_FALSE;
	} CATCH_EXCEPTION("IRenderingDeviceImpl::SetViewport()")
}

STDMETHODIMP IRenderingDeviceImpl::SweepScanline(DWORD y, int *scanline_ptr) {
	try {
		unsigned char *scanline;
		output->SweepScanline(static_cast<unsigned int>(y), &scanline);
		if (scanline_ptr)
			*scanline_ptr = (int)scanline;
		return S_OK;
	} CATCH_EXCEPTION("IRenderingDeviceImpl::SweepScanline()")
}

STDMETHODIMP IRenderingDeviceImpl::Render() {
	try {
		output->Render();
		return S_OK;
	} CATCH_EXCEPTION("IRenderingDeviceImpl::Render()")
}
