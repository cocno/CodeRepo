/****************************************************************************************
*  test.cpp
*		Copyright (c)  Eisoo Software Inc. (2004-2013), All right reserved
*
*  PursePose:
*		Oracle scheduler demo Test.
*
*  Author: 
*		Huang kaide (huang.kaide@eisoo.com)
*  Created Time: 
*		2013-7-21
*     
***************************************************************************************/
#include <abprec.h>
#include <ncutil.h>
#include <public/ncIEEFListener.h>
#include <public/ncIBackupEngine.h>
#include <public/ncIBackupScheduler.h>
#include <public/ncISchedulerConfigurator.h>
#include "ncOracleExecuter.h"
#include "ncOracleBackupEngine.h"

#define ADD_PARAM(key, value)					\
	config->AddParam (key, #value)

#include <iostream>
using namespace std;

void initDll (void)
{
	static AppContext appCtx (AB_APPLICATION_NAME);
	AppContext::setInstance (&appCtx);
	AppSettings* appSettings = AppSettings::getCFLAppSettings ();	
	LibManager::getInstance ()->initLibs (appSettings, &appCtx, 0);

	ncInitXPCOM ();
}

void releaseDll (void)
{
	AppSettings::deleteCFLAppSettings ();
	LibManager::getInstance ()->closeLibs (0);
	LibManager::delInstance ();
}

String
getValue (AppSettings &appSet, const String &key)
{
	String value;
	try {
		value = appSet.getValue (key);
	}
	catch (Exception &e) {
		try {
			vector <String> values;
			appSet.getArrayValue (key, values);

			value = values[0];
		}
		catch (Exception &e) {
			String error;
			error.format (_T("从配置文件中取 %s 值时出错:"), key.getCStr ());
			throw Exception (error);
		}
	}

	return value;
}

vector<String> 
myGetArrayValue (AppSettings &appSet, const String &key) 
{
	vector<String> dbSource;

	try {
		appSet.getArrayValue(key, dbSource);
	}
	catch (Exception &e) {		
		String tmp = getValue (appSet,key);
		dbSource.push_back (tmp);
		return dbSource;
	}
	return dbSource;
}


////////////////////////////////////////////////////////////////////////////////////////
class ncEEFListener : public ncIEEFListener
{
public:
	NS_DECL_ISUPPORTS
		NS_DECL_NCIEEFLISTENER
		ncEEFListener() : _totalsize(0),
		_instance()
	{

	}
	~ncEEFListener()
	{

	}

private:
	int64 _totalsize;
	String _instance;
};

NS_IMPL_ISUPPORTS1(ncEEFListener, ncIEEFListener)

/* [notxpcom] void OnPause (); */
NS_IMETHODIMP_(void) ncEEFListener::OnPause()
{
	printMessage2 (_T("ncEEFListener::OnPause ()"));
}

/* [notxpcom] void OnResume (); */
NS_IMETHODIMP_(void) ncEEFListener::OnResume()
{
	printMessage2 (_T("ncEEFListener::OnResume ()"));
}


/* [notxpcom] void OnProgress ([const] in StringRef message); */
NS_IMETHODIMP_(void) ncEEFListener::OnProgress(int64 size, const String & execInfo)
{
	printMessage2 (_T("ncEEFListener::OnProgress (message: %s)"), execInfo.getCStr ());
	_totalsize += size;
}

/* [notxpcom] void OnMessage ([const] in StringRef message); */
NS_IMETHODIMP_(void) ncEEFListener::OnMessage(const String & message)
{
	printMessage2 (_T("ncEEFListener::OnMessage (message: %s)"), message.getCStr ());
}

/* [notxpcom] void OnWarning ([const] in ncCoreWarnExceptionRef warning); */
NS_IMETHODIMP_(void) ncEEFListener::OnWarning(const ncCoreWarnException & warning, bool partSuccess)
{
	printMessage2 (_T("ncEEFListener::OnWarning (warning: %s)"), warning.toFullString ().getCStr ());
}

/* [notxpcom] void OnError ([const] in ncCoreAbortExceptionRef e); */
NS_IMETHODIMP_(void) ncEEFListener::OnError(const ncCoreAbortException & e)
{
	printMessage2 (_T("ncEEFListener::OnError (warning: %s)"), e.toFullString ().getCStr ());
}

NS_IMETHODIMP_(void) ncEEFListener::OnReport(ncExecReport & execReport)
{

}

NS_IMETHODIMP_(void) ncEEFListener::OnAlarm(const String & reason)
{
	printMessage2 (_T("ncEEFListener::OnAlarm ()"));
}

NS_IMETHODIMP_(void) ncEEFListener::OnExecException(const ncCoreAbortException & e)
{
	printMessage2 (_T("ncEEFListener::OnExecException ()"));
}

NS_IMETHODIMP_(void) ncEEFListener::OnStart()
{
}

NS_IMETHODIMP_(void) ncEEFListener::OnStop()
{
}

NS_IMETHODIMP_(void) ncEEFListener::OnProgressEx(int64 finishSize, const String& execInfo, int64 remainSize)
{
}

NS_IMETHODIMP_(void) ncEEFListener::OnStatus(int status)
{
}

int main()
{
	initDll();
	String configFile = _T("oracleScheduleDemo.config");
	String path = Path::combine (Path::getCurrentModulePath (), configFile);
	AppSettings appSet (path);
	appSet.load ();

	String type = getValue (appSet, _T("TYPE"));
	String server_ip = getValue (appSet, _T("SERVER_IP"));
	String server_port = getValue (appSet, _T("SERVER_PORT"));	
	String disk_id = getValue (appSet, _T("DISKPOOLID"));			
	String sign = getValue (appSet, _T("SIGN"));
	String datect_time = getValue (appSet, _T("DETECT_TIME"));
	String rotation_num = getValue (appSet, _T("ROTATION_NUM"));
	String backup_cid = getValue (appSet, _T("BACKUP_CID"));
	String data_source = getValue (appSet, _T("DATA_SOURCE"));
	String backup_type = getValue (appSet, _T("BACKUP_TYPE"));
	String blk_size = getValue (appSet, _T("DEDUP_BLK_SIZE"));
	String com_cpu_num = getValue (appSet, _T("COMPRESS_CPU_NUM"));
	String backup_compress = getValue (appSet, _T("ORACLE_BACKUP_COMPRESS"));
	String delete_arc = getValue (appSet, _T("ORACLE_BDELETE_ARC"));
	String keep_arc_days = getValue (appSet, _T("ORACLE_BACKUP_ARC_KEEP_DAYS"));
	String bct_file_path = getValue (appSet, _T("ORACLE_BCT_FILE_PATH"));
	String script = getValue(appSet, _T("CUSTOM_SCRIPT"));

	//restore
	String restore_type = getValue(appSet, _T("ORACLE_RESTORE_TYPE"));
	String restore_gns = getValue (appSet, _T("RESTORE_GNS")); 
	String restore_path = getValue (appSet, _T("RESTORE_PATH"));
REPUT:
	if (type == "backup"){
		printMessage2 (_T("当前的操作类型为:%s"),type.getCStr());
		try{
			nsresult result;
			nsCOMPtr <ncIBackupEngine>  engine = do_CreateInstance (NC_ORACLE_BACKUP_ENGINE_CONTRACTID, &result);
			if(NS_FAILED (result)) {
				String error;
				error.format(_T("创建备份引擎失败：%d"), result);
				throw Exception (error);
			}

			engine->SetEngineType (13);
			nsCOMPtr <ncIBackupScheduler> scheduler = engine->CreateBackupScheduler ();
			if (scheduler == 0)
				throw Exception (_T("创建备份调度器失败。"));

			nsCOMPtr <ncISchedulerConfigurator> config = getter_AddRefs (scheduler->GetConfigurator());
			nsCOMPtr <ncEEFListener> listener = new ncEEFListener;
			config->SetListener (listener.get ());

			config->AddParam (EEE_SERVER_IP, server_ip);

			config->AddParam (EEE_SERVER_PORT, server_port);

			config->AddParam (EEE_DISKPOOLID, disk_id);

			config->AddParam (EEE_SIGN, sign);

			config->AddParam (EEE_DETECT_TIME, datect_time);

			config->AddParam (EEE_ROTATION_NUM, rotation_num);

			config->AddParam (EEE_BACKUP_CID, backup_cid);

			config->AddParam(EEE_CUSTOM_SCRIPT, script);

			printMessage2 (_T("准备启动执行..."));
			scheduler->Start ();
			printMessage2 (_T("结束执行..."));
		}
		catch (Exception &e) {
			printMessage2 (_T("未知异常…………\n %s"), e.toFullString ().getCStr ());
		}
		printMessage2 (_T("执行结束…………"));
		return 0;
	}
	else if (type == "restore") {
		printMessage2 (_T("当前的操作类型为:%s"),type.getCStr());
		try{
			nsresult result;
			nsCOMPtr <ncIBackupEngine>  engine = do_CreateInstance (NC_ORACLE_BACKUP_ENGINE_CONTRACTID, &result);
			if(NS_FAILED (result)) {
				String error;
				error.format(_T("创建备份引擎失败：%d"), result);
				throw Exception (error);
			}

			nsCOMPtr <ncIBackupScheduler> scheduler = engine->CreateRestoreScheduler();
			if (scheduler == 0)
				throw Exception (_T("创建恢复调度器失败。"));

			nsCOMPtr <ncISchedulerConfigurator> config = getter_AddRefs (scheduler->GetConfigurator());
			nsCOMPtr <ncEEFListener> listener = new ncEEFListener;
			config->SetListener (listener.get ());

			config->AddParam (EEE_SERVER_IP, server_ip);

			config->AddParam (EEE_SERVER_PORT, server_port);

			config->AddParam (EEE_DISKPOOLID, disk_id);

			config->AddParam (EEE_SIGN, sign);

			config->AddParam (EEE_DETECT_TIME, datect_time);

			config->AddParam (EEE_ORACLE_RESTORE_TYPE, restore_type);

			config->AddParam (EEE_RESTORE_GNS, restore_gns);

			config->AddParam (EEE_RESTORE_PATH, restore_path);

			printMessage2 (_T("准备启动执行..."));
			scheduler->Start ();
			printMessage2 (_T("结束执行..."));
		}
		catch (Exception &e) {
			printMessage2 (_T("未知异常………… %s"), e.toFullString ().getCStr ());
		}
		printMessage2 (_T("执行结束…………"));
		return 0;
	}
	else {
		printMessage2 (_T("输入错误，请重新输入"));
		goto REPUT;
	}
	return 0;
}