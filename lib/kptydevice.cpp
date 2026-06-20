#include "kptydevice.h"
#include <QDebug>
#include <cstring>

#if defined(Q_OS_WIN)
#include <malloc.h>
#endif

KPtyDevice::KPtyDevice(QObject* parent)
    : QObject(parent)
{
#if defined(Q_OS_WIN)
    m_consoleSize.X = 80;
    m_consoleSize.Y = 24;
#else
    d = new KPtyPrivate(this);
#endif
}

KPtyDevice::~KPtyDevice()
{
    close();
#if defined(Q_OS_UNIX)
    delete d;
#endif
}

bool KPtyDevice::open()
{
#if defined(Q_OS_WIN)
    if (m_opened)
        return true;
    return initConPTY();
#else
    return d->open();
#endif
}

bool KPtyDevice::open(int masterFd)
{
#if defined(Q_OS_WIN)
    Q_UNUSED(masterFd);
    qWarning() << "Windows ConPTY not support external fd open";
    return false;
#else
    return d->open(masterFd);
#endif
}

void KPtyDevice::close()
{
#if defined(Q_OS_WIN)
    cleanupConPTY();
#else
    d->close();
#endif
}

bool KPtyDevice::isOpen() const
{
#if defined(Q_OS_WIN)
    return m_opened;
#else
    return d->masterFd >= 0;
#endif
}

#if defined(Q_OS_WIN)
HPCON KPtyDevice::conPtyHandle() const
{
    return m_hPC;
}

HANDLE KPtyDevice::readPipeHandle() const
{
    return m_hPipeOut;
}

HANDLE KPtyDevice::writePipeHandle() const
{
    return m_hPipeIn;
}

bool KPtyDevice::setWinSize(int lines, int cols)
{
    if (!m_opened || m_hPC == nullptr)
        return false;
    COORD sz;
    sz.X = (SHORT)cols;
    sz.Y = (SHORT)lines;
    m_consoleSize = sz;
    return SUCCEEDED(ResizePseudoConsole(m_hPC, sz));
}

bool KPtyDevice::write(const char* data, int len)
{
    if (!m_opened || m_hPipeIn == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    BOOL ok = WriteFile(m_hPipeIn, data, len, &written, nullptr);
    return ok && written == (DWORD)len;
}

//QByteArray KPtyDevice::readAll()
//{
//    QByteArray out;
//    if (!m_opened || m_hPipeOut == INVALID_HANDLE_VALUE)
//        return out;
//    char buf[1024];
//    DWORD read = 0;
//    while (ReadFile(m_hPipeOut, buf, sizeof(buf), &read, nullptr) && read > 0)
//    {
//        out.append(buf, read);
//        emit dataReceived(buf, read);
//    }
//    return out;
//}

// 完全复用你提供的 initConPTY 代码
bool KPtyDevice::initConPTY()
{
    HANDLE hOutputRead = INVALID_HANDLE_VALUE;
    HANDLE hOutputWrite = INVALID_HANDLE_VALUE;
    HANDLE hInputRead = INVALID_HANDLE_VALUE;
    HANDLE hInputWrite = INVALID_HANDLE_VALUE;

    // 创建管道
    if (!CreatePipe(&hOutputRead, &hOutputWrite, nullptr, 0))
        goto fail_clean;
    if (!CreatePipe(&hInputRead, &hInputWrite, nullptr, 0))
    {
        CloseHandle(hOutputRead);
        CloseHandle(hOutputWrite);
        goto fail_clean;
    }

    COORD consoleSize = m_consoleSize;
    HRESULT hr = CreatePseudoConsole(consoleSize, hInputRead, hOutputWrite, 0, &m_hPC);
    CloseHandle(hInputRead);
    CloseHandle(hOutputWrite);

    if (FAILED(hr))
    {
        CloseHandle(hOutputRead);
        CloseHandle(hInputWrite);
        goto fail_clean;
    }

    // 开启VT与自动换行
    DWORD dwMode = 0;
    if (GetConsoleMode(hOutputRead, &dwMode))
    {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        dwMode |= ENABLE_WRAP_AT_EOL_OUTPUT;
        dwMode &= ~ENABLE_LINE_INPUT;
        dwMode &= ~ENABLE_ECHO_INPUT;
        SetConsoleMode(hOutputRead, dwMode);
    }

    m_hPipeOut = hOutputRead;
    m_hPipeIn = hInputWrite;
    m_opened = true;

    m_readEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ZeroMemory(&m_readOverlap, sizeof(m_readOverlap));
    m_readOverlap.hEvent = m_readEvent;
    m_readRunning = true;

    //启动读取线程
    m_readFuture = QtConcurrent::run([this]()
        {
            char buf[1024] = { 0 };
            DWORD bytesRead = 0;

            while (m_readRunning)
            {
                ZeroMemory(buf, sizeof(buf));
                BOOL ret = ReadFile(
                    m_hPipeOut,
                    buf,
                    sizeof(buf) - 1,
                    &bytesRead,
                    &m_readOverlap
                );

                if (!ret)
                {
                    DWORD err = GetLastError();
                    if (err == ERROR_IO_PENDING)
                    {
                        WaitForSingleObject(m_readEvent, INFINITE);
                        if (!m_readRunning)
                            break;
                        GetOverlappedResult(m_hPipeOut, &m_readOverlap, &bytesRead, FALSE);
                    }
                    else
                    {
                        break;
                    }
                }

                if (bytesRead > 0)
                {
                    // 直接抛原始字节信号，上层Pty接收
                    QString text = QString::fromUtf8(buf, bytesRead);
                    emit dataReceived(text);
                }
            }
        });
    return true;

fail_clean:
    if (hOutputRead != INVALID_HANDLE_VALUE) CloseHandle(hOutputRead);
    if (hOutputWrite != INVALID_HANDLE_VALUE) CloseHandle(hOutputWrite);
    if (hInputRead != INVALID_HANDLE_VALUE) CloseHandle(hInputRead);
    if (hInputWrite != INVALID_HANDLE_VALUE) CloseHandle(hInputWrite);
    m_hPC = nullptr;
    return false;
}

void KPtyDevice::cleanupConPTY()
{
    m_opened = false;
    // 终止读取循环
    m_readRunning = false;
    if (m_readEvent != nullptr)
    {
        SetEvent(m_readEvent); // 唤醒阻塞等待
    }


    // 释放Event句柄
    if (m_readEvent != nullptr)
    {
        CloseHandle(m_readEvent);
        m_readEvent = nullptr;
    }

    if (m_hPC != nullptr)
    {
        ClosePseudoConsole(m_hPC);
        m_hPC = nullptr;
    }
    if (m_hPipeOut != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hPipeOut);
        m_hPipeOut = INVALID_HANDLE_VALUE;
    }
    if (m_hPipeIn != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hPipeIn);
        m_hPipeIn = INVALID_HANDLE_VALUE;
    }
        // 等待线程退出
    if (m_readFuture.isRunning())
        m_readFuture.waitForFinished();
}

#else
// ====================== Unix 原有逻辑不变 ======================
int KPtyDevice::masterFd() const { return d->masterFd; }
int KPtyDevice::slaveFd() const { return d->slaveFd; }
const char* KPtyDevice::ttyName() const { return d->ttyName.data(); }
bool KPtyDevice::tcGetAttr(struct ::termios* tt) { return d->tcGetAttr(tt); }
bool KPtyDevice::tcSetAttr(struct ::termios* tt) { return d->tcSetAttr(tt); }
void KPtyDevice::setCTty() { d->setCTty(); }
void KPtyDevice::login(const char* u, const char* h) { d->login(u, h); }
void KPtyDevice::logout() { d->logout(); }

bool KPtyDevice::setWinSize(int lines, int cols)
{
    return d->setWinSize(lines, cols);
}

bool KPtyDevice::write(const char* data, int len)
{
    if (d->masterFd < 0) return false;
    return ::write(d->masterFd, data, len) == len;
}

QByteArray KPtyDevice::readAll()
{
    QByteArray res;
    char buf[4096];
    int r;
    while ((r = ::read(d->masterFd, buf, sizeof(buf))) > 0)
        res.append(buf, r);
    return res;
}
#endif