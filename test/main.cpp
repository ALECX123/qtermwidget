#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <qtermwidget.h>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QMainWindow w;
    w.setWindowTitle("QTermWidget Test ConPTY");
    w.resize(1000, 600);

    QWidget* central = new QWidget(&w);
    QVBoxLayout* lay = new QVBoxLayout(central);
    lay->setContentsMargins(0, 0, 0, 0);

    // 创建终端控件
    QTermWidget* term = new QTermWidget(0);
    lay->addWidget(term);

    QMenuBar* menuBar = new QMenuBar(&w);
    QMenu* actionsMenu = new QMenu(QStringLiteral("Actions"), menuBar);
    menuBar->addMenu(actionsMenu);
    actionsMenu->addAction(
        QStringLiteral("Find..."),
        term,                  // 2.接收者对象
        &QTermWidget::toggleShowSearchBar, // 3.槽函数
        QKeySequence(Qt::CTRL | Qt::Key_F)); // 4.快捷键
    actionsMenu->addAction(
        QStringLiteral("Copy"),
        term,
        &QTermWidget::copyClipboard,
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    actionsMenu->addAction(
        QStringLiteral("Paste"),
        term,
        &QTermWidget::pasteClipboard,
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));

    actionsMenu->addAction(QStringLiteral("About Qt"), &a, &QApplication::aboutQt);
    w.setMenuBar(menuBar);

    QFont font = QApplication::font();
#ifdef Q_OS_MACOS
    font.setFamily(QStringLiteral("Monaco"));
#elif defined(Q_WS_QWS)
    font.setFamily(QStringLiteral("fixed"));
#else
    font.setFamily(QStringLiteral("Cascadia Mono"));
#endif
    font.setPointSize(12);

    term->setTerminalFont(font);

    term->setColorScheme("WhiteOnBlack.colorscheme");
    term->setKeyBindings("default");
    term->setBlinkingCursor(true);
    term->setKeyboardCursorShape(Konsole::Emulation::KeyboardCursorShape::IBeamCursor);
    term->setScrollBarPosition(QTermWidget::ScrollBarRight);
    
    // 启动默认 shell
#ifdef Q_OS_WIN
    term->setShellProgram("cmd.exe");
    term->startShellProgram();
    term->sendText("echo test\r\n");
    term->scrollToEnd();
#else
    term->startTerminalProgram("/bin/bash", {});
#endif

    w.setCentralWidget(central);
    w.show();
    return a.exec();
}