#ifndef I2PMANAGER_H
#define I2PMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QProcessEnvironment>
#include <memory>

/**
 * @brief Manages I2P daemon processes and network configuration
 * 
 * The I2PManager class provides comprehensive I2P daemon management functionality
 * including process lifecycle management, configuration handling, tunnel management,
 * and network status monitoring for the Monero GUI application.
 * 
 * This class is implemented as a singleton to ensure a single I2P daemon instance
 * across the entire Monero GUI application.
 */
class I2PManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

public:
    /**
     * @brief Get the singleton instance of I2PManager
     * @return Reference to the singleton instance
     */
    static I2PManager& instance();

    /**
     * @brief Get the singleton instance pointer
     * @return Pointer to the singleton instance
     */
    static I2PManager* getInstance();

public:
    /**
     * @brief I2P daemon status enumeration
     */
    enum class Status {
        Disconnected,    ///< I2P daemon is not running
        Starting,        ///< I2P daemon is starting up
        Connected,       ///< I2P daemon is running and connected to network
        Error,           ///< I2P daemon encountered an error
        Stopping         ///< I2P daemon is shutting down
    };
    Q_ENUM(Status)

    /**
     * @brief I2P tunnel types
     */
    enum class TunnelType {
        HTTP,           ///< HTTP proxy tunnel
        SOCKS,          ///< SOCKS proxy tunnel
        Client          ///< I2P client tunnel
    };
    Q_ENUM(TunnelType)

    /**
     * @brief I2P tunnel configuration structure
     */
    struct TunnelConfig {
        QString name;           ///< Tunnel name
        TunnelType type;        ///< Tunnel type
        int localPort;          ///< Local port for tunnel
        QString targetHost;     ///< Target host for client tunnels
        int targetPort;         ///< Target port for client tunnels
        bool enabled;           ///< Whether tunnel is enabled
        
        TunnelConfig() : type(TunnelType::HTTP), localPort(4444), targetPort(0), enabled(true) {}
    };

    /**
     * @brief I2P tunnel information structure
     */
    struct TunnelInfo {
        QString id;             ///< Unique tunnel identifier
        QString name;           ///< Tunnel name
        TunnelType type;        ///< Tunnel type
        int localPort;          ///< Local port
        QString targetHost;     ///< Target host (for client tunnels)
        int targetPort;         ///< Target port (for client tunnels)
        bool enabled;           ///< Tunnel enabled status
        QString status;         ///< Tunnel status string
        
        TunnelInfo() : type(TunnelType::HTTP), localPort(4444), targetPort(0), enabled(false) {}
    };

    /**
     * @brief I2P network statistics structure
     */
    struct NetworkStats {
        int activeTunnels;      ///< Number of active tunnels
        int inboundBandwidth;   ///< Inbound bandwidth (bytes/sec)
        int outboundBandwidth;  ///< Outbound bandwidth (bytes/sec)
        int peersCount;         ///< Number of connected peers
        QString networkID;      ///< I2P network identifier
        double anonymityLevel; ///< Anonymity level (0.0-1.0)
        bool floodfillEnabled;  ///< Whether floodfill is enabled
    };

    /**
     * @brief Destructor
     */
    ~I2PManager();

    // Q_PROPERTY accessors
    /**
     * @brief Check if I2P daemon is running
     * @return true if running, false otherwise
     */
    bool running() const;

    /**
     * @brief Get current I2P daemon status
     * @return Current status
     */
    Status status() const;

public slots:
    /**
     * @brief Start the I2P daemon
     */
    void start();

    /**
     * @brief Stop the I2P daemon
     */
    void stop();

    /**
     * @brief Generate a new I2P identity
     */
    void generateNewIdentity();

signals:
    /**
     * @brief Emitted when I2P daemon is ready
     * @param success Whether I2P daemon started successfully
     * @param socksAddress SOCKS proxy address for Monero daemon
     */
    void i2pReady(bool success, const QString &socksAddress);

    /**
     * @brief Emitted when I2P daemon is stopped
     */
    void i2pStopped();

    /**
     * @brief Emitted when running status changes
     * @param running New running status
     */
    void runningChanged(bool running);

    /**
     * @brief Emitted when I2P daemon status changes
     * @param status New status
     */
    void statusChanged(Status status);

    // Core I2P daemon management
    /**
     * @brief Restart the I2P daemon process
     * @return true if restart command was successful, false otherwise
     */
    bool restartI2PDaemon();

    // Configuration management
    /**
     * @brief Set I2P configuration
     * @param config Configuration object
     */
    void setConfiguration(const QJsonObject& config);

    /**
     * @brief Get current I2P configuration
     * @return Configuration object
     */
    QJsonObject getConfiguration() const;

    /**
     * @brief Load configuration from file
     * @param filePath Path to configuration file
     * @return true if loaded successfully, false otherwise
     */
    bool loadConfiguration(const QString& filePath);

    /**
     * @brief Save configuration to file
     * @param filePath Path to configuration file
     * @return true if saved successfully, false otherwise
     */
    bool saveConfiguration(const QString& filePath) const;

    // Tunnel management
    /**
     * @brief Create a new I2P tunnel
     * @param config Tunnel configuration
     * @return true if tunnel creation was successful, false otherwise
     */
    bool createTunnel(const TunnelConfig& config);

    /**
     * @brief Destroy an existing I2P tunnel
     * @param tunnelId Tunnel identifier
     * @return true if tunnel destruction was successful, false otherwise
     */
    bool destroyTunnel(const QString& tunnelId);

    /**
     * @brief Get list of all tunnels
     * @return List of tunnel information
     */
    QList<I2PManager::TunnelInfo> getTunnels() const;

    /**
     * @brief Get tunnel information by ID
     * @param tunnelId Tunnel identifier
     * @return Tunnel information, or empty if not found
     */
    I2PManager::TunnelInfo getTunnel(const QString& tunnelId) const;

    /**
     * @brief Enable or disable a tunnel
     * @param tunnelId Tunnel identifier
     * @param enabled Enable status
     * @return true if operation was successful, false otherwise
     */
    bool setTunnelEnabled(const QString& tunnelId, bool enabled);

    // Network status and statistics
    /**
     * @brief Get current network statistics
     * @return Network statistics
     */
    I2PManager::NetworkStats getNetworkStats() const;

    /**
     * @brief Get I2P router information
     * @return Router information string
     */
    QString getRouterInfo() const;

    /**
     * @brief Check I2P network connectivity
     * @return true if connected to I2P network, false otherwise
     */
    bool isNetworkConnected() const;

    // Utility functions
    /**
     * @brief Get I2P daemon executable path
     * @return Path to I2P daemon executable
     */
    QString getI2PDaemonPath() const;

    /**
     * @brief Get I2P configuration directory
     * @return Path to I2P configuration directory
     */
    QString getI2PConfigDir() const;

    /**
     * @brief Get I2P data directory
     * @return Path to I2P data directory
     */
    QString getI2PDataDir() const;

    /**
     * @brief Get last error message
     * @return Last error message
     */
    QString getLastError() const;

public slots:
    /**
     * @brief Refresh I2P status and statistics
     */
    void refreshStatus();

    /**
     * @brief Update tunnel configurations
     */
    void updateTunnels();

private slots:
    /**
     * @brief Handle I2P daemon process state changes
     * @param newState New process state
     */
    void onProcessStateChanged(QProcess::ProcessState newState);

    /**
     * @brief Handle I2P daemon process errors
     * @param error Process error
     */
    void onProcessError(QProcess::ProcessError error);

    /**
     * @brief Handle I2P daemon process output
     */
    void onProcessOutput();

    /**
     * @brief Handle I2P daemon process finished
     * @param exitCode Process exit code
     * @param exitStatus Process exit status
     */
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    /**
     * @brief Handle status refresh timer
     */
    void onStatusTimer();

    /**
     * @brief Handle network request completion
     */
    void onNetworkReplyFinished();

signals:
    /**
     * @brief Emitted when a tunnel is created
     * @param tunnelId Tunnel identifier
     */
    void tunnelCreated(const QString& tunnelId);

    /**
     * @brief Emitted when a tunnel is destroyed
     * @param tunnelId Tunnel identifier
     */
    void tunnelDestroyed(const QString& tunnelId);

    /**
     * @brief Emitted when tunnel status changes
     * @param tunnelId Tunnel identifier
     * @param enabled New enabled status
     */
    void tunnelStatusChanged(const QString& tunnelId, bool enabled);

    /**
     * @brief Emitted when network statistics change
     * @param stats Network statistics
     */
    void networkStatsChanged(const NetworkStats& stats);

    /**
     * @brief Emitted when an error occurs
     * @param error Error message
     */
    void errorOccurred(const QString& error);

    /**
     * @brief Emitted when I2P daemon is ready
     */
    void daemonReady();

    /**
     * @brief Emitted when I2P network is connected
     */
    void networkConnected();

    /**
     * @brief Emitted when I2P network is disconnected
     */
    void networkDisconnected();

private:
    /**
     * @brief Initialize I2P manager
     */
    void initialize();

    /**
     * @brief Setup I2P daemon process
     */
    void setupDaemonProcess();

    /**
     * @brief Setup default I2P configuration
     */
    void setupDefaultConfiguration();

    /**
     * @brief Update I2P daemon status
     */
    void updateStatus();

    /**
     * @brief Parse I2P daemon output
     * @param output Daemon output
     */
    void parseDaemonOutput(const QString& output);

    /**
     * @brief Send command to I2P daemon
     * @param command Command to send
     * @return true if command was sent successfully, false otherwise
     */
    bool sendDaemonCommand(const QString& command);

    /**
     * @brief Parse tunnel information from JSON
     * @param json JSON data
     * @return List of tunnel information
     */
    QList<TunnelInfo> parseTunnelInfo(const QJsonObject& json) const;

    /**
     * @brief Parse network statistics from JSON
     * @param json JSON data
     * @return Network statistics
     */
    NetworkStats parseNetworkStats(const QJsonObject& json) const;

    /**
     * @brief Get platform-specific I2P daemon path
     * @return Path to I2P daemon executable
     */
    QString getPlatformDaemonPath() const;

    /**
     * @brief Create I2P configuration file
     * @return true if configuration file was created, false otherwise
     */
    bool createI2PConfigFile() const;

    /**
     * @brief Validate I2P configuration
     * @param config Configuration to validate
     * @return true if configuration is valid, false otherwise
     */
    bool validateConfiguration(const QJsonObject& config) const;

private:
    /**
     * @brief Private constructor for singleton pattern
     * @param parent Parent QObject
     */
    explicit I2PManager(QObject *parent = nullptr);

    // Private I2P daemon management methods
    /**
     * @brief Start the I2P daemon process
     * @return true if start command was successful, false otherwise
     */
    bool startI2PDaemon();

    /**
     * @brief Stop the I2P daemon process
     * @return true if stop command was successful, false otherwise
     */
    bool stopI2PDaemon();

    // Singleton implementation
    static I2PManager* s_instance;                 ///< Singleton instance pointer

    // Core components
    std::unique_ptr<QProcess> m_daemonProcess;     ///< I2P daemon process
    std::unique_ptr<QTimer> m_statusTimer;        ///< Status refresh timer
    std::unique_ptr<QNetworkAccessManager> m_networkManager; ///< Network manager for API calls
    
    // State management
    Status m_status;                              ///< Current I2P daemon status
    QJsonObject m_configuration;                  ///< I2P configuration
    QMap<QString, TunnelInfo> m_tunnels;         ///< Active tunnels
    NetworkStats m_networkStats;                  ///< Current network statistics
    QString m_lastError;                          ///< Last error message
    
    // Configuration
    QString m_daemonPath;                         ///< Path to I2P daemon executable
    QString m_configDir;                          ///< I2P configuration directory
    QString m_dataDir;                            ///< I2P data directory
    int m_statusRefreshInterval;                   ///< Status refresh interval in milliseconds
    
    // Network settings
    QString m_apiHost;                            ///< I2P API host
    int m_apiPort;                                ///< I2P API port
    QString m_apiKey;                             ///< I2P API key
};

#endif // I2PMANAGER_H
