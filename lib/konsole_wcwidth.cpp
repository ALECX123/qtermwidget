/* $XFree86: xc/programs/xterm/wcwidth.character,v 1.3 2001/07/29 22:08:16 tsi Exp $ */
/*
 * This is an implementation of wcwidth() and wcswidth() as defined in
 * "The Single UNIX Specification, Version 2, The Open Group, 1997"
 * <http://www.UNIX-systems.org/online.html>
 *
 * Markus Kuhn -- 2001-01-12 -- public domain
 */

#include <QString>

#ifdef HAVE_UTF8PROC
#include <utf8proc.h>
#elif defined(_WIN32)
// Windows 无 wcwidth，实现极简兼容版本
static int wcwidth(wchar_t c)
{
    // 控制字符、零宽字符宽度0
    if (c < 0x20 || (c >= 0x7f && c < 0xa0))
        return 0;
    // CJK、全角字符宽度2
    if ((c >= 0x1100 && c <= 0x115f)
        || (c >= 0x2e80 && c <= 0xa4cf)
        || (c >= 0xac00 && c <= 0xd7a3)
        || (c >= 0xf900 && c <= 0xfaff)
        || (c >= 0xfe10 && c <= 0xfe19)
        || (c >= 0xfe30 && c <= 0xfe6f)
        || (c >= 0xff00 && c <= 0xff60)
        || (c >= 0xffe0 && c <= 0xffe6))
        return 2;
    // 其余普通字符宽度1
    return 1;
}
#else
#include <cwchar>
#endif

#include "konsole_wcwidth.h"

int konsole_wcwidth(wchar_t ucs)
{
#ifdef HAVE_UTF8PROC
    utf8proc_category_t cat = utf8proc_category( ucs );
    if (cat == UTF8PROC_CATEGORY_CO) {
        // Co: Private use area. libutf8proc makes them zero width, while tmux
        // assumes them to be width 1, and glibc's default width is also 1
        return 1;
    }
    return utf8proc_charwidth( ucs );
#else
    return wcwidth( ucs );
#endif
}

// single byte char: +1, multi byte char: +2
int string_width( const std::wstring & wstr )
{
    int w = 0;
    for ( size_t i = 0; i < wstr.length(); ++i ) {
        w += konsole_wcwidth( wstr[ i ] );
    }
    return w;
}
