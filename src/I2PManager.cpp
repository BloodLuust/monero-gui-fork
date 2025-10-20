#include "I2PManager.h"
#include <QJsonArray>

// Singleton instance pointer
I2PManager* I2PManager::s_instance = nullptr;

I2PManager& I2PManager::instance()
{
    if (!s_instance) {
        s_instance = new I2PManager();
    }
    return *s_instance;
}

I2PManager* I2PManager::getInstance()
{
    if (!s_instance) {
        s_instance = new I2PManager();
    }
    return s_instance;
}

I2PManager::I2PManager(QObject *parent)
    : QObject(parent)
    , m_status(Status::Disconnected)
    , m_statusRefreshInterval(5000) // 5 seconds
    , m_apiHost("127.0.0.1")
    , m_apiPort(7657)
    , m_apiKey("")
{
    initialize();
}

I2PManager::~I2PManager()
{
    if (m_daemonProcess && m_daemonProcess->state() != QProcess::NotRunning) {
        stopI2PDaemon();
    }
}

void I2PManager::initialize()
{
    // Setup daemon process
    setupDaemonProcess();
    
    // Setup default configuration
    setupDefaultConfiguration();
    
    // Initialize network manager
    m_networkManager = std::make_unique<QNetworkAccessManager>(this);
    
    // Setup status timer
    m_statusTimer = std::make_unique<QTimer>(this);
    m_statusTimer->setInterval(m_statusRefreshInterval);
    connect(m_statusTimer.get(), &QTimer::timeout, this, &I2PManager::onStatusTimer);
    
    // Setup directories
    m_configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/i2p";
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/i2p";
    
    // Create directories if they don't exist
    QDir().mkpath(m_configDir);
    QDir().mkpath(m_dataDir);
    
    // Get platform-specific daemon path
    m_daemonPath = getPlatformDaemonPath();
    
    qDebug() << "I2PManager initialized";
    qDebug() << "Daemon path:" << m_daemonPath;
    qDebug() << "Config dir:" << m_configDir;
    qDebug() << "Data dir:" << m_dataDir;
}

void I2PManager::setupDaemonProcess()
{
    m_daemonProcess = std::make_unique<QProcess>(this);
    
    // Connect process signals
    connect(m_daemonProcess.get(), &QProcess::stateChanged, 
            this, &I2PManager::onProcessStateChanged);
    connect(m_daemonProcess.get(), &QProcess::errorOccurred, 
            this, &I2PManager::onProcessError);
    connect(m_daemonProcess.get(), &QProcess::readyReadStandardOutput, 
            this, &I2PManager::onProcessOutput);
    connect(m_daemonProcess.get(), &QProcess::readyReadStandardError, 
            this, &I2PManager::onProcessOutput);
    connect(m_daemonProcess.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &I2PManager::onProcessFinished);
    
    // Set process environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("I2P", m_dataDir);
    m_daemonProcess->setProcessEnvironment(env);
}

void I2PManager::setupDefaultConfiguration()
{
    m_configuration = QJsonObject{
        {"enabled", true},
        {"proxyHost", "127.0.0.1"},
        {"proxyPort", 4447},
        {"httpTunnelPort", 4444},
        {"socksTunnelPort", 4447},
        {"tunnelName", "monero-gui"},
        {"bandwidthLimit", 0},
        {"maxConnections", 100},
        {"enableUPnP", false},
        {"enableFloodfill", false},
        {"enableReseed", true},
        {"reseedURL", "https://reseed.i2p.net"},
        {"logLevel", "INFO"},
        {"logFile", ""},
        {"routerConfig", QJsonObject{
            {"port", 7654},
            {"host", "127.0.0.1"},
            {"enableUPnP", false},
            {"enableSSU", true},
            {"enableNTCP", true}
        }}
    };
}

bool I2PManager::running() const
{
    return m_status == Status::Connected;
}

I2PManager::Status I2PManager::status() const
{
    return m_status;
}

void I2PManager::start()
{
    if (startI2PDaemon()) {
        // Wait for I2P to be ready and get SOCKS address
        QString socksAddress = QString("127.0.0.1:4447");
        emit i2pReady(true, socksAddress);
    } else {
        emit i2pReady(false, QString());
    }
}

void I2PManager::stop()
{
    if (stopI2PDaemon()) {
        emit i2pStopped();
    }
}

void I2PManager::generateNewIdentity()
{
    if (m_status != Status::Connected) {
        m_lastError = "I2P daemon not connected";
        emit errorOccurred(m_lastError);
        return;
    }
    
    // Send command to generate new identity
    QString command = "router generateIdentity";
    if (sendDaemonCommand(command)) {
        qDebug() << "New I2P identity generation requested";
    } else {
        m_lastError = "Failed to generate new identity";
        emit errorOccurred(m_lastError);
    }
}

bool I2PManager::startI2PDaemon()
{
    if (m_status == Status::Connected || m_status == Status::Starting) {
        qDebug() << "I2P daemon already running or starting";
        return true;
    }
    
    if (!QFileInfo::exists(m_daemonPath)) {
        m_lastError = QString("I2P daemon not found at: %1").arg(m_daemonPath);
        emit errorOccurred(m_lastError);
        return false;
    }
    
    // Create I2P configuration file
    if (!createI2PConfigFile()) {
        m_lastError = "Failed to create I2P configuration file";
        emit errorOccurred(m_lastError);
        return false;
    }
    
    // Set daemon arguments
    QStringList arguments;
    arguments << "-c" << m_configDir;
    arguments << "-d" << m_dataDir;
    arguments << "-l" << "INFO";
    
    // Start daemon process
    m_daemonProcess->setProgram(m_daemonPath);
    m_daemonProcess->setArguments(arguments);
    
    qDebug() << "Starting I2P daemon:" << m_daemonPath << arguments;
    
    m_daemonProcess->start();
    
    if (!m_daemonProcess->waitForStarted(5000)) {
        m_lastError = QString("Failed to start I2P daemon: %1").arg(m_daemonProcess->errorString());
        emit errorOccurred(m_lastError);
        return false;
    }
    
    updateStatus();
    return true;
}

bool I2PManager::stopI2PDaemon()
{
    if (m_status == Status::Disconnected || m_status == Status::Stopping) {
        qDebug() << "I2P daemon not running";
        return true;
    }
    
    qDebug() << "Stopping I2P daemon";
    
    // Stop status timer
    if (m_statusTimer->isActive()) {
        m_statusTimer->stop();
    }
    
    // Send shutdown command to daemon
    sendDaemonCommand("shutdown");
    
    // Wait for process to finish
    if (!m_daemonProcess->waitForFinished(10000)) {
        qDebug() << "I2P daemon did not stop gracefully, terminating";
        m_daemonProcess->terminate();
        
        if (!m_daemonProcess->waitForFinished(5000)) {
            qDebug() << "I2P daemon did not terminate, killing";
            m_daemonProcess->kill();
        }
    }
    
    updateStatus();
    return true;
}

bool I2PManager::restartI2PDaemon()
{
    qDebug() << "Restarting I2P daemon";
    
    if (m_status == Status::Connected || m_status == Status::Starting) {
        if (!stopI2PDaemon()) {
            return false;
        }
        
        // Wait a bit before restarting
        QThread::msleep(2000);
    }
    
    return startI2PDaemon();
}

I2PManager::Status I2PManager::getStatus() const
{
    return m_status;
}

bool I2PManager::isRunning() const
{
    return m_status == Status::Connected;
}

void I2PManager::setConfiguration(const QJsonObject& config)
{
    m_configuration = config;
    
    // Validate configuration
    if (!validateConfiguration(config)) {
        m_lastError = "Invalid I2P configuration";
        emit errorOccurred(m_lastError);
        return;
    }
    
    // Save configuration to file
    saveConfiguration(m_configDir + "/i2p.conf");
    
    qDebug() << "I2P configuration updated";
}

QJsonObject I2PManager::getConfiguration() const
{
    return m_configuration;
}

bool I2PManager::loadConfiguration(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QString("Failed to open configuration file: %1").arg(filePath);
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        m_lastError = QString("Failed to parse configuration file: %1").arg(error.errorString());
        return false;
    }
    
    if (!doc.isObject()) {
        m_lastError = "Configuration file does not contain a valid JSON object";
        return false;
    }
    
    m_configuration = doc.object();
    return true;
}

bool I2PManager::saveConfiguration(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    QJsonDocument doc(m_configuration);
    QByteArray data = doc.toJson(QJsonDocument::Indented);
    
    return file.write(data) == data.size();
}

bool I2PManager::createTunnel(const TunnelConfig& config)
{
    if (m_status != Status::Connected) {
        m_lastError = "I2P daemon not connected";
        emit errorOccurred(m_lastError);
        return false;
    }
    
    // Create tunnel configuration
    QJsonObject tunnelConfig;
    tunnelConfig["name"] = config.name;
    tunnelConfig["type"] = (config.type == TunnelType::HTTP) ? "http" : "socks";
    tunnelConfig["port"] = config.localPort;
    tunnelConfig["enabled"] = config.enabled;
    
    if (config.type == TunnelType::Client) {
        tunnelConfig["target"] = config.targetHost;
        tunnelConfig["targetPort"] = config.targetPort;
    }
    
    // Send tunnel creation command
    QString command = QString("tunnel create %1").arg(QJsonDocument(tunnelConfig).toJson(QJsonDocument::Compact));
    
    if (!sendDaemonCommand(command)) {
        m_lastError = "Failed to create tunnel";
        emit errorOccurred(m_lastError);
        return false;
    }
    
    // Update tunnels list
    updateTunnels();
    
    emit tunnelCreated(config.name);
    return true;
}

bool I2PManager::destroyTunnel(const QString& tunnelId)
{
    if (m_status != Status::Connected) {
        m_lastError = "I2P daemon not connected";
        emit errorOccurred(m_lastError);
        return false;
    }
    
    QString command = QString("tunnel destroy %1").arg(tunnelId);
    
    if (!sendDaemonCommand(command)) {
        m_lastError = "Failed to destroy tunnel";
        emit errorOccurred(m_lastError);
        return false;
    }
    
    // Update tunnels list
    updateTunnels();
    
    emit tunnelDestroyed(tunnelId);
    return true;
}

QList<I2PManager::TunnelInfo> I2PManager::getTunnels() const
{
    return m_tunnels.values();
}

I2PManager::TunnelInfo I2PManager::getTunnel(const QString& tunnelId) const
{
    return m_tunnels.value(tunnelId, TunnelInfo());
}

bool I2PManager::setTunnelEnabled(const QString& tunnelId, bool enabled)
{
    if (m_status != Status::Connected) {
        m_lastError = "I2P daemon not connected";
        emit errorOccurred(m_lastError);
        return false;
    }
    
    QString command = QString("tunnel %1 %2").arg(enabled ? "enable" : "disable").arg(tunnelId);
    
    if (!sendDaemonCommand(command)) {
        m_lastError = "Failed to change tunnel status";
        emit errorOccurred(m_lastError);
        return false;
    }
    
    // Update tunnels list
    updateTunnels();
    
    emit tunnelStatusChanged(tunnelId, enabled);
    return true;
}

I2PManager::NetworkStats I2PManager::getNetworkStats() const
{
    return m_networkStats;
}

QString I2PManager::getRouterInfo() const
{
    if (m_status != Status::Connected) {
        return "I2P daemon not connected";
    }
    
    // This would typically make an API call to get router info
    return QString("I2P Router - Status: Connected, Peers: %1").arg(m_networkStats.peersCount);
}

bool I2PManager::isNetworkConnected() const
{
    return m_status == Status::Connected && m_networkStats.peersCount > 0;
}

QString I2PManager::getI2PDaemonPath() const
{
    return m_daemonPath;
}

QString I2PManager::getI2PConfigDir() const
{
    return m_configDir;
}

QString I2PManager::getI2PDataDir() const
{
    return m_dataDir;
}

QString I2PManager::getLastError() const
{
    return m_lastError;
}

void I2PManager::refreshStatus()
{
    if (m_status != Status::Connected) {
        return;
    }
    
    // Make API call to get current status
    QUrl url(QString("http://%1:%2/api/status").arg(m_apiHost).arg(m_apiPort));
    QNetworkRequest request(url);
    
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &I2PManager::onNetworkReplyFinished);
}

void I2PManager::updateTunnels()
{
    if (m_status != Status::Connected) {
        return;
    }
    
    // Make API call to get tunnel information
    QUrl url(QString("http://%1:%2/api/tunnels").arg(m_apiHost).arg(m_apiPort));
    QNetworkRequest request(url);
    
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &I2PManager::onNetworkReplyFinished);
}

void I2PManager::onProcessStateChanged(QProcess::ProcessState newState)
{
    qDebug() << "I2P daemon process state changed:" << newState;
    
    switch (newState) {
    case QProcess::NotRunning:
        if (m_status != Status::Stopping) {
            updateStatus();
        }
        break;
    case QProcess::Starting:
        updateStatus();
        break;
    case QProcess::Running:
        updateStatus();
        break;
    }
}

void I2PManager::onProcessError(QProcess::ProcessError error)
{
    qDebug() << "I2P daemon process error:" << error;
    
    m_lastError = QString("I2P daemon process error: %1").arg(m_daemonProcess->errorString());
    emit errorOccurred(m_lastError);
    
    updateStatus();
}

void I2PManager::onProcessOutput()
{
    QByteArray output = m_daemonProcess->readAllStandardOutput();
    QString outputStr = QString::fromUtf8(output);
    
    qDebug() << "I2P daemon output:" << outputStr;
    
    parseDaemonOutput(outputStr);
}

void I2PManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "I2P daemon process finished, exit code:" << exitCode << "exit status:" << exitStatus;
    
    updateStatus();
    
    if (exitStatus == QProcess::CrashExit) {
        m_lastError = "I2P daemon crashed";
        emit errorOccurred(m_lastError);
    }
}

void I2PManager::onStatusTimer()
{
    refreshStatus();
}

void I2PManager::onNetworkReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Network request failed:" << reply->errorString();
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "Failed to parse JSON response:" << error.errorString();
        reply->deleteLater();
        return;
    }
    
    QJsonObject json = doc.object();
    
    // Parse response based on URL
    QString url = reply->url().toString();
    if (url.contains("/api/status")) {
        m_networkStats = parseNetworkStats(json);
        emit networkStatsChanged(m_networkStats);
    } else if (url.contains("/api/tunnels")) {
        QList<TunnelInfo> tunnels = parseTunnelInfo(json);
        m_tunnels.clear();
        for (const auto& tunnel : tunnels) {
            m_tunnels.insert(tunnel.id, tunnel);
        }
    }
    
    reply->deleteLater();
}

void I2PManager::updateStatus()
{
    Status newStatus = Status::Disconnected;
    
    if (m_daemonProcess) {
        switch (m_daemonProcess->state()) {
        case QProcess::NotRunning:
            newStatus = Status::Disconnected;
            break;
        case QProcess::Starting:
            newStatus = Status::Starting;
            break;
        case QProcess::Running:
            newStatus = Status::Connected;
            break;
        }
    }
    
    if (newStatus != m_status) {
        bool wasRunning = (m_status == Status::Connected);
        bool isRunning = (newStatus == Status::Connected);
        
        m_status = newStatus;
        emit statusChanged(m_status);
        
        // Emit runningChanged signal if running status changed
        if (wasRunning != isRunning) {
            emit runningChanged(isRunning);
        }
        
        if (m_status == Status::Connected) {
            emit daemonReady();
            m_statusTimer->start();
        } else {
            m_statusTimer->stop();
        }
    }
}

void I2PManager::parseDaemonOutput(const QString& output)
{
    // Parse daemon output for status information
    if (output.contains("I2P router started")) {
        updateStatus();
    } else if (output.contains("I2P router stopped")) {
        updateStatus();
    } else if (output.contains("error", Qt::CaseInsensitive)) {
        m_lastError = output;
        emit errorOccurred(m_lastError);
    }
}

bool I2PManager::sendDaemonCommand(const QString& command)
{
    if (m_status != Status::Connected) {
        return false;
    }
    
    // Send command to daemon via API
    QUrl url(QString("http://%1:%2/api/command").arg(m_apiHost).arg(m_apiPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }
    
    QJsonObject commandObj;
    commandObj["command"] = command;
    
    QJsonDocument doc(commandObj);
    QByteArray data = doc.toJson();
    
    QNetworkReply* reply = m_networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, &I2PManager::onNetworkReplyFinished);
    
    return true;
}

QList<I2PManager::TunnelInfo> I2PManager::parseTunnelInfo(const QJsonObject& json) const
{
    QList<TunnelInfo> tunnels;
    
    QJsonArray tunnelArray = json["tunnels"].toArray();
    for (const QJsonValue& value : tunnelArray) {
        QJsonObject tunnelObj = value.toObject();
        
        TunnelInfo tunnel;
        tunnel.id = tunnelObj["id"].toString();
        tunnel.name = tunnelObj["name"].toString();
        tunnel.type = (tunnelObj["type"].toString() == "http") ? TunnelType::HTTP : TunnelType::SOCKS;
        tunnel.localPort = tunnelObj["port"].toInt();
        tunnel.targetHost = tunnelObj["target"].toString();
        tunnel.targetPort = tunnelObj["targetPort"].toInt();
        tunnel.enabled = tunnelObj["enabled"].toBool();
        tunnel.status = tunnelObj["status"].toString();
        
        tunnels.append(tunnel);
    }
    
    return tunnels;
}

I2PManager::NetworkStats I2PManager::parseNetworkStats(const QJsonObject& json) const
{
    NetworkStats stats;
    
    stats.activeTunnels = json["activeTunnels"].toInt();
    stats.inboundBandwidth = json["inboundBandwidth"].toInt();
    stats.outboundBandwidth = json["outboundBandwidth"].toInt();
    stats.peersCount = json["peersCount"].toInt();
    stats.networkID = json["networkID"].toString();
    stats.anonymityLevel = json["anonymityLevel"].toDouble();
    stats.floodfillEnabled = json["floodfillEnabled"].toBool();
    
    return stats;
}

QString I2PManager::getPlatformDaemonPath() const
{
    QString basePath = QCoreApplication::applicationDirPath();
    
#ifdef Q_OS_WIN
    return basePath + "/i2pd.exe";
#elif defined(Q_OS_MACOS)
    return basePath + "/i2pd";
#elif defined(Q_OS_LINUX)
    return basePath + "/i2pd";
#elif defined(Q_OS_ANDROID)
    return basePath + "/i2pd";
#else
    return basePath + "/i2pd";
#endif
}

bool I2PManager::createI2PConfigFile() const
{
    QString configPath = m_configDir + "/i2p.conf";
    
    // Create basic I2P configuration
    QString config = QString(R"(
# I2P Configuration for Monero GUI
router.name=Monero GUI I2P Router
router.description=I2P Router for Monero GUI
router.port=7654
router.host=127.0.0.1
router.enableUPnP=false
router.enableSSU=true
router.enableNTCP=true

# Proxy settings
proxy.host=127.0.0.1
proxy.port=4447
proxy.enabled=true

# Tunnel settings
tunnel.name=monero-gui
tunnel.port=4444
tunnel.enabled=true

# Logging
log.level=INFO
log.file=

# Network settings
network.enableFloodfill=false
network.enableReseed=true
network.reseedURL=https://reseed.i2p.net
)");
    
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    return file.write(config.toUtf8()) == config.size();
}

bool I2PManager::validateConfiguration(const QJsonObject& config) const
{
    // Basic validation
    if (!config.contains("enabled") || !config["enabled"].isBool()) {
        return false;
    }
    
    if (!config.contains("proxyHost") || !config["proxyHost"].isString()) {
        return false;
    }
    
    if (!config.contains("proxyPort") || !config["proxyPort"].isDouble()) {
        return false;
    }
    
    return true;
}
