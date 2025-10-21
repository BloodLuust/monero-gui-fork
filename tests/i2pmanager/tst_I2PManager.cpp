#include <algorithm>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "I2PManager.h"

class I2PManagerToggleTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void startAndStop();
    void restartAfterStop();

private:
    void waitForConnected(I2PManager *manager);
    void stopAndWait(I2PManager *manager);
};

void I2PManagerToggleTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    const QString scriptPath = QCoreApplication::applicationDirPath() + "/fake_i2pd.py";
    QVERIFY2(QFileInfo::exists(scriptPath), "fake i2pd helper not found");

    qputenv("MONERO_GUI_I2PD_PATH", scriptPath.toUtf8());
    qRegisterMetaType<I2PManager::Status>("I2PManager::Status");
}

void I2PManagerToggleTest::cleanupTestCase()
{
    auto *manager = I2PManager::instance();
    stopAndWait(manager);
}

void I2PManagerToggleTest::waitForConnected(I2PManager *manager)
{
    QSignalSpy readySpy(manager, &I2PManager::i2pReady);
    QSignalSpy runningSpy(manager, &I2PManager::runningChanged);
    QSignalSpy statusSpy(manager, &I2PManager::statusChanged);

    QVERIFY2(readySpy.wait(5000), "Timed out waiting for I2P readiness");
    const auto readyArgs = readySpy.takeFirst();
    QVERIFY(readyArgs.at(0).toBool());
    QCOMPARE(readyArgs.at(1).toString(), QStringLiteral("127.0.0.1:4447"));

    if (runningSpy.isEmpty()) {
        runningSpy.wait(1000);
    }
    bool sawRunningTrue = std::any_of(runningSpy.cbegin(), runningSpy.cend(), [](const QList<QVariant> &args) {
        return args.at(0).toBool();
    });
    QVERIFY(sawRunningTrue);

    bool sawConnected = false;
    for (const auto &args : statusSpy) {
        if (args.at(0).value<I2PManager::Status>() == I2PManager::Connected) {
            sawConnected = true;
            break;
        }
    }
    if (!sawConnected) {
        QVERIFY2(statusSpy.wait(1000), "No status update captured");
        for (const auto &args : statusSpy) {
            if (args.at(0).value<I2PManager::Status>() == I2PManager::Connected) {
                sawConnected = true;
                break;
            }
        }
    }
    QVERIFY(sawConnected);

    QVERIFY(manager->running());
    QCOMPARE(manager->status(), I2PManager::Connected);
}

void I2PManagerToggleTest::stopAndWait(I2PManager *manager)
{
    if (!manager->running() && manager->status() == I2PManager::Disconnected) {
        manager->stop();
        return;
    }

    QSignalSpy stoppedSpy(manager, &I2PManager::i2pStopped);
    QSignalSpy runningSpy(manager, &I2PManager::runningChanged);
    QSignalSpy statusSpy(manager, &I2PManager::statusChanged);

    manager->stop();

    if (stoppedSpy.isEmpty()) {
        stoppedSpy.wait(500);
    }

    QTRY_VERIFY_WITH_TIMEOUT(stoppedSpy.count() > 0, 5000);

    if (runningSpy.isEmpty()) {
        runningSpy.wait(500);
    }
    QTRY_VERIFY_WITH_TIMEOUT(std::any_of(runningSpy.cbegin(), runningSpy.cend(), [](const QList<QVariant> &args) {
        return !args.at(0).toBool();
    }), 5000);

    if (statusSpy.isEmpty()) {
        statusSpy.wait(500);
    }
    QTRY_VERIFY_WITH_TIMEOUT(std::any_of(statusSpy.cbegin(), statusSpy.cend(), [](const QList<QVariant> &args) {
        return args.at(0).value<I2PManager::Status>() == I2PManager::Disconnected;
    }), 5000);

    QTRY_VERIFY_WITH_TIMEOUT(!manager->running(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(manager->status(), I2PManager::Disconnected, 5000);
}

void I2PManagerToggleTest::startAndStop()
{
    auto *manager = I2PManager::instance();

    manager->start();
    waitForConnected(manager);

    stopAndWait(manager);
}

void I2PManagerToggleTest::restartAfterStop()
{
    auto *manager = I2PManager::instance();

    manager->start();
    waitForConnected(manager);

    stopAndWait(manager);

    manager->start();
    waitForConnected(manager);

    stopAndWait(manager);
}

QTEST_MAIN(I2PManagerToggleTest)
#include "tst_I2PManager.moc"
