#ifndef KPTYDEVICE_H
#define KPTYDEVICE_H

#include <QObject>
#include <QByteArray>

#if defined(Q_OS_WIN)
#define NOMINMAX
#  include <windows.h>
#  include <QtConcurrent/QtConcurrent>
#  include <QFuture>
#else
#include "kpty_p.h"
#endif

class KPtyDevice : public QObject
{
    Q_OBJECT
public:
    explicit KPtyDevice(QObject* parent = nullptr);
    ~KPtyDevice() override;

    bool open();
    bool open(int masterFd); // Windows直接返回false
    void close();

    bool isOpen() const;

#if defined(Q_OS_WIN)
    HPCON conPtyHandle() const;
    HANDLE readPipeHandle() const;
    HANDLE writePipeHandle() const;
#endif

    // 统一对外接口，上层Pty直接调用
    bool write(const char* data, int len);
    //QByteArray readAll();

    bool setWinSize(int lines, int cols);

#if defined(Q_OS_UNIX)
    int masterFd() const;
    int slaveFd() const;
    const char* ttyName() const;
    bool tcGetAttr(struct ::termios* tt);
    bool tcSetAttr(struct ::termios* tt);
    void setCTty();
    void login(const char* user, const char* remote);
    void logout();
#endif

private:
#if defined(Q_OS_WIN)
    bool initConPTY();
    void cleanupConPTY();

    HPCON m_hPC = nullptr;
    HANDLE m_hPipeOut = INVALID_HANDLE_VALUE; // 读输出（终端stdout/stderr）
    HANDLE m_hPipeIn = INVALID_HANDLE_VALUE;  // 写输入（终端stdin）
    bool m_opened = false;
    COORD m_consoleSize{};
    // 异步读取线程相关
    HANDLE m_readEvent = nullptr;
    OVERLAPPED m_readOverlap{};
    bool m_readRunning = false;
    QFuture<void> m_readFuture;
#else
    KPtyPrivate* d;
#endif

signals:
    void dataReceived(QString);
};

#endif