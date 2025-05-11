#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private slots:
    void slot_readyRead();
    void slot_bytesWritten(qint64 anNumBytes);
private:
    void copyFile(QString acFileToCopy);
    Ui::MainWindow *ui;

    QString mcReceivingDirectory;
    QTcpServer mcServer;
    QTcpSocket *mpcIncomingConnection = nullptr;
    bool mbReadingNewFile = false;
    qint64 mnBytesReceived=0;
    qint64 mnTotalSizeToReceive=0;
    QString mcFileNameToReceive;
    QFile *mpcFileToReceive;
    QByteArray mcInBlock;

    QTcpSocket *mpcClient;
    bool mbClientConnected = false;
    qint64 mnBytesToWrite = 0;
    qint64 mnTotalSize = 0;
    qint64 mnLoadSize;
    QByteArray mcOutBlock;
    QFile *mpcFileToSend;
    QStringList mcFilesToSend;
};
#endif // MAINWINDOW_H
