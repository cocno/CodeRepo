/**************************************************************************************************
write_trace.cpp
	Copyright Reserved by Peng Ting

Purpose:
	implement trace tracking function

Author:
	peng.ting

Created date:
	2015.11.11
**************************************************************************************************/

#include <cstdio>
#include <iostream>
#include <fstream>

void write_trace (const char* fmt, ...)
{
	char buf[2048];
	va_list ap;
	va_start (ap, fmt);
	int len = ::vsprintf (buf, fmt, ap);
	va_end (ap);

#ifdef __WINDOWS__
	std::fstream fs (_T("C:\\trace.log"), fstream::app | fstream::out | fstream::ate);
#else
	std::fstream fs (_T("/trace.log"), fstream::app | fstream::out | fstream::ate);
#endif

	fs << buf << std::endl;
	fs.close ();
}

#ifdef __LINUX__
#define ADD_XXX_TRACE(args...)							\
	::write_trace ("[%s %s] [%s %4d] : %s", __DATE__, __TIME__, __FILE__, __LINE__, args)
#else
#define ADD_XXX_TRACE(...)								\
	::write_trace ("[%s %s] [%s %4d] : %s", __DATE__, __TIME__, __FILE__, __LINE__, __VA_ARGS__)
#endif

int main (void)
{
	printf ("JUST GO");
	ADD_XXX_TRACE ("It's just a test.");
	return 0;
}
