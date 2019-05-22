/*    miniperlmain.c
 *
 *    Copyright (C) 1994, 1995, 1996, 1997, 1999, 2000, 2001, 2002, 2003,
 *    2004, 2005, 2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "The Road goes ever on and on, down from the door where it began."
 */

/* This file contains the main() function for the perl interpreter.
 * Note that miniperlmain.c contains main() for the 'miniperl' binary,
 * while perlmain.c contains main() for the 'perl' binary.
 *
 * Miniperl is like perl except that it does not support dynamic loading,
 * and in fact is used to build the dynamic modules needed for the 'real'
 * perl executable.
 */

#ifdef OEMVS
#ifdef MYMALLOC
/* sbrk is limited to first heap segment so make it big */
#pragma runopts(HEAP(8M,500K,ANYWHERE,KEEP,8K,4K) STACK(,,ANY,) ALL31(ON))
#else
#pragma runopts(HEAP(2M,500K,ANYWHERE,KEEP,8K,4K) STACK(,,ANY,) ALL31(ON))
#endif
#endif


#include "EXTERN.h"
#define PERL_IN_MINIPERLMAIN_C
#include "perl.h"

static void xs_init (pTHX);
static PerlInterpreter *my_perl;

#if defined (__MINT__) || defined (atarist)
/* The Atari operating system doesn't have a dynamic stack.  The
   stack size is determined from this value.  */
long _stksize = 64 * 1024;
#endif

#if defined(PERL_GLOBAL_STRUCT_PRIVATE)
/* The static struct perl_vars* may seem counterproductive since the
 * whole idea PERL_GLOBAL_STRUCT_PRIVATE was to avoid statics, but note
 * that this static is not in the shared perl library, the globals PL_Vars
 * and PL_VarsPtr will stay away. */
static struct perl_vars* my_plvarsp;
struct perl_vars* Perl_GetVarsPrivate(void) { return my_plvarsp; }
#endif

#ifdef NO_ENV_ARRAY_IN_MAIN
extern char **environ;
int
main(int argc, char **argv)
#else
int
main(int argc, char **argv, char **env)
#endif
{
    dVAR;
    int exitstatus;
#ifdef PERL_GLOBAL_STRUCT
    struct perl_vars *plvarsp = init_global_struct();
#  ifdef PERL_GLOBAL_STRUCT_PRIVATE
    my_vars = my_plvarsp = plvarsp;
#  endif
#endif /* PERL_GLOBAL_STRUCT */
    (void)env;
#ifndef PERL_USE_SAFE_PUTENV
    PL_use_safe_putenv = 0;
#endif /* PERL_USE_SAFE_PUTENV */

    /* if user wants control of gprof profiling off by default */
    /* noop unless Configure is given -Accflags=-DPERL_GPROF_CONTROL */
    PERL_GPROF_MONCONTROL(0);

#ifdef NO_ENV_ARRAY_IN_MAIN
    PERL_SYS_INIT3(&argc,&argv,&environ);
#else
    PERL_SYS_INIT3(&argc,&argv,&env);
#endif

#if defined(USE_ITHREADS)
    /* XXX Ideally, this should really be happening in perl_alloc() or
     * perl_construct() to keep libperl.a transparently fork()-safe.
     * It is currently done here only because Apache/mod_perl have
     * problems due to lack of a call to cancel pthread_atfork()
     * handlers when shared objects that contain the handlers may
     * be dlclose()d.  This forces applications that embed perl to
     * call PTHREAD_ATFORK() explicitly, but if and only if it hasn't
     * been called at least once before in the current process.
     * --GSAR 2001-07-20 */
    PTHREAD_ATFORK(Perl_atfork_lock,
                   Perl_atfork_unlock,
                   Perl_atfork_unlock);
#endif

    if (!PL_do_undump) {
	my_perl = perl_alloc();
	if (!my_perl)
	    exit(1);
	perl_construct(my_perl);
	PL_perl_destruct_level = 0;
    }
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    exitstatus = perl_parse(my_perl, xs_init, argc, argv, (char **)NULL);
    if (!exitstatus)
        perl_run(my_perl);

    exitstatus = perl_destruct(my_perl);

    perl_free(my_perl);

#if defined(USE_ENVIRON_ARRAY) && defined(PERL_TRACK_MEMPOOL) && !defined(NO_ENV_ARRAY_IN_MAIN)
    /*
     * The old environment may have been freed by perl_free()
     * when PERL_TRACK_MEMPOOL is defined, but without having
     * been restored by perl_destruct() before (this is only
     * done if destruct_level > 0).
     *
     * It is important to have a valid environment for atexit()
     * routines that are eventually called.
     */
    environ = env;
#endif

#ifdef PERL_GLOBAL_STRUCT
    free_global_struct(plvarsp);
#endif /* PERL_GLOBAL_STRUCT */

    PERL_SYS_TERM();

    exit(exitstatus);
    return exitstatus;
}

/* Register any extra external extensions */

/* Do not delete this line--writemain depends on it */
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);
EXTERN_C void boot_B (pTHX_ CV* cv);
EXTERN_C void boot_Compress__Raw__Zlib (pTHX_ CV* cv);
EXTERN_C void boot_Cwd (pTHX_ CV* cv);
EXTERN_C void boot_Data__Dumper (pTHX_ CV* cv);
EXTERN_C void boot_Devel__DProf (pTHX_ CV* cv);
EXTERN_C void boot_Devel__PPPort (pTHX_ CV* cv);
EXTERN_C void boot_Devel__Peek (pTHX_ CV* cv);
EXTERN_C void boot_Digest__MD5 (pTHX_ CV* cv);
EXTERN_C void boot_Digest__SHA (pTHX_ CV* cv);
EXTERN_C void boot_Encode (pTHX_ CV* cv);
EXTERN_C void boot_Fcntl (pTHX_ CV* cv);
EXTERN_C void boot_File__Glob (pTHX_ CV* cv);
EXTERN_C void boot_Filter__Util__Call (pTHX_ CV* cv);
EXTERN_C void boot_Hash__Util (pTHX_ CV* cv);
EXTERN_C void boot_I18N__Langinfo (pTHX_ CV* cv);
EXTERN_C void boot_IO (pTHX_ CV* cv);
EXTERN_C void boot_IPC__SysV (pTHX_ CV* cv);
EXTERN_C void boot_List__Util (pTHX_ CV* cv);
EXTERN_C void boot_MIME__Base64 (pTHX_ CV* cv);
EXTERN_C void boot_Math__BigInt__FastCalc (pTHX_ CV* cv);
EXTERN_C void boot_Opcode (pTHX_ CV* cv);
EXTERN_C void boot_POSIX (pTHX_ CV* cv);
EXTERN_C void boot_PerlIO__encoding (pTHX_ CV* cv);
EXTERN_C void boot_PerlIO__scalar (pTHX_ CV* cv);
EXTERN_C void boot_PerlIO__via (pTHX_ CV* cv);
EXTERN_C void boot_SDBM_File (pTHX_ CV* cv);
EXTERN_C void boot_Socket (pTHX_ CV* cv);
EXTERN_C void boot_Storable (pTHX_ CV* cv);
EXTERN_C void boot_Sys__Hostname (pTHX_ CV* cv);
EXTERN_C void boot_Sys__Syslog (pTHX_ CV* cv);
EXTERN_C void boot_Text__Soundex (pTHX_ CV* cv);
EXTERN_C void boot_Time__HiRes (pTHX_ CV* cv);
EXTERN_C void boot_Time__Piece (pTHX_ CV* cv);
EXTERN_C void boot_Unicode__Normalize (pTHX_ CV* cv);
EXTERN_C void boot_attrs (pTHX_ CV* cv);
EXTERN_C void boot_re (pTHX_ CV* cv);
EXTERN_C void boot_threads (pTHX_ CV* cv);
EXTERN_C void boot_threads__shared (pTHX_ CV* cv);
EXTERN_C void boot_Hash__Util__FieldHash (pTHX_ CV* cv);
EXTERN_C void boot_Encode__Byte (pTHX_ CV* cv);
EXTERN_C void boot_Encode__CN (pTHX_ CV* cv);
EXTERN_C void boot_Encode__EBCDIC (pTHX_ CV* cv);
EXTERN_C void boot_Encode__JP (pTHX_ CV* cv);
EXTERN_C void boot_Encode__KR (pTHX_ CV* cv);
EXTERN_C void boot_Encode__Symbol (pTHX_ CV* cv);
EXTERN_C void boot_Encode__TW (pTHX_ CV* cv);
EXTERN_C void boot_Encode__Unicode (pTHX_ CV* cv);

static void
xs_init(pTHX)
{
    static const char file[] = __FILE__;
    dXSUB_SYS;
        newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
        newXS("B::bootstrap", boot_B, file);
        newXS("Compress::Raw::Zlib::bootstrap", boot_Compress__Raw__Zlib, file);
        newXS("Cwd::bootstrap", boot_Cwd, file);
        newXS("Data::Dumper::bootstrap", boot_Data__Dumper, file);
        newXS("Devel::DProf::bootstrap", boot_Devel__DProf, file);
        newXS("Devel::PPPort::bootstrap", boot_Devel__PPPort, file);
        newXS("Devel::Peek::bootstrap", boot_Devel__Peek, file);
        newXS("Digest::MD5::bootstrap", boot_Digest__MD5, file);
        newXS("Digest::SHA::bootstrap", boot_Digest__SHA, file);
        newXS("Encode::bootstrap", boot_Encode, file);
        newXS("Fcntl::bootstrap", boot_Fcntl, file);
        newXS("File::Glob::bootstrap", boot_File__Glob, file);
        newXS("Filter::Util::Call::bootstrap", boot_Filter__Util__Call, file);
        newXS("Hash::Util::bootstrap", boot_Hash__Util, file);
        newXS("I18N::Langinfo::bootstrap", boot_I18N__Langinfo, file);
        newXS("IO::bootstrap", boot_IO, file);
        newXS("IPC::SysV::bootstrap", boot_IPC__SysV, file);
        newXS("List::Util::bootstrap", boot_List__Util, file);
        newXS("MIME::Base64::bootstrap", boot_MIME__Base64, file);
        newXS("Math::BigInt::FastCalc::bootstrap", boot_Math__BigInt__FastCalc, file);
        newXS("Opcode::bootstrap", boot_Opcode, file);
        newXS("POSIX::bootstrap", boot_POSIX, file);
        newXS("PerlIO::encoding::bootstrap", boot_PerlIO__encoding, file);
        newXS("PerlIO::scalar::bootstrap", boot_PerlIO__scalar, file);
        newXS("PerlIO::via::bootstrap", boot_PerlIO__via, file);
        newXS("SDBM_File::bootstrap", boot_SDBM_File, file);
        newXS("Socket::bootstrap", boot_Socket, file);
        newXS("Storable::bootstrap", boot_Storable, file);
        newXS("Sys::Hostname::bootstrap", boot_Sys__Hostname, file);
        newXS("Sys::Syslog::bootstrap", boot_Sys__Syslog, file);
        newXS("Text::Soundex::bootstrap", boot_Text__Soundex, file);
        newXS("Time::HiRes::bootstrap", boot_Time__HiRes, file);
        newXS("Time::Piece::bootstrap", boot_Time__Piece, file);
        newXS("Unicode::Normalize::bootstrap", boot_Unicode__Normalize, file);
        newXS("attrs::bootstrap", boot_attrs, file);
        newXS("re::bootstrap", boot_re, file);
        newXS("threads::bootstrap", boot_threads, file);
        newXS("threads::shared::bootstrap", boot_threads__shared, file);
        newXS("Hash::Util::FieldHash::bootstrap", boot_Hash__Util__FieldHash, file);
        newXS("Encode::Byte::bootstrap", boot_Encode__Byte, file);
        newXS("Encode::CN::bootstrap", boot_Encode__CN, file);
        newXS("Encode::EBCDIC::bootstrap", boot_Encode__EBCDIC, file);
        newXS("Encode::JP::bootstrap", boot_Encode__JP, file);
        newXS("Encode::KR::bootstrap", boot_Encode__KR, file);
        newXS("Encode::Symbol::bootstrap", boot_Encode__Symbol, file);
        newXS("Encode::TW::bootstrap", boot_Encode__TW, file);
        newXS("Encode::Unicode::bootstrap", boot_Encode__Unicode, file);
}