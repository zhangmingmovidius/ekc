// irenderingdata_wrapper.h
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

#ifndef __PLCL_IRENDERING_DATA_WRAPPER_H
#define __PLCL_IRENDERING_DATA_WRAPPER_H

#include "rendering_data.h"

struct IRenderingData;
struct IRenderingDataWrapper : public plcl::RenderingData {
	IRenderingDataWrapper(IRenderingData *v);
  IRenderingDataWrapper(const IRenderingDataWrapper &r);
	operator IRenderingData*() const;
	IRenderingData* operator=(IRenderingData *v);
	IRenderingDataWrapper& operator=(const IRenderingDataWrapper &r);
	virtual ~IRenderingDataWrapper();

	virtual unsigned int Width() const;
	virtual unsigned int Height() const;
	virtual int Stride() const;
	virtual unsigned int Pixfmt() const;
	///* suck, shit, blyaha */
	//virtual void Pixfmt(unsigned int v);
	virtual unsigned int BitsPerPixel() const;

	virtual unsigned char* Scanline(unsigned int y);
private:
	IRenderingData *data;

	IRenderingDataWrapper();
};

#endif // __PLCL_IRENDERING_DATA_WRAPPER_H
