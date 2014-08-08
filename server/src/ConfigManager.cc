/*
 * Copyright (C) 2013-2014 Project Hatohol
 *
 * This file is part of Hatohol.
 *
 * Hatohol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hatohol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hatohol. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include <Mutex.h>
#include "ConfigManager.h"
#include "DBClientConfig.h"
#include "Reaper.h"
using namespace std;
using namespace mlpl;

enum {
	CONF_MGR_ERROR_NULL,
	CONF_MGR_ERROR_INVALID_PORT,
};

const char *ConfigManager::HATOHOL_DB_DIR_ENV_VAR_NAME = "HATOHOL_DB_DIR";
static const char *DEFAULT_DATABASE_DIR = "/tmp";
static const size_t DEFAULT_NUM_PRESERVED_REPLICA_GENERATION = 3;

int ConfigManager::ALLOW_ACTION_FOR_ALL_OLD_EVENTS;
static int DEFAULT_ALLOWED_TIME_OF_ACTION_FOR_OLD_EVENTS
  = 60 * 60 * 24; // 24 hours
const char *ConfigManager::DEFAULT_PID_FILE_PATH = LOCALSTATEDIR "run/hatohol.pid";

static int DEFAULT_MAX_NUM_RUNNING_COMMAND_ACTION = 10;

struct CommandLineOptions {
	gchar    *pidFilePath;
	gchar    *dbServer;
	gboolean  foreground;
	gboolean  testMode;
	gboolean  enableCopyOnDemand;
	gboolean  disableCopyOnDemand;
	gint      faceRestPort;

	CommandLineOptions(void)
	: pidFilePath(NULL),
	  dbServer(NULL),
	  foreground(FALSE),
	  testMode(FALSE),
	  enableCopyOnDemand(FALSE),
	  disableCopyOnDemand(FALSE),
	  faceRestPort(0)
	{
	}

	virtual ~CommandLineOptions()
	{
		clear();
	}

	void clear(void)
	{
		g_free(pidFilePath);
		pidFilePath = NULL;

		g_free(dbServer);
		dbServer = NULL;

		foreground = FALSE;
		testMode   = FALSE;

		enableCopyOnDemand = FALSE;
		disableCopyOnDemand = FALSE;

		faceRestPort = 0;
	}

	static gboolean parseFaceRestPort(
	  const gchar *option_name, const gchar *value,
	  gpointer data, GError **error)
	{
		GQuark quak =
		  g_quark_from_static_string("config-manager-quark");
		CommandLineOptions *obj =
		  static_cast<CommandLineOptions *>(data);
		if (!value) {
			g_set_error(error, quak, CONF_MGR_ERROR_NULL,
			            "value is NULL.");
			return FALSE;
		}
		int port = atoi(value);
		if (!Utils::isValidPort(port, false)) {
			g_set_error(error, quak, CONF_MGR_ERROR_INVALID_PORT,
			            "value: %s, %d.", value, port);
			return FALSE;
		}

		obj->faceRestPort = port;
		return TRUE;
	}
};
static CommandLineOptions g_cmdLineOpts;

struct ConfigManager::Impl {
	static Mutex          mutex;
	static ConfigManager *instance;
	string                confFilePath;
	string                databaseDirectory;
	string                actionCommandDirectory;
	string                residentYardDirectory;
	bool                  foreground;
	string                dbServerAddress;
	int                   dbServerPort;
	bool                  testMode;
	ConfigState           copyOnDemand;
	int                   faceRestPort;
	string                pidFilePath;

	// methods
	Impl(void)
	: foreground(false),
	  dbServerAddress("localhost"),
	  dbServerPort(0),
	  testMode(false),
	  copyOnDemand(UNKNOWN),
	  faceRestPort(0),
	  pidFilePath(DEFAULT_PID_FILE_PATH)
	{
	}

	static void lock(void)
	{
		mutex.lock();
	}

	static void unlock(void)
	{
		mutex.unlock();
	}

	void parseDBServer(const string &dbServer)
	{
		const size_t posColon = dbServer.find(":");
		if (posColon == string::npos) {
			dbServerAddress = dbServer;
			return;
		}
		if (posColon == dbServer.size() - 1) {
			MLPL_ERR("A column must not be the tail: %s\n",
			         dbServer.c_str());
			return;
		}
		dbServerAddress = string(dbServer, 0, posColon);
		dbServerPort = atoi(&dbServer.c_str()[posColon+1]);
	}

	bool loadConfFile(const string &path)
	{
		if (path.empty())
			return false;

		GKeyFile *keyFile = g_key_file_new();
		if (!keyFile) {
			MLPL_CRIT("Failed to call g_key_file_new().\n");
			return false;
		}
		Reaper<GKeyFile> keyFileReaper(keyFile, g_key_file_unref);

		GError *error = NULL;
		gboolean succeeded =
		  g_key_file_load_from_file(keyFile, path.c_str(),
		                            G_KEY_FILE_NONE, &error);
		if (!succeeded) {
			MLPL_DBG("Failed to load config file: %s (%s)\n",
			         path.c_str(),
			         error ? error->message : "Unknown reason");
			g_error_free(error);
			return false;
		}

		return true;
	}

	void reflectCommandLineOptions(const CommandLineOptions &cmdLineOpts)
	{
		if (cmdLineOpts.dbServer)
			parseDBServer(cmdLineOpts.dbServer);
		if (cmdLineOpts.foreground)
			foreground = true;
		if (cmdLineOpts.testMode)
			testMode = true;
		if (cmdLineOpts.enableCopyOnDemand)
			copyOnDemand = ENABLE;
		if (cmdLineOpts.disableCopyOnDemand)
			copyOnDemand = DISABLE;
		if (cmdLineOpts.faceRestPort)
			faceRestPort = cmdLineOpts.faceRestPort;
		if (cmdLineOpts.pidFilePath)
			pidFilePath = cmdLineOpts.pidFilePath;
	}
};

Mutex          ConfigManager::Impl::mutex;
ConfigManager *ConfigManager::Impl::instance = NULL;

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------
bool ConfigManager::parseCommandLine(gint *argc, gchar ***argv)
{
	CommandLineOptions *cmdLineOpts = &g_cmdLineOpts;
	static GOptionEntry entries[] = {
		{"pid-file-path", 'p', 0, G_OPTION_ARG_STRING,
		 &cmdLineOpts->pidFilePath, "Pid file path", NULL},
		{"foreground", 'f', 0, G_OPTION_ARG_NONE,
		 &cmdLineOpts->foreground, "Run as a foreground process", NULL},
		{"test-mode", 't', 0, G_OPTION_ARG_NONE,
		 &cmdLineOpts->testMode, "Run in a test mode", NULL},
		{"config-db-server", 'c', 0, G_OPTION_ARG_STRING,
		 &cmdLineOpts->dbServer, "Database server", NULL},
		{"enable-copy-on-demand", 'd', 0, G_OPTION_ARG_NONE,
		 &cmdLineOpts->enableCopyOnDemand,
		 "Current monitoring values are obtained on demand.", NULL},
		{"disable-copy-on-demand", 'd', 0, G_OPTION_ARG_NONE,
		 &cmdLineOpts->disableCopyOnDemand,
		 "Current monitoring values are obtained periodically.", NULL},
		{"face-rest-port", 'r', 0, G_OPTION_ARG_CALLBACK,
		 (gpointer)CommandLineOptions::parseFaceRestPort,
		 "Port of FaceRest", NULL},
		{ NULL }
	};

	GOptionContext *optCtx = g_option_context_new(NULL);
	GOptionGroup *optGrp = g_option_group_new(
	  "ConfigManager", "ConfigManager group", "ConfigManager",
	  cmdLineOpts, NULL);
	g_option_context_set_main_group(optCtx, optGrp);
	//g_option_context_add_group(optCtx, optGrp);
	g_option_context_add_main_entries(optCtx, entries, NULL);
	GError *error = NULL;
	if (!g_option_context_parse(optCtx, argc, argv, &error)) {
		MLPL_ERR("Failed to parse command line argment. (%s)\n",
		         error ? error->message : "Unknown reason");
		g_error_free(error);
		return false;
	}

	// refect options so that ConfigManager can return them
	// even before reset() is called.
	getInstance()->m_impl->reflectCommandLineOptions(g_cmdLineOpts);
	return true;
}

void ConfigManager::clearParseCommandLineResult(void)
{
	g_cmdLineOpts.clear();
}

void ConfigManager::reset(void)
{
	delete Impl::instance;
	Impl::instance = NULL;
	ConfigManager *confMgr = getInstance();
	confMgr->loadConfFile();

	confMgr->m_impl->actionCommandDirectory =
	  StringUtils::sprintf("%s/%s/action", LIBEXECDIR, PACKAGE);
	confMgr->m_impl->residentYardDirectory = string(PREFIX"/sbin");

	// override by the command line options if needed
	CommandLineOptions *optVal = &g_cmdLineOpts;
	Impl *impl = confMgr->m_impl.get();
	if (optVal->dbServer)
		impl->parseDBServer(optVal->dbServer);
	if (optVal->foreground)
		impl->foreground = true;
	if (optVal->testMode)
		impl->testMode = true;
	confMgr->m_impl->reflectCommandLineOptions(g_cmdLineOpts);
}

ConfigManager *ConfigManager::getInstance(void)
{
	if (Impl::instance)
		return Impl::instance;

	Impl::lock();
	if (!Impl::instance)
		Impl::instance = new ConfigManager();
	Impl::unlock();

	return Impl::instance;
}

void ConfigManager::getTargetServers
  (MonitoringServerInfoList &monitoringServers, ServerQueryOption &option)
{
	DBClientConfig dbConfig;
	dbConfig.getTargetServers(monitoringServers, option);
}

const string &ConfigManager::getDatabaseDirectory(void) const
{
	return m_impl->databaseDirectory;
}

size_t ConfigManager::getNumberOfPreservedReplicaGeneration(void) const
{
	return DEFAULT_NUM_PRESERVED_REPLICA_GENERATION;
}

bool ConfigManager::isForegroundProcess(void) const
{
	return m_impl->foreground;
}

string ConfigManager::getDBServerAddress(void) const
{
	return m_impl->dbServerAddress;
}

int ConfigManager::getDBServerPort(void) const
{
	return m_impl->dbServerPort;
}

int ConfigManager::getAllowedTimeOfActionForOldEvents(void)
{
	return DEFAULT_ALLOWED_TIME_OF_ACTION_FOR_OLD_EVENTS;
}

int ConfigManager::getMaxNumberOfRunningCommandAction(void)
{
	return DEFAULT_MAX_NUM_RUNNING_COMMAND_ACTION;
}

string ConfigManager::getActionCommandDirectory(void)
{
	AutoMutex autoLock(&m_impl->mutex);
	return m_impl->actionCommandDirectory;
}

void ConfigManager::setActionCommandDirectory(const string &dir)
{
	AutoMutex autoLock(&m_impl->mutex);
	m_impl->actionCommandDirectory = dir;
}

string ConfigManager::getResidentYardDirectory(void)
{
	AutoMutex autoLock(&m_impl->mutex);
	return m_impl->residentYardDirectory;
}

void ConfigManager::setResidentYardDirectory(const string &dir)
{
	AutoMutex autoLock(&m_impl->mutex);
	m_impl->residentYardDirectory = dir;
}

bool ConfigManager::isTestMode(void) const
{
	return m_impl->testMode;
}

ConfigManager::ConfigState ConfigManager::getCopyOnDemand(void) const
{
	return m_impl->copyOnDemand;
}

int ConfigManager::getFaceRestPort(void) const
{
	return m_impl->faceRestPort;
}

string ConfigManager::getPidFilePath(void) const
{
	return m_ctx->pidFilePath;
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------
void ConfigManager::loadConfFile(void)
{
	vector<string> confFiles;
	confFiles.push_back(m_impl->confFilePath);

	char *systemWideConfFile =
	   g_build_filename(SYSCONFDIR, PACKAGE_NAME, "hatohol.conf", NULL);
	confFiles.push_back(systemWideConfFile);
	g_free(systemWideConfFile);

	for (size_t i = 0; i < confFiles.size(); i++) {
		const bool succeeded = m_impl->loadConfFile(confFiles[i]);
		if (!succeeded)
			continue;
		MLPL_INFO("Use configuration file: %s\n",
		          confFiles[i].c_str());
		break;
	}
}

// ---------------------------------------------------------------------------
// Private methods
// ---------------------------------------------------------------------------
ConfigManager::ConfigManager(void)
: m_impl(new Impl())
{
	const char *envDBDir = getenv(HATOHOL_DB_DIR_ENV_VAR_NAME);
	if (envDBDir)
		m_impl->databaseDirectory = envDBDir;
	else
		m_impl->databaseDirectory = DEFAULT_DATABASE_DIR;
}

ConfigManager::~ConfigManager()
{
}
