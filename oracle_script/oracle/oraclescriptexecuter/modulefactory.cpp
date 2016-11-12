/******************************************************************************
modulefactory.cpp:
	Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#include <abprec.h>
#include <ncutil.h>
#include "ncOracleExecuter.h"

#define LIB_NAME _T("oracleschedule")

IResourceLoader* oracleExecuterLoader = 0;

/**
* DEFINE TRACE
*/
NC_DEFINE_TRACE_MODULE(ncOracleExecuterTrace);

NS_GENERIC_FACTORY_CONSTRUCTOR(ncOracleExecuter);

//
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static const nsModuleComponentInfo components[] =
{
	{
		"ncOracleExecuter",
		NC_ORACLE_EXECUTER_CID,
		NC_ORACLE_EXECUTER_CONTRACTID,
		ncOracleExecuterConstructor
	},
};

class ncOracleBackupLib : public ISharedLibrary
{
public:
	virtual void onInitLibrary(const AppSettings *appSettings,
		const class AppContext *appCtx)
	{
		if (oracleExecuterLoader != 0)
			return;

		oracleExecuterLoader = new MoResourceLoader(::getResourceFileName(LIB_NAME,
			appSettings,
			appCtx,
			AB_RESOURCE_MO_EXT_NAME));

		//
		// 添加 trace
		//
		NC_CREATE_TRACE_MODULE(ncOracleBackupTrace, LIB_NAME);
	}

	virtual void onCloseLibrary(void) NO_THROW
	{
		if (oracleExecuterLoader != 0) {
			delete oracleExecuterLoader;
			oracleExecuterLoader = 0;
		}
	}

	virtual void onInstall(const AppSettings* appSettings,
		const AppContext* appCtx)
	{
	}

	virtual void onUninstall(void) NO_THROW
	{
	}

	virtual const tchar_t* getLibName(void) const
	{
		return LIB_NAME;
	}
}; // class ncOracleBackupLib

//
// 定义共享库
//
AB_IMPL_NSGETMODULE_WITH_LIB(LIB_NAME, components, ncOracleBackupLib)
