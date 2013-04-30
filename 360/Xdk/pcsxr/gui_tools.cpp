#include "gui.h"

std::string get_string(const std::wstring & s, std::string & d)
{
	char buffer[256];
	const wchar_t * cs = s.c_str();
	wcstombs ( buffer, cs, sizeof(buffer) );

	d = buffer;

	return d;
}

std::wstring get_wstring(const std::string & s, std::wstring & d)
{
	wchar_t buffer[256];
	const char * cs = s.c_str();
	mbstowcs ( buffer, cs, sizeof(buffer) );

	d = buffer;

	return d;
}
