#pragma once
#include <Windows.h>
#include <string>
#include <atlimage.h>

class CHeTool {
public:
	//使用静态方法，转变成一个相对的全局函数
	static void Dump(BYTE* pData, size_t nSize) {
		std::string strOut;
		for (size_t i = 0; i < nSize; i++) {
			char buf[8] = "";
			if (i > 0 && (i % 16 == 0)) strOut += "\n";
			snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
			strOut += buf;
		}
		strOut += "\n";
		OutputDebugStringA(strOut.c_str());
	}

	static int BytesToImage(CImage& image, const std::string& strBuffer) {
		//更新数据到缓存
		BYTE* pData = (BYTE*)strBuffer.c_str();
		//数据存入CImage
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
		if (hMem == NULL) {
			TRACE("内存不足!");
			Sleep(1);
			return -1;
		}
		IStream* pStream = NULL;
		HRESULT hRet = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
		if (hRet == S_OK) {
			ULONG length = 0;
			pStream->Write(pData, strBuffer.size(), &length);
			LARGE_INTEGER bg = { 0 };
			pStream->Seek(bg, STREAM_SEEK_SET, NULL);
			if ((HBITMAP)image != NULL)
				image.Destroy();
			image.Load(pStream);
		}

		return hRet;
	}
};

