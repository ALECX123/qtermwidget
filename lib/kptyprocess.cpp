/*
 * This file is a part of QTerminal - http://gitorious.org/qterminal
 *
 * This file was un-linked from KDE and modified
 * by Maxim Bourmistrov <maxim@unixconn.com>
 *
 */

 /*
     This file is part of the KDE libraries

     Copyright (C) 2007 Oswald Buddenhagen <ossi@kde.org>

     This library is free software; you can redistribute it and/or
     modify it under the terms of the GNU Library General Public
     License as published by the Free Software Foundation; either
     version 2 of the License, or (at your option) any later version.

     This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Library General Public License for more details.

     You should have received a copy of the GNU Library General Public License
     along with this library; see the file COPYING.LIB.  If not, write to
     the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
 */

#include "kptyprocess.h"
#include "kprocess.h"
#include "kptydevice.h"

#include <cstdlib>
#include <csignal>
#include <QDebug>
#include <QVector>

#if defined(Q_OS_WIN)
#define NOMINMAX
#include <windows.h>
#include <processenv.h>
#include <handleapi.h>
#include <processthreadsapi.h>
#include <strsafe.h>
#else
#include <unistd.h>
#endif

KPtyProcess::KPtyProcess(QObject* parent) :
    KPtyProcess(-1, parent)
{
}

KPtyProcess::KPtyProcess(int ptyMasterFd, QObject* parent) :
    KProcess(parent),
    d_ptr(new KPtyProcessPrivate)
{
    Q_D(KPtyProcess);

#if defined(Q_OS_UNIX)
    // Unix Only: KDE KProcess child modifier, dup2 fd redirect
    setChildProcessModifier([d]() {
        d->pty->setCTty();
#if 0
        if (d->addUtmp) {
            d->pty->login(KUser(KUser::UseRealUserID).loginName().toLocal8Bit().constData(), qgetenv("DISPLAY").constData());
        }
#endif
        if (d->ptyChannels & StdinChannel) {
            dup2(d->pty->slaveFd(), 0);
        }
        if (d->ptyChannels & StdoutChannel) {
            dup2(d->pty->slaveFd(), 1);
        }
        if (d->ptyChannels & StderrChannel) {
            dup2(d->pty->slaveFd(), 2);
        }
        });
#endif

    d->pty = std::make_unique<KPtyDevice>(this);

    if (ptyMasterFd == -1) {
        d->pty->open();
    }
    else {
        d->pty->open(ptyMasterFd);
    }

    connect(this, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
#if defined(Q_OS_UNIX)
        if (state == QProcess::NotRunning && d_ptr->addUtmp) {
            d_ptr->pty->logout();
        }
#else
        Q_UNUSED(state);
        Q_UNUSED(d_ptr);
#endif
        });
}
KPtyProcess::~KPtyProcess()
{
    Q_D(KPtyProcess);

    if (state() != QProcess::NotRunning && d->addUtmp)
    {
        //d->pty->logout();
        disconnect(this, &QProcess::stateChanged, this, nullptr);
    }
}

void KPtyProcess::setPtyChannels(PtyChannels channels)
{
    Q_D(KPtyProcess);

    d->ptyChannels = channels;
}

KPtyProcess::PtyChannels KPtyProcess::ptyChannels() const
{
    Q_D(const KPtyProcess);

    return d->ptyChannels;
}

void KPtyProcess::setUseUtmp(bool value)
{
    Q_D(KPtyProcess);

    d->addUtmp = value;
}

bool KPtyProcess::isUseUtmp() const
{
    Q_D(const KPtyProcess);

    return d->addUtmp;
}

KPtyDevice* KPtyProcess::pty() const
{
    Q_D(const KPtyProcess);

    return d->pty.get();
}
#if defined(Q_OS_WIN)
bool KPtyProcess::startConPtyProcess(const QString &program, const QStringList &args)
{
    Q_D(KPtyProcess);
    KPtyDevice *ptyDev = d->pty.get();
    if (!ptyDev || !ptyDev->isOpen())
    {
        qWarning() << "ConPTY device not open";
        return false;
    }

    HPCON hConPty = ptyDev->conPtyHandle();
    if (hConPty == nullptr)
    {
        qWarning() << "ConPTY handle null";
        return false;
    }

    QString cmdLine = program;
    for (const QString &arg : args)
    {
        cmdLine += " \"" + arg + "\"";
    }
    std::wstring wCmd = cmdLine.toStdWString();
    WCHAR *pCmdBuf = (WCHAR *)wCmd.data();

    SIZE_T attrBufSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrBufSize);
    void *attrMem = malloc(attrBufSize);
    LPPROC_THREAD_ATTRIBUTE_LIST pAttrList = (LPPROC_THREAD_ATTRIBUTE_LIST)attrMem;

    if (!InitializeProcThreadAttributeList(pAttrList, 1, 0, &attrBufSize))
    {
        free(attrMem);
        qWarning() << "InitializeProcThreadAttributeList fail";
        return false;
    }

    if (!UpdateProcThreadAttribute(
            pAttrList,
            0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            hConPty,
            sizeof(HPCON),
            nullptr, nullptr))
    {
        free(attrMem);
        qWarning() << "UpdateProcThreadAttribute fail";
        return false;
    }

    STARTUPINFOEXW siEx{};
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    siEx.lpAttributeList = pAttrList;

    PROCESS_INFORMATION procInfo{};
    BOOL created = CreateProcessW(
        nullptr,
        pCmdBuf,
        nullptr,
        nullptr,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT,
        nullptr,
        nullptr,
        &siEx.StartupInfo,
        &procInfo);

    free(attrMem);

    if (!created)
    {
        DWORD err = GetLastError();
        qWarning() << "CreateProcessW error:" << err;
        return false;
    }

    hChildProcess = procInfo.hProcess;
    CloseHandle(procInfo.hThread);
    return true;
}
#endif
//#include "kptyprocess.moc"
