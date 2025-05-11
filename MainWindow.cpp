#include "MainWindow.h"
#include "./ui_MainWindow.h"

#include <thread>
#include <QFileDialog>
#include <QNetworkInterface>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mpcClient(new QTcpSocket(this))
{
    ui->setupUi(this);

    connect (ui->mpcSelectDirBtn, &QAbstractButton::clicked, this, [this](bool)
    {
        mcReceivingDirectory = QFileDialog::getExistingDirectory(
            this, "Open Directory", "/", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        ui->mpcReceivingDirLabel->setText(mcReceivingDirectory);
    });

    connect (ui->mpcListenBtn, &QAbstractButton::clicked, this, [this](bool abClicked)
    {
        if (mpcIncomingConnection != nullptr)
        {
            ui->mpcStatusLabel->setText("Received a second connection!");
        }

        if (abClicked)
        {
            ui->mpcStatusLabel->setText("Listening");
            mcServer.listen(QHostAddress::Any, static_cast<std::uint16_t>(ui->mpcPortSb->value()));
        }
        else
        {
            ui->mpcStatusLabel->setText("No Longer Listening");
            mcServer.close();

            if (mpcIncomingConnection == nullptr) return;
            mpcIncomingConnection->close();
            mpcIncomingConnection = nullptr;
        }
    });

    connect (&mcServer, &QTcpServer::newConnection, this, [this]()
    {
        ui->mpcStatusLabel->setText("New Connection!");
        mpcIncomingConnection = mcServer.nextPendingConnection();
        connect(mpcIncomingConnection, &QTcpSocket::readyRead,
                this, &MainWindow::slot_readyRead);
    });

    connect(ui->mpcConnectBtn, &QAbstractButton::clicked, this, [this](bool)
    {
        mpcClient->connectToHost(ui->mpcServerIpLe->text(),
                                 static_cast<std::uint16_t>(ui->mpcServerPortSb->value()));
        mbClientConnected = mpcClient->waitForConnected(30000);
        QString lcStatusLabel = mbClientConnected ? "Connected!" : "Failed To Connect";
        ui->mpcConnectionStatusLabel->setText(lcStatusLabel);
    });

    connect(ui->mpcFilesToSendBtn, &QAbstractButton::clicked, this, [this](bool)
    {
        qDebug() << "connected? " << mbClientConnected;
        if (!mbClientConnected) return;

        auto lcDialog = QFileDialog();
        mcFilesToSend = QFileDialog::getOpenFileNames(this, "Send Files", "/");
        if (mcFilesToSend.size() == 0) return;

        qDebug() << "Files to send = " << mcFilesToSend.size();
        copyFile(mcFilesToSend.takeFirst());
    });

    connect(mpcClient, &QIODevice::bytesWritten,
            this, &MainWindow::slot_bytesWritten);


    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    QString output;
    // Iterate through each interface
    for (const QNetworkInterface &interface : interfaces)
    {
        // Skip inactive or loopback interfaces
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            // Get all addresses for this interface
            QList<QHostAddress> addresses = interface.allAddresses();

            // Iterate through each address
            for (const QHostAddress &address : addresses)
            {
                // Check for IPv4 or IPv6 addresses (excluding loopback)
                if (address.protocol() == QAbstractSocket::IPv4Protocol ||
                    address.protocol() == QAbstractSocket::IPv6Protocol)
                {
                    output += QString("Interface: %1, IP Address: %2\n")
                    .arg(interface.humanReadableName())
                        .arg(address.toString());
                }
            }
        }
    }
    ui->mpcIpInfo->setPlainText(output);


}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slot_readyRead()
{
    if (mnBytesReceived == 0) // jus tstarted to receive data, this data is the file info.
    {
        ui->mpcReceiveProgressBar->setValue(0);
        QDataStream lcIn(mpcIncomingConnection);
        lcIn >> mnTotalSizeToReceive >> mnBytesReceived >>mcFileNameToReceive;
        mcFileNameToReceive = mcReceivingDirectory + "/" + mcFileNameToReceive;
        qDebug() << "Receiving new file " << mcFileNameToReceive;

        mpcFileToReceive = new QFile(mcFileNameToReceive);
        mpcFileToReceive->open(QFile::WriteOnly);
    }
    else // Officially read the file content
    {
        mcInBlock = mpcIncomingConnection->readAll();

        mnBytesReceived += mcInBlock.size();
        mpcFileToReceive->write(mcInBlock);
        mpcFileToReceive->flush();
    }

    ui->mpcReceiveProgressBar->setMaximum(static_cast<int>(mnTotalSizeToReceive));
    ui->mpcReceiveProgressBar->setValue(static_cast<int>(mnBytesReceived));

    if (mnBytesReceived == mnTotalSizeToReceive)
    {
        mcInBlock.clear();
        mnBytesReceived = 0;
        mnTotalSizeToReceive = 0;
        mpcFileToReceive->close();
    }
}

void MainWindow::slot_bytesWritten(qint64 anNumBytes)
{
    // Remaining data size
    mnBytesToWrite -= anNumBytes;
    mcOutBlock = mpcFileToSend->read(std::min(mnBytesToWrite, mnLoadSize));
    mpcClient->write(mcOutBlock);

    ui->mpcProgressBar->setMaximum(static_cast<int>(mnTotalSize));
    ui->mpcProgressBar->setValue(static_cast<int>(mnTotalSize-mnBytesToWrite));

    if (mcOutBlock.size() == 0)
    {
        mpcFileToSend->close();
        delete mpcFileToSend;
        mnBytesToWrite=0;
        if (mcFilesToSend.size() != 0)
        {
            copyFile(mcFilesToSend.takeFirst());
        }
    }
}

void MainWindow::copyFile(QString acFileToCopy)
{
    QFileInfo lcFile(acFileToCopy);
    mpcFileToSend = new QFile(acFileToCopy);
    mpcFileToSend->open(QFile::ReadOnly);

    mnBytesToWrite = lcFile.size();
    mnTotalSize = lcFile.size();

    // Size of data to send each packet.
    mnLoadSize = 4*1024;

    QDataStream lcOut (&mcOutBlock, QIODevice::WriteOnly);
    lcOut << qint64(0) << qint64(0) << lcFile.fileName();

    mnTotalSize += mcOutBlock.size();
    mnBytesToWrite += mcOutBlock.size();

    lcOut.device()->seek(0);
    lcOut << mnTotalSize << qint64(mcOutBlock.size());
    qDebug() << "Writing " << lcFile.fileName();
    mpcClient->write(mcOutBlock);

    ui->mpcProgressBar->setMaximum(static_cast<int>(mnTotalSize));
    ui->mpcProgressBar->setValue(static_cast<int>(mnTotalSize-mnBytesToWrite));


}
