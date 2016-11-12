/******************************************************************************
ncOracleExecuterCommon.h:
	Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#ifndef __NC_ORACLE_EXECUTER_COMMON_H__
#define __NC_ORACLE_EXECUTER_COMMON_H__

extern IResourceLoader* oracleExecuterLoader;

#define LOAD_STRING(ids)			\
	oracleExecuterLoader->loadString (ids)

/**
* trace
*/
NC_DECLARE_TRACE_MODULE(ncOracleExecuterTrace);

#ifdef __WINDOWS__
#define NC_ORACLE_EXECUTER_TRACE(...)			NC_TRACE (ncOracleExecuterTrace, __VA_ARGS__)
#else
#define NC_ORACLE_EXECUTER_TRACE(args...)		NC_TRACE (ncOracleExecuterTrace, args)
#endif // end of Linux

#endif // __NC_ORACLE_EXECUTER_COMMON_H__
