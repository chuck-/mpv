#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"

#ifdef CONFIG_TRANSLATION
#include <locale.h>
#include <libintl.h>
#endif

#ifdef CONFIG_ICONV
#include <iconv.h>
#include <errno.h>
/**
 * \brief gets the name of the system's terminal character set
 * \return a malloced string indicating the system charset
 *
 * Be warned that this function on many systems is in no way thread-safe
 * since it modifies global data
 */
char* get_term_charset(void);
#endif

#include "mp_msg.h"

/* maximum message length of mp_msg */
#define MSGSIZE_MAX 3072

int mp_msg_levels[MSGT_MAX]; // verbose level of this module. initialized to -2
int mp_msg_level_all = MSGL_STATUS;
int verbose = 0;
int mp_msg_color = 0;
int mp_msg_module = 0;
#ifdef CONFIG_ICONV
char *mp_msg_charset = NULL;
static char *old_charset = NULL;
static iconv_t msgiconv;
#endif

const char* filename_recode(const char* filename)
{
#if !defined(CONFIG_ICONV) || !defined(MSG_CHARSET)
    return filename;
#else
    static iconv_t inv_msgiconv = (iconv_t)(-1);
    static char recoded_filename[MSGSIZE_MAX];
    size_t filename_len, max_path;
    char* precoded;
    if (!mp_msg_charset ||
        !strcasecmp(mp_msg_charset, MSG_CHARSET) ||
        !strcasecmp(mp_msg_charset, "noconv"))
        return filename;
    if (inv_msgiconv == (iconv_t)(-1)) {
        inv_msgiconv = iconv_open(MSG_CHARSET, mp_msg_charset);
        if (inv_msgiconv == (iconv_t)(-1))
            return filename;
    }
    filename_len = strlen(filename);
    max_path = MSGSIZE_MAX - 4;
    precoded = recoded_filename;
    if (iconv(inv_msgiconv, &filename, &filename_len,
              &precoded, &max_path) == (size_t)(-1) && errno == E2BIG) {
        precoded[0] = precoded[1] = precoded[2] = '.';
        precoded += 3;
    }
    *precoded = '\0';
    return recoded_filename;
#endif
}

void mp_msg_init(void){
    int i;
    char *env = getenv("MPLAYER_VERBOSE");
    if (env)
        verbose = atoi(env);
    for(i=0;i<MSGT_MAX;i++) mp_msg_levels[i] = -2;
    mp_msg_levels[MSGT_IDENTIFY] = -1; // no -identify output by default
#ifdef CONFIG_ICONV
    mp_msg_charset = getenv("MPLAYER_CHARSET");
    if (!mp_msg_charset)
      mp_msg_charset = get_term_charset();
#endif
#ifdef CONFIG_TRANSLATION
    textdomain("mplayer");
    char *localedir = getenv("MPLAYER_LOCALEDIR");
    if (localedir == NULL && strlen(MPLAYER_LOCALEDIR))
        localedir = MPLAYER_LOCALEDIR;
    bindtextdomain("mplayer", localedir);
    bind_textdomain_codeset("mplayer", "UTF-8");
#endif
}

int mp_msg_test(int mod, int lev)
{
    return lev <= (mp_msg_levels[mod] == -2 ? mp_msg_level_all + verbose : mp_msg_levels[mod]);
}

static void set_msg_color(FILE* stream, int lev)
{
    static const unsigned char v_colors[10] = {9, 1, 3, 15, 7, 2, 2, 8, 8, 8};
    int c = v_colors[lev];
#ifdef MP_ANNOY_ME
    /* that's only a silly color test */
    {
        int c;
        static int flag = 1;
        if (flag)
            for(c = 0; c < 24; c++)
                printf("\033[%d;3%dm***  COLOR TEST %d  ***\n", c>7, c&7, c);
        flag = 0;
    }
#endif
    if (mp_msg_color)
        fprintf(stream, "\033[%d;3%dm", c >> 3, c & 7);
}

static void print_msg_module(FILE* stream, int mod)
{
    static const char *module_text[MSGT_MAX] = {
        "GLOBAL",
        "CPLAYER",
        "GPLAYER",
        "VIDEOOUT",
        "AUDIOOUT",
        "DEMUXER",
        "DS",
        "DEMUX",
        "HEADER",
        "AVSYNC",
        "AUTOQ",
        "CFGPARSER",
        "DECAUDIO",
        "DECVIDEO",
        "SEEK",
        "WIN32",
        "OPEN",
        "DVD",
        "PARSEES",
        "LIRC",
        "STREAM",
        "CACHE",
        "MENCODER",
        "XACODEC",
        "TV",
        "OSDEP",
        "SPUDEC",
        "PLAYTREE",
        "INPUT",
        "VFILTER",
        "OSD",
        "NETWORK",
        "CPUDETECT",
        "CODECCFG",
        "SWS",
        "VOBSUB",
        "SUBREADER",
        "AFILTER",
        "NETST",
        "MUXER",
        "OSDMENU",
        "IDENTIFY",
        "RADIO",
        "ASS",
        "LOADER",
        "STATUSLINE",
    };
    int c2 = (mod + 1) % 15 + 1;

    if (!mp_msg_module)
        return;
    if (mp_msg_color)
        fprintf(stream, "\033[%d;3%dm", c2 >> 3, c2 & 7);
    fprintf(stream, "%9s", module_text[mod]);
    if (mp_msg_color)
        fprintf(stream, "\033[0;37m");
    fprintf(stream, ": ");
}

void mp_msg_va(int mod, int lev, const char *format, va_list va)
{
    char tmp[MSGSIZE_MAX];
    FILE *stream = lev <= MSGL_WARN ? stderr : stdout;
    static int header = 1;

    if (!mp_msg_test(mod, lev)) return; // do not display
    vsnprintf(tmp, MSGSIZE_MAX, format, va);
    tmp[MSGSIZE_MAX-2] = '\n';
    tmp[MSGSIZE_MAX-1] = 0;

#if defined(CONFIG_ICONV) && defined(MSG_CHARSET)
    if (mp_msg_charset && strcasecmp(mp_msg_charset, "noconv")) {
      char tmp2[MSGSIZE_MAX];
      size_t inlen = strlen(tmp), outlen = MSGSIZE_MAX;
      char *in = tmp, *out = tmp2;
      if (!old_charset || strcmp(old_charset, mp_msg_charset)) {
        if (old_charset) {
          free(old_charset);
          iconv_close(msgiconv);
        }
        msgiconv = iconv_open(mp_msg_charset, MSG_CHARSET);
        old_charset = strdup(mp_msg_charset);
      }
      if (msgiconv == (iconv_t)(-1)) {
        fprintf(stderr,"iconv: conversion from %s to %s unsupported\n"
               ,MSG_CHARSET,mp_msg_charset);
      }else{
      memset(tmp2, 0, MSGSIZE_MAX);
      while (iconv(msgiconv, &in, &inlen, &out, &outlen) == -1) {
        if (!inlen || !outlen)
          break;
        *out++ = *in++;
        outlen--; inlen--;
      }
      strncpy(tmp, tmp2, MSGSIZE_MAX);
      tmp[MSGSIZE_MAX-1] = 0;
      tmp[MSGSIZE_MAX-2] = '\n';
      }
    }
#endif

    if (header)
        print_msg_module(stream, mod);
    set_msg_color(stream, lev);
    header = tmp[strlen(tmp)-1] == '\n' || tmp[strlen(tmp)-1] == '\r';

    fprintf(stream, "%s", tmp);
    fflush(stream);
}

void mp_msg(int mod, int lev, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    mp_msg_va(mod, lev, format, va);
    va_end(va);
}

char *mp_gtext(const char *string)
{
#ifdef CONFIG_TRANSLATION
    /* gettext expects the global locale to be set with
     * setlocale(LC_ALL, ""). However doing that would suck for a
     * couple of reasons (locale stuff is badly designed and sucks in
     * general).
     *
     * First setting the locale, especially LC_CTYPE, changes the
     * behavior of various C functions and we don't want that - we
     * want isalpha() for example to always behave like in the C
     * locale.

     * Second, there is no way to enforce a sane character set. All
     * strings inside MPlayer must always be in utf-8, not in the
     * character set specified by the system locale which could be
     * something different and completely insane. The locale system
     * lacks any way to say "set LC_CTYPE to utf-8, ignoring the
     * default system locale if it specifies something different". We
     * could try to work around that flaw by leaving LC_CTYPE to the C
     * locale and only setting LC_MESSAGES (which is the variable that
     * must be set to tell gettext which language to translate
     * to). However if we leave LC_MESSAGES set then things like
     * strerror() may produce completely garbled output when they try
     * to translate their results but then try to convert some
     * translated non-ASCII text to the character set specified by
     * LC_CTYPE which would still be in the C locale (this doesn't
     * affect gettext itself because it supports specifying the
     * character set directly with bind_textdomain_codeset()).
     *
     * So the only solution (at leat short of trying to work around
     * things possibly producing non-utf-8 output) is to leave all the
     * locale variables unset. Note that this means it's not possible
     * to get translated output from any libraries we call if they
     * only rely on the broken locale system to specify the language
     * to use; this is the case with libc for example.
     *
     * The locale changing below is rather ugly, but hard to avoid.
     * gettext doesn't support specifying the translation target
     * directly, only through locale.
     * The main actual problem this could cause is interference with
     * other threads; that could be avoided with thread-specific
     * locale changes, but such functionality is less standard and I
     * think it's not worth adding pre-emptively unless someone sees
     * an actual problem case.
     */
    setlocale(LC_MESSAGES, "");
    string = gettext(string);
    setlocale(LC_MESSAGES, "C");
#endif
    return string;
}

void mp_tmsg(int mod, int lev, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    mp_msg_va(mod, lev, mp_gtext(format), va);
    va_end(va);
}
