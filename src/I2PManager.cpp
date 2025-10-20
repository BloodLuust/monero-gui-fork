#include "I2PManager.h"

// Qt Includes for Core functionality, File System, and Processes
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcess>
#include <QDebug>

// C++ Standard Library Includes
#include <memory>

// Singleton instance pointer
I2PManager* I2PManager::s_instance = nullptr;

I2PManager* I2PManager::instance()
{
    if (!s_instance) {
        s_instance = new I2PManager();
    }
    return s_instance;
}

I2PManager::I2PManager(QObject *parent)
    : QObject(parent)
    , m_status(Status::Disconnected)
    , m_socksProxyReady(false)
    , m_networkReady(false)
{
    initialize();
}

I2PManager::~I2PManager()
{
    if (m_daemonProcess && m_daemonProcess->state()!= QProcess::NotRunning) {
        stop();
    }
}

void I2PManager::initialize()
{
    // Setup directories
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/i2p";
    QDir().mkpath(m_dataDir);

    // Get platform-specific daemon path
    m_daemonPath = getPlatformDaemonPath();

    // Setup daemon process
    setupDaemonProcess();

    qDebug() << "I2PManager initialized";
    qDebug() << "Daemon path:" << m_daemonPath;
    qDebug() << "Data dir:" << m_dataDir;
}

void I2PManager::setupDaemonProcess()
{
    m_daemonProcess = std::make_unique<QProcess>(this);

    // Connect process signals
    connect(m_daemonProcess.get(), &QProcess::stateChanged, this, &I2PManager::onProcessStateChanged);
    connect(m_daemonProcess.get(), &QProcess::errorOccurred, this, &I2PManager::onProcessError);
    connect(m_daemonProcess.get(), &QProcess::readyReadStandardOutput, this, &I2PManager::onProcessOutput);
    connect(m_daemonProcess.get(), &QProcess::readyReadStandardError, this, &I2PManager::onProcessOutput);
    connect(m_daemonProcess.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &I2PManager::onProcessFinished);
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
    if (m_status == Status::Connected |

| m_status == Status::Starting) {
        qDebug() << "I2P daemon already running or starting";
        return;
    }

    if (!QFileInfo::exists(m_daemonPath)) {
        m_lastError = QString("I2P daemon not found at: %1").arg(m_daemonPath);
        qCritical() << m_lastError;
        emit errorOccurred(m_lastError);
        emit i2pReady(false, QString());
        return;
    }

    // Reset readiness flags
    m_socksProxyReady = false;
    m_networkReady = false;

    // Set up process arguments for i2pd
    QStringList arguments;
    arguments << "--daemon=false";
    arguments << "--log=stdout";
    arguments << "--loglevel=info";
    arguments << "--socksproxy.port=4447";
    arguments << "--datadir=" + m_dataDir;

    m_daemonProcess->setProgram(m_daemonPath);
    m_daemonProcess->setArguments(arguments);

    qDebug() << "Starting I2P daemon:" << m_daemonPath << arguments;

    m_status = Status::Starting;
    emit statusChanged(m_status);

    m_daemonProcess->start();
}

void I2PManager::stop()
{
    if (m_status == Status::Disconnected |

| m_status == Status::Stopping) {
        qDebug() << "I2P daemon not running or already stopping";
        return;
    }

    qDebug() << "Stopping I2P daemon...";
    m_status = Status::Stopping;
    emit statusChanged(m_status);

    if (m_daemonProcess && m_daemonProcess->state() == QProcess::Running) {
        m_daemonProcess->terminate();
        if (!m_daemonProcess->waitForFinished(10000)) {
            qWarning() << "I2P daemon did not stop gracefully, killing process";
            m_daemonProcess->kill();
        }
    } else {
        onProcessFinished(0, QProcess::NormalExit);
    }
}

void I2PManager::generateNewIdentity()
{
    qDebug() << "Generating new I2P identity...";

    auto restartAfterDeletion = [this]() {
        // Delete key files from the managed data directory
        QStringList dirsToRemove = { "netDb", "router" };
        for (const QString& dirName : dirsToRemove) {
            QDir dir(m_dataDir + "/" + dirName);
            if (dir.exists()) {
                if (dir.removeRecursively()) {
                    qDebug() << "Removed directory:" << dir.path();
                } else {
                    qWarning() << "Failed to remove directory:" << dir.path();
                }
            }
        }
        qDebug() << "I2P identity files removed, restarting daemon.";
        start();
    };

    if (m_daemonProcess->state()!= QProcess::NotRunning) {
        connect(m_daemonProcess.get(), &QProcess::finished, this, restartAfterDeletion, Qt::SingleShotConnection);
        stop();
    } else {
        restartAfterDeletion();
    }
}

QString I2PManager::getPlatformDaemonPath() const
{
    QString basePath = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MACOS
    // On macOS, executables are in Contents/MacOS inside the app bundle
    basePath += "/../";
#endif
    
#if defined(Q_OS_WIN)
    return basePath + "/i2pd.exe";
#else
    return basePath + "/i2pd";
#endif
}

void I2PManager::onProcessStateChanged(QProcess::ProcessState newState)
{
    qDebug() << "I2P daemon process state changed:" << newState;
}

void I2PManager::onProcessError(QProcess::ProcessError error)
{
    if (m_status == Status::Stopping) return;

    m_lastError = QString("I2P daemon process error: %1").arg(m_daemonProcess->errorString());
    qCritical() << m_lastError;
    emit errorOccurred(m_lastError);

    m_status = Status::Error;
    emit statusChanged(m_status);
    emit runningChanged(false);
    emit i2pReady(false, QString());
}

void I2PManager::onProcessOutput()
{
    QByteArray data = m_daemonProcess->readAllStandardOutput();
    if (data.isEmpty()) {
        data = m_daemonProcess->readAllStandardError();
    }
    QString output = QString::fromUtf8(data).trimmed();

    for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
        qDebug() << "i2pd:" << line;
        parseDaemonOutput(line);
    }
}

void I2PManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "I2P daemon process finished, exit code:" << exitCode << "exit status:" << exitStatus;

    if (exitStatus == QProcess::CrashExit) {
        m_lastError = "I2P daemon crashed";
        qCritical() << m_lastError;
        emit errorOccurred(m_lastError);
        m_status = Status::Error;
    } else {
        m_status = Status::Disconnected;
    }

    emit statusChanged(m_status);
    emit runningChanged(false);
    emit i2pStopped();
}

void I2PManager::parseDaemonOutput(const QString& output)
{
    if (m_status!= Status::Starting) {
        return;
    }

    if (output.contains("SOCKS proxy started", Qt::CaseInsensitive)) {
        qDebug() << "I2PManager: SOCKS proxy confirmed ready.";
        m_socksProxyReady = true;
    }

    if (output.contains("Network status: OK", Qt::CaseInsensitive)) {
        qDebug() << "I2PManager: Network status confirmed OK.";
        m_networkReady = true;
    }

    if (m_socksProxyReady && m_networkReady) {
        m_status = Status::Connected;
        emit statusChanged(m_status);
        emit runningChanged(true);
        
        QString socksAddress = "127.0.0.1:4447";
        emit i2pReady(true, socksAddress);
        qDebug() << "I2P daemon is fully ready. Emitting i2pReady signal.";
    }

    // Check for critical errors during startup
    if (output.contains("Address already in use", Qt::CaseInsensitive)) {
        m_lastError = "I2P port already in use. Please stop other I2P instances.";
        qCritical() << m_lastError;
        emit errorOccurred(m_lastError);
        m_status = Status::Error;
        emit statusChanged(m_status);
        emit i2pReady(false, QString());
        stop();
    }
}