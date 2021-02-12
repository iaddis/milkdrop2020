#ifndef NULLSOFT_APIH
#define NULLSOFT_APIH

#include <string.h>
#include <windows.h>

inline const wchar_t *WasabiStrCpy(const wchar_t *str, wchar_t *buffer, size_t bufferLen)
{
	wcsncpy(buffer, str, bufferLen);
	buffer[bufferLen-1] = 0;
	return buffer;
}

#define WASABI_API_LNGSTRINGW(__id) (L"" L#__id)
#define WASABI_API_LNGSTRINGW_BUF(__id, __buf, __len) WasabiStrCpy(WASABI_API_LNGSTRINGW(__id), __buf, __len)

class Log {
public:
	inline static void Print(const char *message, ...)
	{
		OutputDebugStringA(message);
		OutputDebugStringA("\r\n");
	}


	inline static void Error(const char *message, ...)
	{
		OutputDebugStringA("ERROR:");
		OutputDebugStringA(message);
		OutputDebugStringA("\r\n");
	//	MessageBoxW(NULL, message, message, MB_OK|MB_SETFOREGROUND|MB_TOPMOST );
	}

};

#endif