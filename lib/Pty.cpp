/*
 * This file is a part of QTerminal - http://gitorious.org/qterminal
 *
 * This file was un-linked from KDE and modified
 * by Maxim Bourmistrov <maxim@unixconn.com>
 *
 */

 /*
     This file is part of Konsole, an X terminal.
     Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software
     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
     02110-1301  USA.
 */

 // Own
#include "Pty.h"

// 【必须先引入 kptydevice.h，完整定义 KPtyDevice】
#include "kptydevice.h"
#include "kpty.h"

// Qt
#include <QFileInfo>
#include <QStringList>
#include <QtDebug>

// 平台区分头文件
#if defined(Q_OS_UNIX)
// Unix POSIX 头文件仅 Unix 编译
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include <cerrno>
#  include <termios.h>
#  include <csignal>
#elif defined(Q_OS_WIN)
#  define NOMINMAX
#  include <windows.h>
#endif

using namespace Konsole;

void Pty::setWindowSize(int lines, int cols)
{
    _windowColumns = cols;
    _windowLines = lines;

#if defined(Q_OS_UNIX)
    if (pty()->masterFd() >= 0)
        pty()->setWinSize(lines, cols);
#elif defined(Q_OS_WIN)
    // Windows ConPTY 调整窗口尺寸
    pty()->setWinSize(lines, cols);
#endif
}

QSize Pty::windowSize() const
{
    return { _windowColumns, _windowLines };
}

void Pty::setFlowControlEnabled(bool enable)
{
    _xonXoff = enable;
#if defined(Q_OS_UNIX)
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        if (!enable)
            ttmode.c_iflag &= ~(IXOFF | IXON);
        else
            ttmode.c_iflag |= (IXOFF | IXON);
        if (!pty()->tcSetAttr(&ttmode))
            qWarning() << "Unable to set terminal attributes.";
    }
#else
    Q_UNUSED(enable);
    // Windows ConPTY 流控由系统自动管理，无手动termios配置
#endif
}

bool Pty::flowControlEnabled() const
{
#if defined(Q_OS_UNIX)
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        return ttmode.c_iflag & IXOFF &&
            ttmode.c_iflag & IXON;
    }
    qWarning() << "Unable to get flow control status, terminal not connected.";
    return false;
#else
    return _xonXoff;
#endif
}

void Pty::setUtf8Mode(bool enable)
{
    _utf8 = enable;
#if defined(Q_OS_UNIX) && defined(IUTF8)
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        if (!enable)
            ttmode.c_iflag &= ~IUTF8;
        else
            ttmode.c_iflag |= IUTF8;
        if (!pty()->tcSetAttr(&ttmode))
            qWarning() << "Unable to set terminal attributes.";
    }
#else
    Q_UNUSED(enable);
    // Windows 原生UTF8，无需termios开关
#endif
}

void Pty::setErase(char erase)
{
    _eraseChar = erase;
#if defined(Q_OS_UNIX)
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        ttmode.c_cc[VERASE] = erase;
        if (!pty()->tcSetAttr(&ttmode))
            qWarning() << "Unable to set terminal attributes.";
    }
#else
    Q_UNUSED(erase);
#endif
}

char Pty::erase() const
{
#if defined(Q_OS_UNIX)
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttyAttributes;
        pty()->tcGetAttr(&ttyAttributes);
        return ttyAttributes.c_cc[VERASE];
    }
#endif
    return _eraseChar;
}

void Pty::setInitialWorkingDirectory(const QString& dir)
{
    QString pwd = dir;

    if (pwd.length() > 1 && pwd.endsWith(QLatin1Char('/'))) {
        pwd.chop(1);
    }

    setWorkingDirectory(pwd);

    if (pwd != QLatin1String(".")) {
        const QString inheritedPwd = QString::fromLocal8Bit(qgetenv("PWD"));
        if (inheritedPwd.isEmpty()
            || QFileInfo(inheritedPwd).canonicalFilePath() != QFileInfo(pwd).canonicalFilePath()) {
            setEnv(QStringLiteral("PWD"), pwd);
        }
    }
}

void Pty::addEnvironmentVariables(const QStringList& environment)
{
    bool termEnvVarAdded = false;
    for (const QString& pair : environment)
    {
        int pos = pair.indexOf(QLatin1Char('='));
        if (pos >= 0)
        {
            QString variable = pair.left(pos);
            QString value = pair.mid(pos + 1);
            setEnv(variable, value);
            if (variable == QLatin1String("TERM")) {
                termEnvVarAdded = true;
            }
        }
    }
    if (!termEnvVarAdded) {
        setEnv(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    }
}

int Pty::start(const QString& program,
    const QStringList& programArguments,
    const QStringList& environment,
    ulong winid,
    bool addToUtmp)
{
    clearProgram();
    Q_ASSERT(programArguments.count() >= 1);
    setProgram(program, programArguments.mid(1));
    addEnvironmentVariables(environment);
    setEnv(QLatin1String("WINDOWID"), QString::number(winid));
    setEnv(QLatin1String("COLORTERM"), QLatin1String("truecolor"));
    setEnv(QLatin1String("LANGUAGE"), QString(), false);

#if defined(Q_OS_UNIX)
    setUseUtmp(addToUtmp);
    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    if (!_xonXoff)
        ttmode.c_iflag &= ~(IXOFF | IXON);
    else
        ttmode.c_iflag |= (IXOFF | IXON);
#ifdef IUTF8
    if (!_utf8)
        ttmode.c_iflag &= ~IUTF8;
    else
        ttmode.c_iflag |= IUTF8;
#endif
    if (_eraseChar != 0)
        ttmode.c_cc[VERASE] = _eraseChar;
    if (!pty()->tcSetAttr(&ttmode))
        qWarning() << "Unable to set terminal attributes.";
    pty()->setWinSize(_windowLines, _windowColumns);
    KProcess::start();
    if (!waitForStarted())
        return -1;
#elif defined(Q_OS_WIN)
    Q_UNUSED(addToUtmp);
    // Windows 使用ConPTY专用启动接口，不调用KProcess::start
    bool ok = startConPtyProcess(program, programArguments);
    if (!ok)
        return -1;
#endif
    return 0;
}

void Pty::setEmptyPTYProperties()
{
#if defined(Q_OS_UNIX)
    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    if (!_xonXoff)
        ttmode.c_iflag &= ~(IXOFF | IXON);
    else
        ttmode.c_iflag |= (IXOFF | IXON);
#ifdef IUTF8
    if (!_utf8)
        ttmode.c_iflag &= ~IUTF8;
    else
        ttmode.c_iflag |= IUTF8;
#endif
    if (_eraseChar != 0)
        ttmode.c_cc[VERASE] = _eraseChar;
    if (!pty()->tcSetAttr(&ttmode))
        qWarning() << "Unable to set terminal attributes.";
#endif
}

void Pty::setWriteable(bool writeable)
{
#if defined(Q_OS_UNIX)
    struct stat sbuf;
    const char* ttyName = pty()->ttyName();
    if (ttyName && stat(ttyName, &sbuf) == 0)
    {
        if (writeable)
            chmod(ttyName, sbuf.st_mode | S_IWGRP);
        else
            chmod(ttyName, sbuf.st_mode & ~(S_IWGRP | S_IWOTH));
    }
#else
    Q_UNUSED(writeable);
    // Windows ConPTY 无tty节点，无需修改权限
#endif
}

Pty::Pty(int masterFd, QObject* parent)
    : KPtyProcess(masterFd, parent)
{
    init();
}

Pty::Pty(QObject* parent)
    : KPtyProcess(parent)
{
    init();
}

void Pty::init()
{
#if defined(Q_OS_UNIX)
    auto parentChildProcModifier = KPtyProcess::childProcessModifier();
    setChildProcessModifier([parentChildProcModifier = std::move(parentChildProcModifier)]() {
        if (parentChildProcModifier) {
            parentChildProcModifier();
        }
        // 重置信号处理器（仅Unix）
        struct sigaction action;
        sigemptyset(&action.sa_mask);
        action.sa_handler = SIG_DFL;
        action.sa_flags = 0;
        for (int signal = 1; signal < NSIG; signal++) {
            sigaction(signal, &action, nullptr);
        }
    });
#endif

    _windowColumns = 0;
    _windowLines = 0;
    _eraseChar = 0;
    _xonXoff = true;
    _utf8 = true;

#if defined(Q_OS_WIN)
    // Windows 正确绑定带参信号，新式Qt5 connect语法
    connect(pty(), &KPtyDevice::dataReceived, this, &Pty::dataReceived);
#else
    // Unix 原有逻辑不变
    connect(pty(), SIGNAL(readyRead()) , this , SLOT(dataReceived()));
#endif
    setPtyChannels(KPtyProcess::AllChannels);
}

Pty::~Pty()
{
}

void Pty::sendData(const char* data, int length)
{
    if (!length)
        return;
    if (!pty()->write(data, length))
    {
        qWarning() << "Pty::doSendJobs - Could not send input data to terminal process.";
        return;
    }
}

void Pty::dataReceived(QString text)
{
    QByteArray data = text.toUtf8();
    if (data.isEmpty())
    {
        return;
    }
    emit receivedData(data.constData(),data.size());
}
void Pty::lockPty(bool lock)
{
    Q_UNUSED(lock)
}

int Pty::foregroundProcessGroup() const
{
#if defined(Q_OS_UNIX)
    const int master_fd = pty()->masterFd();
    if (master_fd >= 0)
    {
        int pid = tcgetpgrp(master_fd);
        if (pid != -1)
            return pid;
    }
#endif
    return 0;
}

void Pty::closePty()
{
    pty()->close();
}
