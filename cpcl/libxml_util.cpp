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

#include <libxml/xpath.h>
#include <libxml/encoding.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>

#include "libxml_util.h"
#include "string_util.h"
#include "trace.h"

namespace cpcl {

struct LibXmlStuff {
	xmlTextReaderPtr reader;
	xmlXPathContextPtr xpath_context;
	xmlTextWriterPtr writer;

	LibXmlStuff() : reader(0), xpath_context(0), writer(0) {}
	~LibXmlStuff() {
		if (xpath_context)
			xmlXPathFreeContext(xpath_context);
		if (reader)
			xmlFreeTextReader(reader);

		if (writer)
			xmlFreeTextWriter(writer);
	}
};

static volatile int LibXmlInitialized = 0;

static int XMLCALL _readCallback(void *user_data, char *buffer, int len) {
	IOStream *stream = (IOStream*)user_data;
	unsigned long to_read = (unsigned long)(len&INT_MAX);
	unsigned long readed = stream->Read(buffer, to_read);
	if ((readed != to_read) && (!bool(*stream)))
		return -1;
	else
		return (int)readed;
}
static int XMLCALL _writeCallback(void *user_data, char const *buffer, int len) {
	IOStream *stream = (IOStream*)user_data;
	unsigned long to_write = (unsigned long)(len&INT_MAX);
	unsigned long written = stream->Write(buffer, to_write);
	if ((written != to_write) && (!bool(*stream)))
		return -1;
	else
		return (int)written;
}
static int XMLCALL _closeCallback(void* /*user_data*/) {
	return 0;
}

XmlDocument::XmlDocument() : TrimResults(true)
{}
XmlDocument::~XmlDocument()
{}

bool XmlDocument::XPath(StringPiece const &v, std::wstring *r) {
	WStringPiece str;
	xmlXPathObjectPtr xpath = xmlXPathEvalExpression((unsigned char const*)v.data(), libxml_stuff->xpath_context);
	if (xpath) {
		xmlChar *s_ = xmlXPathCastToString(xpath);
		if (s_) {
			StringPiece s((char*)s_);
			wchar_t *output = conv_buf.Alloc(s.length());
			int const conv_result = cpcl::TryConvertUTF8_UTF16(s, output, conv_buf.Size());
			if (conv_result == -1) {
				output = 0;
				cpcl::Trace(CPCL_TRACE_LEVEL_ERROR, "XmlDocument::XPath(%s): utf_to_uc fails", v.as_string().c_str());
			} else {
				output = conv_buf.Data() + conv_result;
				if (conv_result > ((int)(conv_buf.Size()&INT_MAX)))
					cpcl::Trace(CPCL_TRACE_LEVEL_WARNING, "XmlDocument::XPath(%s): TryConvertUTF8_UTF16 okashi desu ne...", v.as_string().c_str());
			}
			/*for(int i = 0, n = static_cast<int>(s.length()); i < n;) {
				int w = 0, k = cpcl::utf_to_uc(s_ + i, &w, n - i);
				if ((0 == k) || (w > WCHAR_MAX)) {
					output = 0;
					cpcl::Trace(CPCL_TRACE_LEVEL_ERROR, "XmlDocument::XPath(%s): utf_to_uc fails", v.as_string().c_str());
					break;
				}
				i += k;
				*output++ = (wchar_t)w;
			}*/

			xmlFree(s_);
			if (output) {
				//*output = 0;
				str = WStringPiece(conv_buf.Data(), output - conv_buf.Data());
			}
		}
		xmlXPathFreeObject(xpath);
	}

	/*if ((!str.empty()) && (TrimResults)) {
		wchar_t *head(conv_buf.Data()), *tail(conv_buf.Data() + str.size() - 1);
		while ((head != tail) && (wcschr(TrimChars, *head)))
			++head;
		while ((head != tail) && (wcschr(TrimChars, *tail)))
			--tail;

		++tail;
		str = WStringPiece(head, tail - head);
		*tail = 0;
	}*/
	if (TrimResults)
		str = str.trim(TrimChars);

	if ((!str.empty()) && (r))
		r->assign(str.data(), str.size());
	return (!str.empty());
}

wchar_t const XmlDocument::TrimChars[] = L" \t";

bool XmlDocument::Create(IOStream *input, XmlDocument **r) {
	if (!LibXmlInitialized) {
		cpcl::Error(cpcl::StringPieceFromLiteral("XmlDocument::Create(): call LibXmlInit() first"));
		return false;
	}
	if (!input)
		return false;

	std::auto_ptr<XmlDocument> xml(new XmlDocument());
	xml->libxml_stuff.reset(new LibXmlStuff());

	xml->libxml_stuff->reader = xmlReaderForIO(_readCallback, _closeCallback, input,
		"url://", NULL,
		XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOCDATA | XML_PARSE_COMPACT);
	if (!xml->libxml_stuff->reader) {
		Error(StringPieceFromLiteral("XmlDocument::Create(): xmlReaderForIO fails"));
		return false;
	}
	if (xmlTextReaderRead(xml->libxml_stuff->reader) != 1) {
		Error(StringPieceFromLiteral("XmlDocument::Create(): xmlTextReaderRead fails"));
		return false;
	}

	xmlNodePtr node = xmlTextReaderExpand(xml->libxml_stuff->reader);
	if ((!node) || (!node->doc))
		return false;
	xml->libxml_stuff->xpath_context = xmlXPathNewContext(node->doc);
	if (!xml->libxml_stuff->xpath_context)
		return false;

	if (r)
		*r = xml.release();
	return true;
}

XmlReader::XmlReader() : end_element(false)
{}
XmlReader::~XmlReader()
{}

// if (xmlTextReaderNodeType(reader) != -1) {
static char const* xmlReaderTypeNames[] = {
	"XML_READER_TYPE_NONE",
	"XML_READER_TYPE_ELEMENT",
	"XML_READER_TYPE_ATTRIBUTE",
	"XML_READER_TYPE_TEXT",
	"XML_READER_TYPE_CDATA",
	"XML_READER_TYPE_ENTITY_REFERENCE",
	"XML_READER_TYPE_ENTITY",
	"XML_READER_TYPE_PROCESSING_INSTRUCTION",
	"XML_READER_TYPE_COMMENT",
	"XML_READER_TYPE_DOCUMENT",
	"XML_READER_TYPE_DOCUMENT_TYPE",
	"XML_READER_TYPE_DOCUMENT_FRAGMENT",
	"XML_READER_TYPE_NOTATION",
	"XML_READER_TYPE_WHITESPACE",
	"XML_READER_TYPE_SIGNIFICANT_WHITESPACE",
	"XML_READER_TYPE_END_ELEMENT",
	"XML_READER_TYPE_END_ENTITY",
	"XML_READER_TYPE_XML_DECLARATION"
};

bool XmlReader::IsEndElement() { return end_element; }

bool XmlReader::Read() {
	end_element = false;

	bool r(xmlTextReaderRead(libxml_stuff->reader) == 1);
	while (r) {
		int const node_type = xmlTextReaderNodeType(libxml_stuff->reader);
		end_element = (node_type == XML_READER_TYPE_END_ELEMENT);

		if ((node_type == XML_READER_TYPE_END_ELEMENT)
			|| (node_type == XML_READER_TYPE_ELEMENT)
			|| (node_type == XML_READER_TYPE_TEXT))
			break;

		r = (xmlTextReaderRead(libxml_stuff->reader) == 1);
	}
	return r;
	//return (xmlTextReaderRead(libxml_stuff->reader) == 1);
}

int XmlReader::Depth() const {
	return xmlTextReaderDepth(libxml_stuff->reader);
}

bool XmlReader::HasAttributes() {
	return (xmlTextReaderHasAttributes(libxml_stuff->reader) == 1);
}

bool XmlReader::MoveToFirstAttribute() {
	end_element = false;

	bool r(xmlTextReaderMoveToFirstAttribute(libxml_stuff->reader) == 1);
	while (r) {
		int const node_type = xmlTextReaderNodeType(libxml_stuff->reader);
		if (node_type == XML_READER_TYPE_ATTRIBUTE)
			break;

		r = (xmlTextReaderMoveToNextAttribute(libxml_stuff->reader) == 1);
	}
	return r;
	//return (xmlTextReaderMoveToFirstAttribute(libxml_stuff->reader) == 1);
}

bool XmlReader::MoveToNextAttribute() {
	end_element = false;

	bool r(xmlTextReaderMoveToNextAttribute(libxml_stuff->reader) == 1);
	while (r) {
		int const node_type = xmlTextReaderNodeType(libxml_stuff->reader);
		if (node_type == XML_READER_TYPE_ATTRIBUTE)
			break;

		r = (xmlTextReaderMoveToNextAttribute(libxml_stuff->reader) == 1);
	}
	return r;
	//return (xmlTextReaderMoveToNextAttribute(libxml_stuff->reader) == 1);
}

//inline xmlChar const* xmlTextReaderNameWithoutCopy(xmlTextReaderPtr reader) {
//	xmlNodePtr node;
//	xmlChar *ret;
//	
//	if ((reader == NULL) || (reader->node == NULL))
//		return(NULL);
//	if (reader->curnode != NULL)
//		node = reader->curnode;
//	else
//		node = reader->node;
//	
//	switch (node->type) {
//		case XML_ELEMENT_NODE:
//		case XML_ATTRIBUTE_NODE:
//		case XML_ENTITY_NODE:
//		case XML_ENTITY_REF_NODE:
//		case XML_PI_NODE:
//		case XML_NOTATION_NODE:
//		case XML_DOCUMENT_TYPE_NODE:
//		case XML_DTD_NODE:
//			return node->name;
//		case XML_TEXT_NODE:
//			return (unsigned char const*)"#text";
//		case XML_CDATA_SECTION_NODE:
//			return (unsigned char const*)"#cdata-section";
//		case XML_COMMENT_NODE:
//	    return (unsigned char const*)"#comment";
//		case XML_DOCUMENT_NODE:
//		case XML_HTML_DOCUMENT_NODE:
//			return (unsigned char const*)"#document";
//		case XML_DOCUMENT_FRAG_NODE:
//			return (unsigned char const*)"#document-fragment";
//		case XML_NAMESPACE_DECL:
//			return (unsigned char const*)"xmlns";
//	}
//	return(NULL);
//}

StringPiece XmlReader::Name_UTF8() {
	/*return StringPiece(xmlTextReaderNameWithoutCopy(libxml_stuff->reader));*/
	return StringPiece((char const*)xmlTextReaderConstName(libxml_stuff->reader));
}
StringPiece XmlReader::Value_UTF8() {
	return StringPiece((char const*)xmlTextReaderConstValue(libxml_stuff->reader));
}

bool XmlReader::Create(IOStream *input, XmlReader **r) {
	if (!LibXmlInitialized) {
		cpcl::Error(cpcl::StringPieceFromLiteral("XmlReader::Create(): call LibXmlInit first()"));
		return false;
	}
	if (!input)
		return false;

	std::auto_ptr<XmlReader> xml(new XmlReader());
	xml->libxml_stuff.reset(new LibXmlStuff());

	xml->libxml_stuff->reader = xmlReaderForIO(_readCallback, _closeCallback, input,
		"url://", NULL,
		XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOCDATA | XML_PARSE_COMPACT);
	if (!xml->libxml_stuff->reader) {
		cpcl::Error(cpcl::StringPieceFromLiteral("XmlReader::Create(): xmlReaderForIO fails"));
		return false;
	}

	if (r)
		*r = xml.release();
	return true;
}

XmlWriter::XmlWriter()
{}
XmlWriter::~XmlWriter()
{}

bool XmlWriter::StartDocument(StringPiece encoding/*, IOStream *output*/) {
	/*unsigned char UTF8_BOM[] = { 0xEF, 0xBB, 0xBF };
	output->Write(UTF8_BOM, arraysize(UTF8_BOM));*/

	if (encoding.empty())
		encoding = StringPieceFromLiteral("UTF-8");
	return (xmlTextWriterStartDocument(libxml_stuff->writer, NULL, encoding.data(), NULL) != -1);
}
bool XmlWriter::StartElement(StringPiece const &name_utf8) {
	return (xmlTextWriterStartElement(libxml_stuff->writer, (unsigned char const*)name_utf8.data()) != -1);
}
bool XmlWriter::WriteAttribute(StringPiece const &name_utf8, StringPiece const &v_utf8) {
	return (xmlTextWriterWriteAttribute(libxml_stuff->writer, (unsigned char const*)name_utf8.data(), (unsigned char const*)v_utf8.data()) != -1);
}
bool XmlWriter::WriteAttribute(StringPiece const &name_utf8, WStringPiece const &v) {
	conv_buf.Alloc(v.size() * 3);
	unsigned char *tail = cpcl::ConvertUTF16_UTF8(v.data(), v.size(), conv_buf.Data(), conv_buf.Size() - 1); // shrink
	*tail = 0;
	return (xmlTextWriterWriteAttribute(libxml_stuff->writer, (unsigned char const*)name_utf8.data(), conv_buf.Data()) != -1);
}

bool XmlWriter::WriteAttribute(StringPiece const &name_utf8, double v) {
	char buf[32];
	_snprintf_s(buf, _TRUNCATE, "%.3f", v); // %f
	return (xmlTextWriterWriteAttribute(libxml_stuff->writer, (unsigned char const*)name_utf8.data(), (unsigned char const*)buf) != -1);
}
bool XmlWriter::WriteAttribute(StringPiece const &name_utf8, unsigned int v) {
	char buf[32];
	_snprintf_s(buf, _TRUNCATE, "%u", v);
	return (xmlTextWriterWriteAttribute(libxml_stuff->writer, (unsigned char const*)name_utf8.data(), (unsigned char const*)buf) != -1);
}
bool XmlWriter::WriteAttribute(StringPiece const &name_utf8, unsigned long v) {
	return XmlWriter::WriteAttribute(name_utf8, static_cast<unsigned int>(v));
}
bool XmlWriter::WriteAttribute(StringPiece const &name_utf8, int v) {
	char buf[32];
	_snprintf_s(buf, _TRUNCATE, "%d", v);
	return (xmlTextWriterWriteAttribute(libxml_stuff->writer, (unsigned char const*)name_utf8.data(), (unsigned char const*)buf) != -1);
}

bool XmlWriter::WriteString(StringPiece const &v_utf8) {
	return (xmlTextWriterWriteString(libxml_stuff->writer, (unsigned char const*)v_utf8.data()) != -1);
}
bool XmlWriter::WriteString(WStringPiece const &v) {
	conv_buf.Alloc(v.size() * 3);
	unsigned char *tail = cpcl::ConvertUTF16_UTF8(v.data(), v.size(), conv_buf.Data(), conv_buf.Size() - 1); // shrink
	*tail = 0;
	return (xmlTextWriterWriteString(libxml_stuff->writer, conv_buf.Data()) != -1);
}
bool XmlWriter::EndElement() {
	return (xmlTextWriterEndElement(libxml_stuff->writer) != -1);
}

bool XmlWriter::FlushWriter() {
	return (xmlTextWriterFlush(libxml_stuff->writer) != -1);
}
bool XmlWriter::SetEncoding(char const *encoding) {
	return (xmlTextWriterSetEncoding(libxml_stuff->writer, encoding) != -1);
}

bool XmlWriter::Create(IOStream *output, XmlWriter **r) {
	if (!LibXmlInitialized) {
		cpcl::Error(cpcl::StringPieceFromLiteral("XmlWriter::Create(): call LibXmlInit first()"));
		return false;
	}
	if (!output)
		return false;

	xmlOutputBufferPtr output_buffer = xmlOutputBufferCreateIO(_writeCallback, _closeCallback, output, NULL);
	if (!output_buffer) {
		cpcl::Error(cpcl::StringPieceFromLiteral("XmlWriter::Create(): xmlOutputBufferCreateIO fails"));
		return false;
	}

	std::auto_ptr<XmlWriter> xml(new XmlWriter());
	xml->libxml_stuff.reset(new LibXmlStuff());

	/* NOTE: the xmlOutputBufferPtr parameter will be deallocated when the writer is closed (if the call succeed.) */
	xml->libxml_stuff->writer = xmlNewTextWriter(output_buffer);
	if (!xml->libxml_stuff->writer) {
		xmlOutputBufferClose(output_buffer);
		output_buffer = NULL;
		cpcl::Error(cpcl::StringPieceFromLiteral("XmlWriter::Create(): xmlNewTextWriter fails"));
		return false;
	}

	if (r) {
		xmlTextWriterSetIndent(xml->libxml_stuff->writer, 1);
		/*if (!xml->StartDocument(output)) {
			cpcl::Error(cpcl::StringPieceFromLiteral("XmlWriter::Create(): xmlTextWriterStartDocument fails"));
			return false;
		}*/
		*r = xml.release();
	}
	return true;
}

/* xmlCharEncodingInputFunc:
 * @out:  a pointer to an array of bytes to store the UTF-8 result
 * @outlen:  the length of @out
 * @in:  a pointer to an array of chars in the original encoding
 * @inlen:  the length of @in
 *
 * Take a block of chars in the original encoding and try to convert
 * it to an UTF-8 block of chars out.
 *
 * Returns the number of bytes written, -1 if lack of space, or -2
 *     if the transcoding failed.
 * The value of @inlen after return is the number of octets consumed
 *     if the return value is positive, else unpredictiable.
 * The value of @outlen after return is the number of octets consumed.
 */
static int XmlConvertCP1251_UTF8(unsigned char *out, int *outlen,
	const unsigned char *in, int *inlen)
{
	if ((out == NULL) || (outlen == NULL) || (inlen == NULL) || (in == NULL))
		return (-1);
	int in_consumed = 0, in_length = *inlen;
	int out_consumed = 0, out_length = *outlen;
	*inlen = *outlen = -1;
	while (in_consumed < in_length) {
		int uc = CP1251_UTF16(*in++);
		unsigned char utf8[6];
		int len = uc_to_utf(uc, utf8);
		if (!len)
			return -2;

		if ((out_consumed + len) > out_length)
			break;
		in_consumed += 1;
		out_consumed += len;

		for (int i = 0; i < len; ++i)
			*out++ = utf8[i];
		*outlen = out_consumed;
		*inlen = in_consumed;
	}
	return *outlen;
}

/**
 * xmlCharEncodingOutputFunc:
 * @out:  a pointer to an array of bytes to store the result
 * @outlen:  the length of @out
 * @in:  a pointer to an array of UTF-8 chars
 * @inlen:  the length of @in
 *
 * Take a block of UTF-8 chars in and try to convert it to another
 * encoding.
 * Note: a first call designed to produce heading info is called with
 * in = NULL. If stateful this should also initialize the encoder state.
 *
 * Returns the number of bytes written, -1 if lack of space, or -2
 *     if the transcoding failed.
 * The value of @inlen after return is the number of octets consumed
 *     if the return value is positive, else unpredictiable.
 * The value of @outlen after return is the number of octets produced.
 */
static int XmlConvertUTF8_CP1251(unsigned char *out, int *outlen,
	const unsigned char *in, int *inlen)
{
	if (in == NULL) {
		/*
		* initialization nothing to do
		*/
		*outlen = 0;
		*inlen = 0;
		return 0;
	}
	int in_consumed = 0, in_length = *inlen;
	int out_consumed = 0, out_length = *outlen;
	*inlen = *outlen = -1;
	while (in_consumed < in_length) {
		int uc;
		int len = utf_to_uc(in, &uc, in_length - in_consumed);
		if (!len)
			break;
		in += len;
		unsigned char c = UTF16_CP1251(uc);

		if ((out_consumed + 1) > out_length)
			break;
		in_consumed += len;
		out_consumed += 1;
		
		*out++ = c;
		*outlen = out_consumed;
		*inlen = in_consumed;
	}
	return *outlen;
}

static CPCL_TRACE_LEVEL XML_TRACE_LEVEL[] = {
	CPCL_TRACE_LEVEL_INFO,    // XML_ERR_NONE    = 0
	CPCL_TRACE_LEVEL_WARNING, // XML_ERR_WARNING = 1
	CPCL_TRACE_LEVEL_ERROR,   // XML_ERR_ERROR   = 2
	CPCL_TRACE_LEVEL_ERROR    // XML_ERR_FATAL   = 3
};
static char const* XML_ERROR_DOMAIN_NAME[] = {
	"XML_FROM_NONE",
	"XML_FROM_PARSER",
	"XML_FROM_TREE",
	"XML_FROM_NAMESPACE",
	"XML_FROM_DTD",
	"XML_FROM_HTML",
	"XML_FROM_MEMORY",
	"XML_FROM_OUTPUT",
	"XML_FROM_IO",
	"XML_FROM_FTP",
	"XML_FROM_HTTP",
	"XML_FROM_XINCLUDE",
	"XML_FROM_XPATH",
	"XML_FROM_XPOINTER",
	"XML_FROM_REGEXP",
	"XML_FROM_DATATYPE",
	"XML_FROM_SCHEMASP",
	"XML_FROM_SCHEMASV",
	"XML_FROM_RELAXNGP",
	"XML_FROM_RELAXNGV",
	"XML_FROM_CATALOG",
	"XML_FROM_C14N",
	"XML_FROM_XSLT",
	"XML_FROM_VALID",
	"XML_FROM_CHECK",
	"XML_FROM_WRITER",
	"XML_FROM_MODULE",
	"XML_FROM_I18N",
	"XML_FROM_SCHEMATRONV"
};

static void _structuredErrorHandler(void *not_used, xmlErrorPtr error) {
	if (!error || (XML_ERR_OK == error->code))
		return;
	
	/*if ((error->node != NULL) && ((xmlNodePtr) error->node)->type == XML_ELEMENT_NODE) {
		node = ((xmlNodePtr) error->node)->name;
	}*/
	char const *s = "_structuredErrorHandler";
	if (error->message) {
		char *tail = error->message + strlen(error->message);
		while ((tail != error->message) && (*(tail - 1) == '\n'))
			--tail;
		*tail = '\0';

		s = error->message;
	}
	Trace(XML_TRACE_LEVEL[error->level], "%s: %s", XML_ERROR_DOMAIN_NAME[error->domain], s/*error->message*/);
}

void LibXmlInit() {
	xmlInitParser();

	xmlSetStructuredErrorFunc(0, _structuredErrorHandler);
	xmlNewCharEncodingHandler("WINDOWS-1251", XmlConvertCP1251_UTF8, XmlConvertUTF8_CP1251);

	LibXmlInitialized = 1;
}

void LibXmlCleanup() {
	xmlCleanupParser();

	LibXmlInitialized = 0;
}

} // namespace cpcl

#if 0 

tests

cpcl::LibXmlInit();

cpcl::XmlReader *reader_;
if (cpcl::XmlReader::Create(input.get(), &reader_)) {
	std::auto_ptr<cpcl::XmlReader> reader(reader_);
	while (reader->Read()) {
		std::cout << reader->Name_UTF8() << " : is closing tag : " << std::boolalpha << reader->IsEndElement();
		std::cout << std::endl;
		if (reader->HasAttributes()) {
			if (reader->MoveToFirstAttribute()) {
				std::cout << "\t" << reader->Name_UTF8() << " : " << reader->Value_UTF8() << std::endl;
			}
			while (reader->MoveToNextAttribute()) {
				std::cout << "\t" << reader->Name_UTF8() << " : " << reader->Value_UTF8() << std::endl;
			}
			std::cout << std::endl;
		}
	}
}

cpcl::FileStream *output_;
if (cpcl::FileStream::Create(L"C:\\Source\\hlam\\srv.session\\lsd-updated\\test_output_.xml", &output_)) {
	std::auto_ptr<cpcl::FileStream> output(output_);

	cpcl::XmlWriter *writer_;
	if (cpcl::XmlWriter::Create(output.get(), &writer_)) {
		std::auto_ptr<cpcl::XmlWriter> writer(writer_);

		writer->StartElement("info");
		writer->WriteAttribute("width", 1024);
		writer->WriteAttribute("height", 768);
		writer->WriteAttribute("zoom", 100.0);
		writer->WriteAttribute("next", "nani|page");
		writer->WriteAttribute("prev", "nani|page");
		writer->EndElement();
	}
}

void write_catalog(cpcl::XmlWriter *writer, Catalog::Tree const &tree) {
	for (Catalog::TreeConstIterator i = tree.begin(); i != tree.end(); ++i) {
		writer->StartElement("item");

		writer->WriteAttribute("id", (*i).first);
		writer->WriteAttribute("text", (*i).second.name);
		if ((*i).second.type == Catalog::CATALOG_ITEM_TYPE_FOLDER)
			writer->WriteAttribute("path", "unknown folder path");
		else if ((*i).second.type == Catalog::CATALOG_ITEM_TYPE_DOCUMENT)
			writer->WriteAttribute("path", (*((*i).second.page)).doc_path);
		else if ((*i).second.type == Catalog::CATALOG_ITEM_TYPE_PAGE)
			writer->WriteAttribute("page", (*((*i).second.page)).page_num);
		cpcl::StringPiece type("Page");
		switch ((*i).second.type) {
			case Catalog::CATALOG_ITEM_TYPE_FOLDER: type = "Folder"; break;
			case Catalog::CATALOG_ITEM_TYPE_DOCUMENT: type = "Document"; break;
			case Catalog::CATALOG_ITEM_TYPE_COMPOUND: type = "Compound"; break;
			case Catalog::CATALOG_INVALID_ITEM_TYPE: throw std::exception("CATALOG_INVALID_ITEM_TYPE");
		}	
		writer->WriteAttribute("type", type);

		if (!(*i).second.childs.empty())
			write_catalog(writer, (*i).second.childs);
		
		writer->EndElement();
	}
}

void write_catalog_worker(Catalog *catalog, cpcl::MemoryStream *output) {
	cpcl::XmlWriter *writer_;
	if (!cpcl::XmlWriter::Create(output, &writer_)) {
		cpcl::Error("XmlWriter::Create(output) fails");
		return;
	}

	std::auto_ptr<cpcl::XmlWriter> writer(writer_);
	writer->StartDocument();
	writer->StartElement("box");
	write_catalog(writer.get(), catalog->tree);
	writer->EndElement();
	writer.reset(); // writer not own output stream, but output stream must be valid until writer deleted

	cpcl::ScopedBuf<unsigned char, 0> tmp_buf;
	size_t size = static_cast<size_t>(output->Tell()&UINT_MAX);
	cpcl::MemoryStream tmp(tmp_buf.Alloc(size), size, true);

	if (!cpcl::XmlWriter::Create(&tmp, &writer_)) {
		cpcl::Error("XmlWriter::Create(tmp) fails");
		return;
	}

	writer.reset(writer_);
	writer->StartDocument();
	writer->StartElement("box");
	write_catalog(writer.get(), catalog->tree);
	writer->EndElement();
	writer.reset(); // writer not own output stream, but output stream must be valid until writer deleted

	if (memcmp(tmp.p, output->p, size))
		cpcl::Error("tmp != output");
	else
		cpcl::Debug("tmp == output");
}

cpcl::LibXmlCleanup();

#endif
