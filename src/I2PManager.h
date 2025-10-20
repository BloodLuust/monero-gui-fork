#ifndef I2PMANAGER_H
#define I2PMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <memory>

class I2PManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

public:
    enum Status {
        Disconnected,
        Starting,
        Connected,
        Stopping,
        Error
    };
    Q_ENUM(Status)

    static I2PManager* instance();

    bool running() const;
    Status status() const;

public slots:
    void start();
    void stop();
    void generateNewIdentity();

signals:
    void runningChanged();
    void statusChanged();
    void i2pReady(bool success, const QString &socksAddress);
    void i2pStopped();
    void errorOccurred(const QString &errorMsg);

private slots:
    void onProcessStateChanged(QProcess::ProcessState newState);
    void onProcessError(QProcess::ProcessError error);
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    explicit I2PManager(QObject *parent = nullptr);
    ~I2PManager();
    I2PManager(const I2PManager&) = delete;
    I2PManager& operator=(const I2PManager&) = delete;

    static I2PManager* s_instance;

    void initialize();
    void setupDaemonProcess();
    QString getPlatformDaemonPath() const;
    void parseDaemonOutput(const QString& output);

    std::unique_ptr<QProcess> m_daemonProcess;
    QString m_daemonPath;
    QString m_dataDir;
    QString m_lastError;
    Status m_status;

    // Flags for readiness check
    bool m_socksProxyReady;
    bool m_networkReady;
};

#endif // I2PMANAGER_H