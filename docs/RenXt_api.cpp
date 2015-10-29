//RenXt simplified API
//RenXt simplified API
//provides wrappers to low-level functions to make integration with target sequencer (Vixen 2.x, Nutcracker/XLights, Vixen 3?, HLS?, etc) easier, since there are many choices now
//history:
// 1.0  8/28/13  DJ  created
//1.3 9/15/13 DJ first try at parallel WS281X, GECE
// 1.4  10/28/14  DJ  ins extra Sync after ReadMem, re-ins pad if it went near end of prev ReadMem (to recover from trailing Esc), force explicit pad off if > 1 stop bit, send empty node list if dedup list passes
// 1.4e  11/16/14  DJ  fix bit order problem in parallel palette entries (only occurs for non-primary colors)

//NOTE:
//to build boost libs from src:
//0. junction c:\mingw "C:\Program Files (x86)\CodeBlocks\mingw"
//1. download nightly, ie 1_57_0_b1
//NO: 1. download boost and boost.build, unpack boost.build INTO boost folder
//NO:3. then bootstrap will run ok with gcc/mingw/code::blocks
//NO:4. cd "C:\Users\user\Documents\boost_1_56_0\tools\build\src\build"; b2 install ---prefix=C:\Users\user\Documents\boost_1_56_0\built
//2. edit tools\build\src\engine\build.bat: change order and/or c:\mingw path of compiler detect so gcc comes < msvc
//3. .\b2
//3. edit .\project-config.jam, change msvc to gcc

//just use compiled version, but first time do something like:
//http://www.boost.org/doc/libs/1_57_0/more/getting_started/windows.html
//
//download boost1.57 from boost.org
//unpack
//set up c:\mingw junction
//
//    Go to the directory tools\build\.
//    Run bootstrap.bat
//    Run b2 install --prefix=PREFIX where PREFIX is the directory where you want Boost.Build to be installed
//    Add PREFIX\bin to your PATH environment variable.
//b2 --build-dir=build-directory toolset=toolset-name --build-type=complete gcc stage

#include <wx/string.h>

#include <stddef.h> //size_t
#include <memory.h>
#include <time.h>
#include <assert.h>
//TODO #include <Magick++.h>

//#define LEPT

#define SPLITABLE  100 //allow splitable bitmaps, nodelists
//#define RESEND_MAXLEN  100 //max length to resend

#define WANT_API  1 //simple API
#define WANT_DEBUG
//#define WANT_STRICMP
//#define DLL_EXPORT __declspec(dllexport)
//#define DLL_EXPORT __declspec(dllimport)
#include "RenXt.h"

#include "platform.h"

#ifdef __STRICT_ANSI__
// #pragma message WARN("turning off STRICT_ANSI to pull in strcasecmp")
 #undef __STRICT_ANSI__
 #ifndef __WXMSW__
  #define _fileno(__F) ((__F)->_fileno) //from stdio.h
 #else
  #define _fileno(ignored)  0 //TODO
 #endif // __WXMSW__
 #undef __WXMSW__
#endif // __STRICT_ANSI__

#include "platform.h"  //stricmp

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
//#include <strings.h> //strcasecmp
#include <ctype.h>
//#include <math.h>
#include <limits>
#include <sstream>
#include <unistd.h> //sleep
//#ifdef LEPT
//#include <allheaders.h> //leptonica
//#else
#define PIX  byte
#define pixDestroy(hpix)
//#endif

#ifdef _MSC_VER
 #include <hash_map>
 #define unordered_map  hash_map //c++ 2011 compat
#else
 #include <unordered_map>
#endif
//#include <unordered_set>
//#include <queue> //priority_queue
#include <algorithm> //sort
#include <list>


#ifndef FALSE
 #define FALSE  0
#endif // FALSE

#define node_value  uint32_t //allows for RGBW or subset, or 4-way monochrome value
#define BLACK  RGB2Value(0, 0, 0)
#define DONT_CARE  FALSE  //;arbitrary, but it's safer to turn off a don't-care feature than to leave it on

//#define ABS(val)  (((val) < 0)? -(val): (val))
#define rdiv(num, den)  (((num) + (den)/2) / MAX(den, 1))
#define safe_div(num, den)  ((num) / MAX(den, 1))
#define divup(num, den)  safe_div((num) + (den) - 1, den) //ceil((num)/MAX(den, 1))
#define numents(ary)  (sizeof(ary)/sizeof(ary[0]))
#define swap_notemps(a, b)  { a ^= b; b ^= a; a ^= b; }
#define non_const  //dummy keyword to explicitly show intention
#define safe_byte(ary, inx, len)  (((inx) < (len))? ary[inx]: 0)

int padlen(int curlen, int unitlen)
{
    int pad = curlen % unitlen;
//    return pad? curlen + unitlen - pad: curlen; //make integral
    return pad? unitlen - pad: 0; //make integral
}

//#define NOPE(stmt)
//type-safe min that's also safe with side effects:
// from http://stackoverflow.com/questions/3437404/min-and-max-in-c
//#define min_safe(a, b)
//({
//    __typeof__ (a) _a = a;
//    __typeof__ (b) _b = b;
//    (_a > _b)? _a: _b;
//})
template <typename T>
T min_safe(T a, T b)
{
    return (a < b)? a: b;
}
template <typename T>
T max_safe(T a, T b)
{
    return (a > b)? a: b;
}

//template<typename TKey, typename TList>
//bool contains(TKey key, TList& list)
//{
//    return (list.find(key) != list.end());
//}
//template<typename TKey, typename TList>
//class MyHashMap: public std::hash_map<TKey, TList>
//{
//public:
//    bool contains(const TKey& key) const { return this->find(key) != this->end(); }
//};
template <typename TKey, typename TVal>
class MyHashMap: public std::unordered_map<TKey, TVal>
{
public:
    bool Contains(const TKey& key) const { return std::unordered_map<TKey, TVal>::find(key) != std::unordered_map<TKey, TVal>::end(); }
};
//conflict with wxString.h  #define Contains(hashmap, key)  (hashmap.find(key) != hashmap.end())
//bool Contains(std::unordered_map<TKey, TVal>& hashmap, TKey& key)
//{
//    return hashmap.find(key) != hashmap.end();
//}


//wrapper to allow int to be used in place of std::vector
template<typename T>
class non_vector
{
private:
    int m_count;
public:
    non_vector(void): m_count(0) {}
    ~non_vector(void) {}
public:
    int size(void) const { return m_count; }
    void push_back(T val) { ++m_count; }
};
template<typename T>
#ifdef WANT_DEBUG
class maybe_vector: public std::vector<T> {};
#else
class maybe_vector: public non_vector<T> {};
#endif // WANT_DEBUG



//IFCPP(extern "C")
int RenXt_palovfl = 0;//(int level);


#define varmsg(msgtype, where, stream, fmt)  \
{ \
    char newfmt[256]; \
    if (!strlen(#msgtype)) sprintf(newfmt, "%s\n", fmt); \
    else sprintf(newfmt, "[" #msgtype "#%d @%d] %s\n", seq++, where, fmt); \
    va_list argp; \
    va_start(argp, fmt); \
    vfprintf(stream, newfmt, argp); \
    if (stream != stdout) /*echo to stdout also*/ \
        if (!isatty(_fileno(stdout)) || !isatty(_fileno(stderr))) vfprintf(stdout, newfmt, argp); /*copy to stdout (easier debug)*/ \
    va_end(argp); \
}

std::string RenXt_LastErrorText;

//IFCPP(extern "C")
int RenXt_debug_level /*WantDebug*/ = 0; //default none (just errors)
std::string RenXt_debug_file = "";

//#ifdef _DEBUG
//#pragma message WARN("compiled for debug")
//#define debug(level, ...)  RenXt_debug(level, __LINE__, __VA_ARGS__)
//IFCPP(extern "C")
//typedef void (debug_cb*)(const char* buf);
//debug_cb RenXt_debug_cb = 0;
/*PRIVATE*/ void RenXt_debug(int level, int where, const char* fmt, ...)
{
    static wxString prev_file = "";
    static FILE* prev_fd = stdout;
    static int seq = 0;
//    char newfmt[256];
    if (ABS(level) > ABS(RenXt_debug_level) /*WantDebug*/) return;
//    if (stricmp(RenXt_debug_file.c_str(), prev_file.c_str())) //missing?
    if (prev_file.CmpNoCase(RenXt_debug_file))
    {
        FILE* new_fd = fopen(RenXt_debug_file.c_str(), "w");
        if (new_fd != NULL)
        {
            if (prev_fd) fclose(prev_fd);
            prev_fd = new_fd;
            prev_file = RenXt_debug_file;
        }
    }
    if (level < 0) fseek(prev_fd, -1, SEEK_CUR); //kludge: overwrite newline to join with prev line
    if (level < 0) varmsg(, where, /*stdout*/ prev_fd, fmt) //continue prev debug line (skip hdr info)
    else if (level > 0) varmsg(DEBUG, where, /*stdout*/ prev_fd, fmt) //kludge; reuse logic for errors
    else varmsg(ERROR, where, prev_fd, fmt);
//    sprintf(newfmt, "[DEBUG @%d] %s\n", where, fmt);
//    va_list argp;
//    va_start(argp, fmt);
//    vfprintf(stdout, newfmt, argp);
//    va_end(argp);
//    if (!isatty(_fileno(stdout))) vfprintf(stdout, newfmt, argp); //put a copy in redirected file (easier debug)
//    vfprintf(stderr, newfmt, argp);
//    fflush(stderr);
#if 1
    if (RenXt_debug_level < 0) fflush(prev_fd); //make sure it gets logged
#endif
//    if (level) return;
    if (!level) //|| RenXt_debug_cb)
    {
//    e sprintf(newfmt, "[" #msgtype "#%d @%d] %s\n", seq++, where, fmt);
        va_list argp;
        va_start(argp, fmt);
        char buf[1024];
        vsnprintf(buf, 1024, fmt, argp);
        if (!level) RenXt_LastErrorText = buf;
//        if (RenXt_debug_cb) (*RenXt_debug_cb)(buf);
        va_end(argp);
    }
}
//#else // WANT_DEBUG
//#define debug(...)
//#endif // WANT_DEBUG


//IFCPP(extern "C")
int RenXt_errors = 0;
#if 0
#if 1
// #define error(...)  RenXt_debug(0, __LINE__, __VA_ARGS__)
#else
// #define error(...)  error_(__LINE__, __VA_ARGS__)
/*PRIVATE*/ void error_(int where, const char* fmt, ...)
{
//    char newfmt[256];
//    sprintf(newfmt, "[ERROR @%d] %s\n", where, fmt);
//    va_list argp;
//    va_start(argp, fmt);
    varmsg(ERROR, where, stderr, fmt);
//	va_arg(argp, int);
//    msg(stderr, "ERROR", fmt, argp);
//#ifdef WANT_DEBUG
//    static bool first = true;
//    if (first) { fprintf(stderr, "errors sent to stdout\n"); first = false; } //DEBUG only
//    if (!isatty(_fileno(stdout))) vfprintf(stdout, newfmt, argp); //put a copy in redirected file (easier debug)
//    vfprintf(stderr, newfmt, argp);
    fflush(stderr);
//    va_end(argp);
    ++RenXt_errors; //NumErr;
}
#endif // 0
#endif


//display contents of buffer in hex (mainly for debug):
//IFCPP(extern "C")
/*static*/ void showbuf(const char* desc, const void* buf, int buflen, bool full)
{
    const byte* ptr = (const byte*)(const void*)buf; //kludge: gcc not allowing inline cast to l-value, so use alternate var
    char prevline[4+ 16 * 5 + 1] = "";

    debug(10, "%s (%d bytes):", desc, buflen);
    for (int i = 0; i < buflen; i += 16)
    {
        char linebuf[sizeof(prevline)];
        int curlen = 0;
        for (int j = 0; (j < 16) && (i + j < buflen); ++j)
        {
            sprintf(linebuf + curlen, (ptr[i + j] < 10)? "%d ": "x%.2x ", ptr[i + j]);
            curlen += strlen(linebuf + curlen);
            if (full) continue; //no repeat check
            /*bool*/ int repeated = MIN(16, buflen - i) - j - 1; //(i + j + 1 < buflen)? 0: buflen - i - j;
//            for (int k = j + 1; repeated && (k < 16) && (i + k < buflen); ++k)
//            while (repeated) if (ptr[i + j + repeated--] != ptr[i + j]) repeated = 0;
            for (int k = repeated; k > 0; --k)
                if (ptr[i + j + k] != ptr[i + j]) { repeated = 0; break; }
//                if (ptr[i + k] != ptr[i + j]) repeated = 0 /*false*/; //break; }
            if (repeated) { sprintf(linebuf + curlen, "...+%dx", repeated); break; }
        }
        if (!full && /*i && (i + 16 < buflen) &&*/(i + 16 < buflen) && !strcmp(linebuf, prevline)) continue; //don't show dup lines, except on last
        if (!full && i && (i + 16 < buflen) && (ptr[i] == ptr[i - 1])) //check for last char repeated for entire line
        {
            char* bp = strchr(linebuf, ' ');
            if (!bp || !strncmp(bp + 1, "...", 3)) continue; //line is all same value
        }
//            if (!strcmp(prevline + strlen(prevline) - strlen(linebuf), linebuf)) continue;
//        char* bp = strstr(linebuf, "...");
//        int linelen = bp? bp - linebuf + 3: strlen((linebuf);
//        bp = strstr(prevline, "...");
//        int prevlen = bp? bp - prevline + 3: strlen(prevline);
//        if ((linelen <= prevlen) && (!strncmp(prevline + strlen(prevline) - strlen(linebuf), linebuf)) continue;
        debug(10, (i < 10)? "'[%d]: %s": "'[x%x]: %s", i, linebuf);
        strcpy(prevline, linebuf);
    }
}


#if 0 //use only in emergency; very slow
static bool want_checkpoint = false;
static void checkpoint(int line, int val1 = -2, int val2 = -2, int val3 = -2, int val4 = -2, int val5 = -2, int val6 = -2, int val7 = -2)
{
    if (!want_checkpoint) return;
    FILE* fd = fopen("checkpoint.log", (line < 0)? "w": "a");
    if (fd == NULL) return;
    if ((val1 != -2) || (val2 != -2)) fprintf(fd, "checkpoint@%d = %d, %d, %d, %d, %d, %d, %d\n", line, val1, val2, val3, val4, val5, val6, val7);
    else fprintf(fd, "chkpt@%d\n", line);
    fclose(fd);
}
#else
 #define checkpoint(...)
#endif // 0


const char* commas(const char* fmtspec, long val)
{
    static char buf[12][40]; //allow 12 simultaneous values
    static int ff = 0;
    ++ff;
#if 0
    if (val >= 1000*1000) sprintf(buf[ff % 4], "~ %ldM", divup(val, 1000*1000));
    else if (val >= 1000) sprintf(buf[ff % 4], "~ %ldK", divup(val, 1000));
    else sprintf(buf[ff % 4], fmtspec, val);
#endif // 0
    char* bp = buf[ff % numents(buf)];
    int len = sprintf(bp, fmtspec, val);
    for (int i = len; i > 3; i -= 3)
    {
        memmove(bp + i - 2, bp + i - 3, len + 3 - i + 1);
        bp[i - 3] = ','; ++len;
    }
//    sprintf(bp + strlen(bp), " (%ld)", val);
    return bp; //buf[ff % numents(buf)];
}

bool RenXt_wrstats(const std::string& stats_file, const RenXt_Stats* stats)
{
    float fps = 20; //TODO: use caller's value
    FILE* fd = fopen(stats_file.c_str(), "w");
    if (fd == NULL) return false;

    time_t now;
    time(&now);
    struct tm * timeinfo;
    timeinfo = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%m/%d/%y %H:%M:%S", timeinfo);
    fprintf(fd, "RenXt stats checkpoint at %s\n", timebuf);
    fprintf(fd, "latest frame: %s (%3.3f sec), elapsed time: %s sec\n", commas("%d", stats->enc_frame), stats->enc_frame / fps, commas("%ld", stats->elapsed));
    fprintf(fd, "#encodes: %s, outbuf overflows: %s, error frames: %s (last was frame %s at %3.3f sec), null frames: %s (%1.1f%%)\n", commas("%d", stats->num_encodes), commas("%u", stats->num_ovfl), commas("%d", stats->num_error), commas("%d", stats->error_frame), safe_div(stats->error_frame, fps), commas("%ld", stats->null_frames), safe_div(100. * stats->null_frames, stats->num_encodes));
    fprintf(fd, "total bytes in: %s (avg %s/frame), bytes out: %s (avg %s/frame), overflow: %s (avg %s/ovfl)\n", commas("%ld", stats->total_inbytes), commas("%ld", safe_div(stats->total_inbytes, stats->num_encodes)), commas("%ld", stats->total_outbytes), commas("%ld", safe_div(stats->total_outbytes, stats->num_encodes)), commas("%ld", stats->total_ovfl), commas("%ld", safe_div(stats->total_ovfl, stats->num_ovfl)));
    fprintf(fd, "avg compression in->out: %1.1f:1 (%1.1f%%)\n", safe_div((float)stats->total_inbytes, stats->total_outbytes), safe_div(100. * stats->total_outbytes, stats->total_inbytes));
    fprintf(fd, "max out bytes: %s (#occur %s, last was frame %s at %3.3f sec), min out bytes: %s (#occur %s, last was frame %s at %3.3f sec), startup bytes: %s\n", commas("%u", stats->max_outbytes), commas("%u", stats->max_occur), commas("%u", stats->max_frame), safe_div(stats->max_frame, fps), commas("%u", stats->min_outbytes), commas("%u", stats->min_occur), commas("%u", stats->min_frame), safe_div(stats->min_frame, fps), commas("%u", stats->first_outbytes));
    fprintf(fd, "#private palettes: mono %s, normal %s, parallel %s, avg size mono %1.1f ents, normal %1.1f, parallel %1.1f\n", commas("%d", stats->num_palettes[RenXt_Stats::PaletteTypes::Mono]), commas("%d", stats->num_palettes[RenXt_Stats::PaletteTypes::Normal]), commas("%d", stats->num_palettes[RenXt_Stats::PaletteTypes::Parallel]), safe_div((double)stats->total_palents[RenXt_Stats::PaletteTypes::Mono], stats->num_palettes[RenXt_Stats::PaletteTypes::Mono]), safe_div((double)stats->total_palents[RenXt_Stats::PaletteTypes::Normal], stats->num_palettes[RenXt_Stats::PaletteTypes::Normal]), safe_div((double)stats->total_palents[RenXt_Stats::PaletteTypes::Parallel], stats->num_palettes[RenXt_Stats::PaletteTypes::Parallel]));
    fprintf(fd, "#palette reduce: mono %s, normal %s, parallel %s, #failed: mono %s, normal %s, parallel %s, max pal: mono %lu, normal %lu, parallel %lu\n", commas("%u", stats->reduce[RenXt_Stats::PaletteTypes::Mono]), commas("%u", stats->reduce[RenXt_Stats::PaletteTypes::Normal]), commas("%u", stats->reduce[RenXt_Stats::PaletteTypes::Parallel]), commas("%u", stats->reduce_failed[RenXt_Stats::PaletteTypes::Mono]), commas("%u", stats->reduce_failed[RenXt_Stats::PaletteTypes::Normal]), commas("%u", stats->reduce_failed[RenXt_Stats::PaletteTypes::Parallel]), stats->maxpal[RenXt_Stats::PaletteTypes::Mono], stats->maxpal[RenXt_Stats::PaletteTypes::Normal], stats->maxpal[RenXt_Stats::PaletteTypes::Parallel]);
    long total_opc = 0;
    for (int i = 0; i < 256; ++i)
        total_opc += stats->num_opc[i];
#define OpcValAndPct(val)  commas("%ld", stats->num_opc[val]), safe_div(100. * stats->num_opc[val], total_opc)
//TODO: show all, or at least break down by groups?
    fprintf(fd, "#opcodes generated: Ack %s (%1.1f%%), SetPal %s (%1.1f%%), SetAll %s (%1.1f%%), DumbList %s (%1.1f%%), NodeList %s (%1.1f%%), Bitmap %s (%1.1f%%), NodeFlush %s (%1.1f%%), ReadReg %s (%1.1f%%), WriteReg %s (%1.1f%%), SetType %s (%1.1f%%), NodeType %s (%1.1f%%), total %s", OpcValAndPct(RENXt_ACK), OpcValAndPct(RENXt_SETPAL(0)), OpcValAndPct(RENXt_SETALL(0)), OpcValAndPct(RENXt_DUMBLIST), OpcValAndPct(RENXt_NODELIST(0)), OpcValAndPct(RENXt_BITMAP(0)), OpcValAndPct(RENXt_NODEFLUSH), OpcValAndPct(RENXt_READ_REG), OpcValAndPct(RENXt_WRITE_REG), OpcValAndPct(RENXt_SETTYPE(0)), OpcValAndPct(RENXt_NODEBYTES), commas("%ld", total_opc));
    fclose(fd);
    return true;
#undef OpcValAndPct
}


//RenXt byte stream encoder/decoder:
class MyOutStream
{
private:
//    byte* ptr; //use caller's storage to avoid additional memory copying (perf)
//NOTE: vector contents seem to move occassionally, so can't just store a ptr
//    size_t maxlen;
//    size_t pad_rate;
public:
    std::vector<byte> frbuf;
    size_t used, rdofs, since_pad, pad_rate;
    byte PrevByte;
    long stats_opc[256];
public:
    MyOutStream(size_t maxlen, size_t padlen): used(0), rdofs(0), since_pad(0), pad_rate(padlen), doing_opcode(0)
    {
        frbuf.resize(maxlen);
        memset(&stats_opc[0], 0, sizeof(stats_opc));
    }
public:
    inline size_t overflow(void) const { return max_safe<int>(used - frbuf.size(), 0); }
    void rewind(size_t len, int new_pad = -1, int new_maxlen = -1)
    {
        rdofs = 0;
        used = len;
        if (new_pad != -1) pad_rate = (size_t)new_pad;
        if (new_maxlen != -1) frbuf.resize(new_maxlen);
        rdofs = 0;
        PrevByte = 0;
        since_pad = 0;
    }
//insert kludgey chars after read op to ensure known state:
    void StopRead(void)
    {
        debug(50, "stop read, since pad %d vs. pad limit %d", since_pad, pad_rate);
        emit_raw(RENARD_SYNC); //terminate prev read and enter known state; kludge: need to send 2 syncs in case last byte of prev read buf was Escape
        if (since_pad < 2) pad(); //kludge: re-pad in case prev pad was defeated by trailing Escape
    }
#if 1
    class Opcode
    {
    public:
//        enum Scheduling {Config = 0, PreCheck = 0x10, Palette = 0x20, ClearNodes = 0x30, SetNodes = 0x40, PostCheck = 0x50, Flush = 0x60}; //relative execution order of opcodes
    public:
        byte adrs;
//TODO        size_t exec_order; //force dependent opcodes to execute first
        size_t stofs; //first byte of opcode
        size_t enclen; //#encoded bytes in opcode
        size_t data_ofs; //readback verification needs to tolerate varying data after this ofs
        size_t duration; //how long this opcode takes to execute; non-0 to delay opcodes that follow
        size_t intlv_stofs; //opcode start ofs (aka execution time) after interleave to reduce wait state overlap
        byte ends_with; //does opcode require Sync + adrs or other opcode at end?
//        static size_t prev_waitst; //wait states propagate to *next* opcode
    public:
        Opcode(byte adrs, size_t exec, size_t stofs, size_t enclen = 0, size_t data_ofs = -1, size_t duration = 0, byte terminator = 0): adrs(adrs), /*exec_order(exec),*/ stofs(stofs), enclen(enclen), data_ofs(data_ofs), duration(duration), intlv_stofs(0), ends_with(terminator) {}
//        {
//            if (prev_waitst == (size_t)-1) error("this opcode will never execute"); //serious problem here; should never happen
//            /*if (new_waitst > prev_waitst)*/ prev_waitst = new_waitst; //delay *next* opcode
//        }
    };
//    size_t /*Opcode::*/prev_waitst;
    Opcode* doing_opcode;
//    byte opc_adrs; //retained only for debug
    std::vector<std::vector<Opcode>> opcodes; //immed_opcodes, defer_opcodes; //opcodes by adrs
#define BeginOpcode(...)  BeginOpcode_from(__LINE__, __VA_ARGS__)
    void BeginOpcode_from(int where, byte adrs, size_t order) //, byte terminator = 0, size_t enc_length = 0, size_t duration = 0, size_t data_ofs = -1)
    {
        EndOpcode_from(where); //__LINE__);
//        std::vector<std::vector<Opcode>>& opcodes = (waits == (size_t)-1)? defer_opcodes: immed_opcodes;
        size_t slot = (adrs + 2) % 256; //>= 0xfe)? adrs - 0xfe: adrs + 2; //kludge: remap dummy adrs to keep array small and reduce scanning overhead; also makes it easier to find in debug (puts it first)
        if (slot >= opcodes.size()) opcodes.resize(slot + 1);
//        if (!opcodes[adrs].size()) Opcode::prev_waitst = 0; //reset wait state; NOT only cumulative within each adrs
        opcodes[slot].emplace_back(adrs, order, used); //, enc_length, data_ofs, duration, terminator);
        doing_opcode = &opcodes[slot].back(); //opc_adrs = adrs;
        debug_from(where, 10, "begin opcode#%u: adrs x%x, slot %u, stofs x%x, enclen %u, data ofs %d, duration %d, terminator? x%x", opcodes[slot].size() - 1, adrs, slot, doing_opcode->stofs, doing_opcode->enclen, doing_opcode->data_ofs, doing_opcode->duration, doing_opcode->ends_with);
    }
    void BeginOpcodeData(void) //set start of variable opcode return data
    {
        if (doing_opcode) doing_opcode->data_ofs = used - doing_opcode->stofs; //start of variable ret data for this opcode
    }
#define EndOpcode()  EndOpcode_from(__LINE__) //, __VA_ARGS__)
    void EndOpcode_from(int where)
    {
        if (!doing_opcode) return;
        if (!doing_opcode->enclen) doing_opcode->enclen = used - doing_opcode->stofs; //set length on variable length opcodes
        size_t slot = (doing_opcode->adrs + 2) % 256; //>= 0xfe)? adrs - 0xfe: adrs + 2; //kludge: remap dummy adrs to keep array small and reduce scanning overhead
        if (!doing_opcode->enclen)
        {
            debug_from(where, 20, "cancelling empty opcode for adrs x%x, stofs x%x", doing_opcode->adrs, doing_opcode->stofs);
            opcodes[slot].pop_back();
            doing_opcode = 0;
            return;
        }
//        debug(10, "end opcode: stofs %u, enclen %u, waits %u", doing_opcode->stofs, doing_opcode->enclen, doing_opcode->waits);
        char desc[80];
        sprintf(desc, "end opcode @%d: adrs x%x, slot %lu, stofs x%lx, enclen %lu, data ofs %lu, duration %lu, terminator x%x", where, doing_opcode->adrs, slot, doing_opcode->stofs, doing_opcode->enclen, doing_opcode->data_ofs, doing_opcode->duration, doing_opcode->ends_with);
        showbuf(desc, &frbuf[doing_opcode->stofs], doing_opcode->enclen, true); //opcodes are usually short, so show full contents
        doing_opcode = 0;
    }
    void SetOpcodeDuration(size_t duration) //set execution time for this opcode
    {
        if (doing_opcode) doing_opcode->duration = duration;
    }
    void SetOpcodeTerminator(byte opc) //remember how to terminate this opcode
    {
        if (doing_opcode) doing_opcode->ends_with = opc;
    }
//    void DelayNextOpcode(size_t new_waitst)
//    {
//        if (!doing_opcode) return;
//        Opcode::prev_waitst = new_waitst;
//    }
//    void CancelOpcode(void)
//    {
//        if (!doing_opcode) return;
//        if (used != doing_opcode->stofs) error("orphaned opcode bytes"); //shouldn't happen
//        opcodes[doing_opcode->adrs].pop_back();
//        doing_opcode = 0;
//    }
#endif // 1
    void SendConfig(byte adrs, byte node_type, byte node_bytes)
    {
        BeginOpcode(adrs, 0); //, RENARD_SYNC); //CAUTION: Sync + next adrs must be included with opcode in order to terminate WriteReg
        SetOpcodeTerminator(RENARD_SYNC);
//        emit_raw(RENARD_SYNC); //start of packet for this controller
//        emit(adrs);
//        if (!node_type && !node_bytes) return; //don't need to send node config info
//        if (node_type)
//        {
//        out.emit(RENXt_SETTYPE(0)); //override value in eeprom
#if 1
//firmware requires SentNodes state bit to be off in order to change node type (otherwise flagged as protocol error)
//since we know what all the state bits should be, just overwrite them all rather than using a complicated read-modify-write process to try to preserve some of them
//state bits are at 0x73 in PIC shared RAM; 0x01 = Echo (should be on), 0x02 = Esc pending (should be off), 0x04 = Protocol inactive (should be off), 0x08 = Sent nodes (should be on)
//        BeginOpcode(__LINE__, adrs, 5);
        emit_opc(RENXt_WRITE_REG);
        emit_uint16(INRAM(0x73)); //2 byte adrs of firmware Status bits; TODO: use const from RenXt.h
        emit(RENXt_WRITE_REG ^ 0 ^ 0x73); //mini-checksum
        emit(0x01); //firmware State bits: Echo = on, Esc = off, Protocol = active, Sent = off; TODO: use const from RenXt.h
//            emit_raw(RENARD_SYNC); //force end of write; must remain part of this opcode
//            emit(adrs); //need adrs to follow Sync
        BeginOpcode(adrs, +1); //, 2+1+2); //opcode separator needed for Sync, so start new opcode
#endif
//        BeginOpcode(__LINE__, adrs, 1);
        emit_opc(RENXt_SETTYPE(node_type)); //changes value if nodes not already sent
#if 1 //1.14 does NOT do a SetAll after setting node type
//            out.emit(RENXt_NOOP, divup(node_bytes, prop->desc.ctlrnodes), prop->isparapal? 14*8 /*8 MIPS: 20 instr == 8 nodes*/: 32 /*5 MIPS, or 48 for 8 MIPS*/)); //PIC takes ~ 50 instr (~11 usec @5 MIPS or ~7 usec @8 MIPS) to set 4 node-pairs (8 nodes), and chars arrive every ~ 44 usec (8N2 is 11 bits/char), so delay next opcode to give set-all time to finish
//        emit_raw(RENARD_SYNC); //cancel SetAll
//        emit(adrs);
#endif // 1
//        }
//        if (node_bytes)
//        {
//        BeginOpcode(__LINE__, adrs, 2);
        emit_opc(RENXt_NODEBYTES); //sets node count to next byte (prep for clear-all or set-all); short so group it with SetType
        emit(node_bytes); //ABS(prop->desc.numnodes) / 2);
//        }
    }
#if 0
    void terminate_pkt(byte adrs) //terminate a packet prematurely; used to send only partial data
    {
        emit_raw(RENARD_SYNC); //start of packet for this controller tells firmware to stop previous opcode
        if (adrs) emit(adrs); //if more opcodes follow, need to re-address current processor; NOTE: this needs to stay part of current opcode; don't start next opcode yet
    }
//start a new packet:
//caller decides whether to send node type and size; typically first time only
    void start_pkt(int where, byte adrs, byte node_type = 0, byte node_bytes = 0)
    {
        BeginOpcode(where, !(adrs & 0x80)? adrs: 0); //CAUTION: assumes Sync + adrs must be included with opcode (otherwise caller would not need a new pkt start); kludge: use slot 0 for 0xfe to reduce array scanning
        emit_raw(RENARD_SYNC); //start of packet for this controller
        emit(adrs);
        if (!node_type && !node_bytes) return; //don't need to send node config info
//        if (node_type)
//        {
//        out.emit(RENXt_SETTYPE(0)); //override value in eeprom
#if 1
//firmware requires SentNodes state bit to be off in order to change node type (otherwise flagged as protocol error)
//since we know what all the state bits should be, just overwrite them all rather than using a complicated read-modify-write process to try to preserve some of them
//state bits are at 0x73 in PIC shared RAM; 0x01 = Echo (should be on), 0x02 = Esc pending (should be off), 0x04 = Protocol inactive (should be off), 0x08 = Sent nodes (should be on)
//        BeginOpcode(__LINE__, adrs, 5);
        emit(RENXt_WRITE_REG);
        emit_uint16(INRAM(0x73)); //2 byte adrs of firmware Status bits; TODO: use const from RenXt.h
        emit(RENXt_WRITE_REG ^ 0 ^ 0x73); //mini-checksum
        emit(0x01); //firmware State bits: Echo = on, Esc = off, Protocol = active, Sent = off; TODO: use const from RenXt.h
        emit_raw(RENARD_SYNC); //force end of write; must remain part of this opcode
        emit(adrs); //need adrs to follow Sync
        BeginOpcode(__LINE__, adrs); //, 2+1+2);
#endif
//        BeginOpcode(__LINE__, adrs, 1);
        emit(RENXt_SETTYPE(node_type)); //changes value if nodes not already sent
#if 1 //1.14 does NOT do a SetAll after setting node type
//            out.emit(RENXt_NOOP, divup(node_bytes, prop->desc.ctlrnodes), prop->isparapal? 14*8 /*8 MIPS: 20 instr == 8 nodes*/: 32 /*5 MIPS, or 48 for 8 MIPS*/)); //PIC takes ~ 50 instr (~11 usec @5 MIPS or ~7 usec @8 MIPS) to set 4 node-pairs (8 nodes), and chars arrive every ~ 44 usec (8N2 is 11 bits/char), so delay next opcode to give set-all time to finish
//        emit_raw(RENARD_SYNC); //cancel SetAll
//        emit(adrs);
#endif // 1
//        }
//        if (node_bytes)
//        {
//        BeginOpcode(__LINE__, adrs, 2);
        emit(RENXt_NODEBYTES); //sets node count to next byte (prep for clear-all or set-all)
        emit(node_bytes); //ABS(prop->desc.numnodes) / 2);
//        }
    }
#endif // 0
    inline size_t unused(void) { return frbuf.size() - used; }
    void emit_buf(const void* values, size_t len)
    {
        const byte* inptr = (const byte*)(const void*)values; //kludge: gcc not allowing inline cast to l-value, so use alternate var
        while (len--) emit(*inptr++); //copy byte-by-byte to handle special chars and padding; escapes will be inserted as necessary
    }
    void emit_rawbuf(const void* values, size_t len)
    {
        const byte* inptr = (const byte*)(const void*)values; //kludge: gcc not allowing inline cast to l-value, so use alternate var
        while (len--) emit_raw(*inptr++); //copy byte-by-byte to handle special chars and padding; caller is responsible for escapes
//        if (used < frbuf.size()) frbuf[used++] = value; //track length regardless of overflow
    }
    void emit_rgb(node_value rgb) //ensure correct byte order
    {
        emit(RGB2R(rgb));
        emit(RGB2G(rgb));
        emit(RGB2B(rgb));
    }
    void emit_uint32(uint32_t val) //ensure correct byte order
    {
        emit(val / 0x1000000);
        emit(val / 0x10000);
        emit(val / 0x100);
        emit(val);
    }
    void emit_uint16_raw(uint16_t val) //ensure correct byte order
    {
        emit_raw(val / 0x100);
        emit_raw(val % 0x100);
    }
    void emit_uint16(uint16_t val) //ensure correct byte order
    {
        emit(val / 0x100);
        emit(val % 0x100);
    }
    void emit_opc(byte value, size_t count = 1)
    {
        stats_opc[value] += count;
        emit(value, count);
    }
    void emit(byte value, size_t count = 1)
    {
        while (count-- > 0)
        {
            if (IsRenardSpecial(value))
            {
                emit_raw(RENARD_ESCAPE);
                ++stats_opc[RENARD_ESCAPE];
                if (doing_opcode && doing_opcode->enclen) ++doing_opcode->enclen; //include extra esc codes in count
            }
            emit_raw(value);
        }
    }
    void pad(void)
    {
        since_pad = 0; //avoid recursion
        emit_raw(RENARD_PAD);
        ++stats_opc[RENARD_PAD];
        debug(10, "pad @'0x%x, @rate %d", used, pad_rate);
    }
    void emit_raw(byte value, size_t count = 1)
    {
        while (count -- > 0)
        {
//            debug(90, "%semit_raw 0x.6%x to ofs %d", (used < frbuf.size())? "": "not-", value, used);
            if (used < frbuf.size()) frbuf[used++] = value;  //track length regardless of overflow
            else ++used; //keep track of overflow
            ++since_pad;
            PrevByte = ((value != RENARD_ESCAPE) || (PrevByte != RENARD_ESCAPE))? value: 0; //kludge: treat escaped escape as a null for pad check below
#ifdef RENARD_PAD //explicit padding
            if (pad_rate && (since_pad >= pad_rate) /* !(used % pad_rate)*/)
                if (PrevByte != RENARD_ESCAPE) pad(); //CAUTION: recursive
#endif
        }
    }
    void remove_pads(void) //for loop-back debug only
    {
#ifdef RENARD_PAD
        for (size_t i = 0, shrink = 0; i < used + shrink; ++i)
            if (frbuf[i] == RENARD_PAD) { ++shrink; --used; debug(10, "remove pad at [%d/%d]", i, used + shrink); }
            else
            {
                if (shrink) frbuf[i] = frbuf[i + shrink];
                if ((frbuf[i] == RENARD_ESCAPE) && (i+1 < used + shrink))
                {
                    ++i;
                    if (shrink) frbuf[i] = frbuf[i + shrink];
                }
            }
#endif
    }
friend bool RenXt_ReadMem(MyOutStream& out, byte ctlr, uint16_t address, void* buf, size_t len, bool enque, bool first = false);
    bool deque_sync(bool skip)
    {
        debug(99, "deque(skip %d): used %d, rdofs 0x%x, buf 0x%x", skip, used, rdofs, (rdofs < frbuf.size())? frbuf[rdofs]: -1);
        if (!skip) return (deque_raw() == RENARD_SYNC);
        for (;;)
        {
            if ((used > 1) && (frbuf[rdofs] == RENARD_ESCAPE)) { used -= 2; rdofs += 2; continue; } //not a real Sync
            if (deque_raw() != RENARD_SYNC) continue;
            while (used && (frbuf[rdofs] == RENARD_SYNC)) { --used; ++rdofs; } //consume multiple Sync if there
            return true;
        }
    }
    uint16_t deque_uint16() //ensure correct byte order
    {
        uint16_t retval = deque() * 0x100; //big endian
        retval += deque();
        return retval;
    }
    void deque_buf(void* values, size_t len)
    {
        byte* inptr = (byte*)(void*)values; //kludge: gcc not allowing inline cast to l-value, so use alternate var
        while (len--) *inptr++ = deque(); //copy byte-by-byte to handle special chars and padding
    }
    byte deque(void)
    {
        if (used && (frbuf[rdofs] == RENARD_ESCAPE)) { --used; ++rdofs; }
        return deque_raw();
    }
    byte deque_raw(void)
    {
        if (used > 0) { --used; return PrevByte = frbuf[rdofs++]; }
        return RENARD_SYNC; //simulate Sync if past end of buffer
    }
};
//size_t MyOutStream::Opcode::prev_waitst;


//serial port wrapper:
//SerialPort is from xLights and defines the following methods:
//    int AvailableToRead();
//    int WaitingToWrite();
//    int SendBreak();
//    int Close();
//    int Open( const wxString& portname, int baudrate, const char* protocol = "8N1" );
//    bool IsOpen();
//    int Read(char* buf,size_t len);
//    int Write(char* buf,size_t len);
#include "serial.h" //from xLights
class MyPort: public SerialPort
{
public:
    std::string name; //port name
    int baud_rate; //typically 115200, 250000, 500000
    char bits[3 +1]; //data bits, parity, stop bits; normally 8N1
    int pad_rate;
    int fps;
//    std::vector<byte> frbuf; //frame I/O buffer
    MyOutStream io; //(0, 0, 0); //out(outbuf, outlen, pad_rate);
    enum { FakeClosed, FakeOpen, IsRealPort } state;
//    bool is_open;
    const char* kludgey_prefix = "\\\\.\\"; //http://support.microsoft.com/kb/115831/en-us
    RenXt_Stats stats;
public: //ctor
    MyPort(void): io(0, 0), state(FakeClosed) { memset(&stats, 0, sizeof(stats)); /*Synchronous = true*/; } //IsRealPort(true) {};
public:
    bool SetName(const char* name_new)
    {
        static wxArrayString /*std::vector<std::string>*/ comm_ports;
//        TCHAR valname[32];
//        /*byte*/TCHAR portname[32];
//        DWORD vallen = sizeof(valname);
//        DWORD portlen = sizeof(portname);
//        HKEY hkey = NULL;
        DWORD err = 0;

        if (name_new && !strncmp(name_new, kludgey_prefix, strlen(kludgey_prefix))) name_new += strlen(kludgey_prefix);
//enum serial comm ports logic based on http://www.cplusplus.com/forum/windows/73821/:
        if (comm_ports.empty()) err = SerialPort::EnumPorts(comm_ports); //should this be cached?  it's not really that expensive
//            if (!(err = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DEVICEMAP\\SERIALCOMM"), 0, KEY_READ, &hkey)))
//                for (DWORD inx = 0; !(err = RegEnumValue(hkey, inx, (LPTSTR)valname, &vallen, NULL, NULL, (LPBYTE)portname, &portlen)) || (err == ERROR_MORE_DATA); ++inx)
//                {
//                    if (err == ERROR_MORE_DATA) portname[numents(portname) - 1] = '\0'; //need to enlarge read buf if this happens; just truncate string for now
//                    debug(3, "found port[%d] %d:'%s' = %d:'%s', err 0x%x", inx, vallen, valname, portlen, portname, err);
//                    comm_ports.push_back(portname);
//                    vallen = sizeof(valname);
//                    portlen = sizeof(portname);
//                }
//        if (err && (err != /*ERROR_FILE_NOT_FOUND*/ ERROR_NO_MORE_ITEMS)) error("Can't get serial comm ports from registry: error %d", err);
//        if (hkey) RegCloseKey(hkey);
//        if (err) SetLastError(err); //tell caller about last real error
        if (err) return false;

        std::string choices;
        state = FakeClosed; //IsRealPort = false;
        name = name_new;
        for (auto it = comm_ports.begin(); it != comm_ports.end(); ++it)
            if (name_new && !stricmp(it->c_str(), name_new)) { state = IsRealPort; break; }
            else choices += ", " + *it;
        choices += ", NUL";
        debug(3, "port '%s' is real? %d", name_new? name_new: "(null)", state == IsRealPort);
        if ((state != IsRealPort) && name_new && stricmp(name_new, "NUL"))
            return error("Unrecognized port: %s (choices are %s)", name_new, choices.substr(2).c_str()), false;
//        name = kludgey_prefix + name; //allow it to be openeed
        return true;
    }
    bool SetBaud(int baud_new)
    {
        static int std_bauds[] = {57600, 115200, 2*115200, 4*115200, 242500, 240 K, 250 K, 500 K, 0}; //NOTE: allow 240K as a way to slow down sender a little
        std::string choices;
        for (size_t i = 0; (i < numents(std_bauds)) && (std_bauds[i] != baud_new); ++i)
            if (!std_bauds[i]) return error("Unsupported baud rate: %d (choices are %s)", baud_new, choices.substr(2).c_str()), false; //end of list
            else { choices += ", "; char buf[10]; choices += itoa(std_bauds[i], buf, 10); }
        baud_rate = baud_new;
        return true;
    }
#ifndef __WXMSW__
    static const char* itoa(int val, char* buf, int base = 10)
    {
        if ((base < 2) || (base > 16)) { sprintf(buf, "error: base %d", base); return buf; }
        bool neg = (val < 0);
        if (neg) val = 0 - val;
        char* revbuf = buf;
        for (;;)
        {
            *revbuf++ = "0123456789abcdef"[val % base];
            val /= base;
            if (!val) break;
        }
        if (neg) *revbuf++ = '-';
//2: '0 <-> '1
//3: '0 <-> '2
//4: '0 <-> '3, '1 <-> '2
//etc
        for (int ofs = (revbuf - buf) / 2; ofs > 0; --ofs) //NOTE: don't need to swap middle char when length is odd
        {
            char ch = buf[ofs]; buf[ofs] =  revbuf[-1 - ofs]; revbuf[-1 - ofs] = ch;
        }
        return buf;
    }
#endif
    bool SetByteConfig(const char* bits_new)
    {
        static const char* std_bits[] = {"8N1", "8N1.5", "8N2", 0};
        std::string choices;
        for (size_t i = 0; (i < numents(std_bits)) && stricmp(std_bits[i], bits_new); ++i)
            if (!std_bits[i]) return error("Unsupported data bits/parity/stop bits combination: '%s' (choices are %s)", bits_new, choices.substr(2).c_str()), false;
            else { choices += ", "; choices += std_bits[i]; }
        bits[0] = bits_new[0];
        bits[1] = toupper(bits_new[1]);
        bits[2] = bits_new[strlen(bits_new) - 1];
        bits[3] = '\0';
        return true;
    }
    bool SetPadrate(int pad_new)
    {
        if (pad_new)
            if ((pad_new < 5) || (pad_new > 1000)) return error("Invalid pad rate: %d (should be 5..1K)", pad_new), false; //min .1%, max 20%
        pad_rate = pad_new;
        return true;
    }
    bool SetFps(int fps_new)
    {
        if ((fps_new < 1) || (fps_new > 1 K)) return error("Invalid FPS: %d (must be 1..1K)", fps_new), false; //min 1 msec
        fps = fps_new;
        return true;
    }
    bool IsCompatible(int baud_other, const char* bits_other, int pad_other, int fps_other)
    {
        if (baud_other) //optional if already open
            if (baud_other != baud_rate)
                return error("Port '%s' is already open with a different baud rate: %d (vs. %d)", name.c_str(), baud_rate, baud_other), false;
        if (bits_other && *bits_other) //optional if already open
            if ((strlen(bits_other) != 3) || (bits_other[0] != bits[0]) || (toupper(bits_other[1]) != bits[1]) || (bits_other[2] != bits[2]))
                return error("Port '%s' is already open with a different data/parity/stop value: %s (vs. %s)", name.c_str(), bits, bits_other), false;
        if (pad_other != pad_rate)
            return error("Port '%s' is already open with a different pad rate: %s (vs. %d)", name.c_str(), pad_rate, pad_other), false;
        if (fps_other != fps)
            return error("Port '%s' is already open with a different frame rate: %s (vs. %d)", name.c_str(), fps, fps_other), false;
        return true;
    }
public:
    bool IsOpen(void) /*const*/ { return (state == IsRealPort)? SerialPort::IsOpen(): (state == FakeOpen); }
//    int Open(const wxString& portname, int baudrate, const char* protocol)
//    {
//        if (state == IsRealPort) return SerialPort::Open(portname, baudrate, protocol);
//        state = FakeOpen;
//        return 0;
//    }
    bool open(void)
    {
        int bufsize = MaxFrameSize(baud_rate, bits[0] - '0' + ((bits[1] != 'N')? 1: 0) + ((bits[2] <= '2')? bits[2] - '0': 1.5), fps);
        if (bufsize < 100) return error("Port %s baud rate too low: %d gives %d bytes @%d fps", name.c_str(), baud_rate, bufsize, fps), false;
        if (IsOpen()) return error("Port '%s' is already open", name.c_str()), false;
        std::string namefix = kludgey_prefix + name;
//??        if (namefix.back() != ':')) namefix.push_back(':');
        int retval = (state == IsRealPort)? Open(namefix.c_str(), baud_rate, bits): (state = FakeOpen, 0);
        if (retval < 0)
        {
            error("Can't open port '%s': error %d (%ld)", name.c_str(), retval, GetLastError());
            if (!IsOpen()) return false; //some errors actually open the port?
        }
        if (!IsOpen()) return error("Port '%s' failed to open?", name.c_str()), false; //paranoid
//        is_open = true;
//        bufsize = 0x200; //debug buf overflow
        if ((bits[2] == '2') || (bits[2] == '#')) pad_rate = 0; //don't need explicit padding with > 1 stop bit
        io.rewind(0, pad_rate, bufsize * 98/100); //typically 10 bits/byte received; 115k @20 FPS == 576, 250k @20 FPS == 1250; allow a little padding
        debug(5, "open port %s: baud %d, data %d, parity %c, stop %1.1f, pad every %d (%1.1f%%), fps %d, frbufsize %d", name.c_str(), baud_rate, bits[0] - '0', bits[1], (bits[2] == '%')? 1.5: bits[2] - '0', pad_rate, pad_rate? 100. / pad_rate: 0, fps, io.frbuf.size());
        return true;
    }
    bool flush(void)
    {
        if (!io.used) return true; //nothing to flush
//        while (WaitingToWrite());
        int iolen = MIN(io.used, io.frbuf.size()); //don't go past end of buffer
        showbuf("OUT", reinterpret_cast</*const*/ char*>(&io.frbuf[0]), iolen, false); //ABS(RenXt_debug_level) >= 20);
        int retval = (state == IsRealPort)? Write(reinterpret_cast</*const byte* */ char*>(&io.frbuf[0]), iolen): (state == FakeOpen)? iolen: -1; //CAUTION: async I/O
        memset(reinterpret_cast<byte*>(&io.frbuf[0]), 0xDB, io.frbuf.size()); //makes it easier to see stale data
        if (retval < /*(int)iolen*/ 0) return error("Can't write port '%s': error %d (%ld)", name.c_str(), retval, GetLastError()), false;
        io.rewind(0, pad_rate);
        return true;
    }
    bool input(int msec)
    {
        io.rewind(0, pad_rate);
//        msec *= 10;
        debug(10, "input: wait %d msec", msec);
        usleep(msec K);
        int retval = (state == IsRealPort)? Read(reinterpret_cast</*byte* */ char*>(&io.frbuf[0]), io.frbuf.size()): (state == FakeOpen)? io.frbuf.size(): -1;
        debug(10, "input: real? %d, read len %d, got len %d", state == IsRealPort, io.frbuf.size(), retval);
        if (retval /*<= 0*/ < 0) return error("Can't read port '%s': error %d (%ld)", name.c_str(), retval, GetLastError()), false;
        else showbuf("IN", reinterpret_cast<byte*>(&io.frbuf[0]), retval, false); //ABS(RenXt_debug_level) >= 20);
        io.used = retval;
        return true;
    }
    bool close(void)
    {
        if (!IsOpen()) return error("Port '%s' not open", name.c_str()), false;
        debug(10, "%d of %d bytes waiting to go to %s", io.used, io.frbuf.size(), name.c_str());
        if (io.used) //anything to flush?
        {
            if (!flush()) return false;
            io.rewind(0, 0, 0);
        }
//        error("TODO: close port '%s'", name.c_str());
//        is_open = false;
        int retval = (state == IsRealPort)? Close(): (state == FakeOpen)? (state = FakeClosed, 0): -1;
        if (retval < 0) return error("Can't close port '%s': error %d (%ld)", name.c_str(), retval, GetLastError()), false;
        if (IsOpen()) return error("Port '%s' failed to close?", name.c_str()), false; //paranoid
        debug(5, "close port %s: baud %d, data %d, parity %c, stop %1.1f, pad %d, fps %d, frbufsize %d", name.c_str(), baud_rate, bits[0] - '0', bits[1], (bits[2] == '%')? 1.5: bits[2] - '0', pad_rate, fps, io.frbuf.size());
        return true;
    }
};

MyHashMap<std::string, MyPort> OpenPorts;


//palette entry:
class PalentInfo
{
public:
    size_t inx; //index# assigned to this color (decreasing order by frequency)
    size_t freq; //#occurrences of this color
    size_t freq16; //#occurrences of this color beyond first 256 nodes (requires > 1 byte each)
    size_t maxnode; //highest node# that uses this color
    node_value key; //primary (initial) key for this entry; multiple aliases might point to same entry
//    node_value hsv; //alternate color model (for blending); NOTE: only populated if needed
    struct HSV
    {
        uint16_t h; //max = 360 so byte will not work here
        byte s, v;
    };
    HSV hsv;
//    int len3d; //distance ^ 2 of 3D point from origin
//    int sum; //used to pick averaged value after merging
    std::vector<int> node_list; //inverted node lists (increasing node# order); are bitmap or rescan better?
public:
//    inline bool operator() (const PalentInfo& lhs, const PalentInfo& rhs) const { return lhs.freq < rhs.freq; } //for use with priority_queue
//    inline bool operator() (PalentInfo* lhs, PalentInfo* rhs) const { return lhs->freq < rhs->freq; } //for use with priority_queue
    PalentInfo(node_value key = 0, size_t inx = 0): inx(inx), freq(0), freq16(0), maxnode(0), key(key) {};
public:
    bool IsDeleted(void) const { return (int)freq <= 0; }
    bool merge(/*const*/ PalentInfo& other)
    {
        if (IsDeleted() || other.IsDeleted()) return false;
        freq += other.freq;
        freq16 += other.freq16;
        maxnode = MAX(maxnode, other.maxnode);
        other.inx = inx; //repoint merged entry to find new entry
        other.freq = -1; //mark deleted
        return true;
    }
//    node_value diff(node_value origin) const //"distance" of color from origin (black or another color)
//    {
//        int rdiff = RGB2R(key) - RGB2R(origin), gdiff = RGB2G(key) - RGB2G(origin), bdiff = RGB2B(key) - RGB2B(origin);
////        return sqrt(rdiff * rdiff + gdiff * gdiff + bdiff * bdiff); //NOTE: omit sqrt (for perf); don't need it for relative compares
//        return RGB2Value(ABS(rdiff), ABS(gdiff), ABS(bdiff));
//    }
};

//kludge to access protected members:
#if 0 //sort broken
class Palette;
template <typename T>
class MyVector: public std::vector<T>
{
friend class Palette;
};
template <typename Tdata, typename Tsorter>
class MyQueue: public std::priority_queue<Tdata, MyVector<Tdata>, Tsorter>
{
friend class Palette;
};
#endif


//pivot 4 1-byte values into one 4-byte value:
#define SetParallelPalette(parapal, color0, color1, color2, color3)  \
{ \
	parapal[0] = (((color0) & 0x80) >> (0-0)) | (((color1) & 0x80) >> (1-0)) | (((color2) & 0x80) >> (2-0)) | (((color3) & 0x80) >> (3-0)) | (((color0) & 0x40) >> (4-1)) | (((color1) & 0x40) >> (5-1)) | (((color2) & 0x40) >> (6-1)) | (((color3) & 0x40) >> (7-1)); \
	parapal[1] = (((color0) & 0x20) << (0-(0-2))) | (((color1) & 0x20) << (0-(1-2))) | (((color2) & 0x20) >> (2-2)) | (((color3) & 0x20) >> (3-2)) | (((color0) & 0x10) >> (4-3)) | (((color1) & 0x10) >> (5-3)) | (((color2) & 0x10) >> (6-3)) | (((color3) & 0x10) >> (7-3)); \
	parapal[2] = (((color0) & 0x08) << (0-(0-4))) | (((color1) & 0x08) << (0-(1-4))) | (((color2) & 0x08) << (0-(2-4))) | (((color3) & 0x08) << (0-(3-4))) | (((color0) & 0x04) << (0-(4-5))) | (((color1) & 0x04) >> (5-5)) | (((color2) & 0x04) >> (6-5)) | (((color3) & 0x04) >> (7-5)); \
	parapal[3] = (((color0) & 0x02) << (0-(0-6))) | (((color1) & 0x02) << (0-(1-6))) | (((color2) & 0x02) << (0-(2-6))) | (((color3) & 0x02) << (0-(3-6))) | (((color0) & 0x01) << (0-(4-7))) | (((color1) & 0x01) << (0-(5-7))) | (((color2) & 0x01) << (0-(6-7))) | (((color3) & 0x01) >> (7-7)); \
}


//enum ColorOrder { RGB, RBG, GRB, GBR, BRG, BGR};

//reorder RGB values:
//reorder during output to allow fx generation to always use RGB order
//NOTE: LED strip with WS281x integrated into LED seems to have R+ G swapped
node_value rgb_reorder(int order, const byte* values)
{
    int threshold = order & ~0xFF; //mask off threshold for on/off types
    if ((threshold == BOOL_RED) || (threshold == BOOL_GREEN) || (threshold == BOOL_BLUE) || (threshold == BOOL_ANY))
    {
        threshold = order & 0xFF;
        order ^= threshold;
    }
    switch (order)
    {
        default: //assume RGB
        case RGB_ORDER: //0x524742: //"RGB"
//        case ColorOrder::RGB:
            return RGB2Value(values[0], values[1], values[2]);
        case RBG_ORDER: //0x524247: //"RBG"
//        case ColorOrder::RBG:
            return RGB2Value(values[0], values[2], values[1]);
        case GRB_ORDER: //0x475242: //"GRB"
//        case ColorOrder::GRB:
            return RGB2Value(values[1], values[0], values[2]);
        case GBR_ORDER: //0x474252: //"GBR"
//        case ColorOrder::GBR:
            return RGB2Value(values[1], values[2], values[0]);
        case BGR_ORDER: //0x424752: //"BGR"
//        case ColorOrder::BGR:
            return RGB2Value(values[2], values[1], values[0]);
        case BRG_ORDER: //0x425247: //"BRG"
//        case ColorOrder::BRG:
            return RGB2Value(values[2], values[0], values[1]);
        case MONO_RED: //0x52
            return RGB2Value(values[0], 0, 0);
        case MONO_GREEN: //0x47
            return RGB2Value(0, values[0], 0);
        case MONO_BLUE: //0x42
            return RGB2Value(0, 0, values[0]);
        case MONO_ANY: //1
            return RGB2Value(MAX(MAX(values[0], values[1]), values[2]), 0, 0);
        case MONO_AVG: //3
            return RGB2Value((values[0] + values[1] + values[2]) / 3, 0, 0);
        case BOOL_RED:  //0x7200 + threshold
            return RGB2Value((values[0] > threshold)? 0xFF: 0, 0, 0);
        case BOOL_GREEN:  //0x6700 + threshold
            return RGB2Value((values[1] > threshold)? 0xFF: 0, 0, 0);
        case BOOL_BLUE:  //0x6200 + threshold
            return RGB2Value((values[2] > threshold)? 0xFF: 0, 0, 0);
        case BOOL_ANY:  //0x100 + threshold
            return RGB2Value((MAX(MAX(values[0], values[1]), values[2]) > threshold)? 0xFF: 0, 0, 0); //> threshold allows 0 to be used as default threshold
//        default:
//            error("unknown RGB order: %d", order);
    }
}

#define IsGECE(type)  !((prop->desc.node_type ^ RENXt_GECE(DONT_CARE)) & ~1)

//convert RGB to GECE color format:
node_value RGB2IBGRZ(node_value color)
{
    int r = RGB2R(color), g = RGB2G(color), b = RGB2B(color);
//    int intensity = MIN(MAX(MAX(r, g), b), 0xCC); //max GECE intensity = 0xCC
    int intensity = MAX(MAX(r, g), b);
    if (intensity)
    {
        int intscale = rdiv(intensity, 15); //0..255 => 0..17
//convert RGB to IBGR: I = max(R, G, B), B = B / (I / 15), G = G / (I / 15), R = R / (I / 15)
//examples:
//0xFF,FF,FF => 0xCC,F,F,F
//0x00,00,00 -=> 0x00,x,x,x
//0x40,80,FF => 0xCC,4,8,F => 0x36,6D,CC
//0xCC,FF,FF => 0xCC,C,F,F => 0xA3,CC,CC
//0x10,20,30 => 0x30,5,A,F
        if (r) { r = rdiv(r, intscale); if (!r) ++r; }
        if (g) { g = rdiv(g, intscale); if (!g) ++g; }
        if (b) { b = rdiv(b, intscale); if (!b) ++b; }
        intensity = MIN(intensity, 0xCC); //max GECE intensity = 0xCC; do this after scaling R, G, B?
    }
    node_value new_color = RGB2Value(intensity, b << 4 | g, r << 4);
    debug(30, "rgb2ibgrz: %x => %x", color, new_color);
    return new_color;
}


//class ColorDiff
//{
//public:
////    int diff; //difference amount
//    int keep, drop; //palette pair being compared
////    enum DiffType { Hue, Sat, Val};
//    int row; //palette partition (row# for dumb nodes, hue/sat/val for smart nodes)
////    DiffType type;
//    ColorDiff(int keep, int drop, int row): keep(keep), drop(drop), row(row) {}
//};


class Palette: public std::vector<PalentInfo> //allows sequential or random access to palette entries //std::hash_map<node_value, PalentInfo> //typedef struct
{
public:
//    std::hash_map<node_value, PalentInfo /*std::pair<int, int>*/> map; //pair = palette index and freq
//    std::vector<node_value> list;
//    enum { None, SortedByFreq, SortedByColor } sort_order = None;
//    std::vector<node_value> keys; //allow sequential or random access to palette entries
//TODO: move keys into Analyze
//    std::unordered_map<node_value, /*PalentInfo* */ size_t> keys; //allow fast lookups and aliases into palette list during initial construction; not used after that
//    std::priority_queue<PalentInfo*, std::array<PalentInfo*>, PalentCompare> sorted; //allow access by frequency
//    /*std::priority_queue<PalentInfo*, std::list<PalentInfo*>,*/ MyQueue<PalentInfo*, PalentInfo> sorted; //allow access by frequency order
//    std::vector<PalentInfo*> sorted;
//public:
//    Palette(void): byfreq_sorted_size(0), byval_sorted_size(0) {}
//    bool isparapal; //4-way parallel palette entries
//public:
//    Palette& operator+=(const Palette& that);
    enum PaletteType { Mono, Series, Parallel};
public:
//    int size(void) const { return keys.size(); } //allow size to be changed but not affect aliased lookups
//    size_t NumDel(void)
//    {
//        size_t numdel = 0;
//        for (auto it = this->begin(); it != this->end(); ++it)
//            if (it->IsDeleted()) ++numdel;
//        return numdel;
//    }
    size_t size(bool want_del) // = true)
    {
        size_t numdel = 0;
        if (!want_del)
            for (auto it = begin(); it != end(); ++it)
                if (it->IsDeleted()) ++numdel;
        return std::vector<PalentInfo>::size() - numdel;
    }
//    inline PalentInfo* SortedLast(void) const { return sorted./*c.*/back(); }
//    inline bool contains(node_value key) const { return Contains(keys, key); } //.find(key) != keys.end(); }
    size_t /*PalentInfo* */ occur(std::unordered_map<node_value, /*PalentInfo* */ size_t>& keys, node_value rgb, size_t nodenum) //add an occurrence
    {
//        PalentInfo*& palent = keys[rgb]; //inserts new entry if not already there
        size_t svsize = keys.size();
        size_t& inx = keys[rgb]; //NOTE: creates new entry if not there
        if (keys.size() != svsize) // !palent) //rgb value was not already in palette
        {
            inx = size(true); //keys[rgb] = size();
//            if ((inx >= 32) && !bank)
            emplace_back(rgb, inx); //push_back(*(new PalentInfo));
        }
        PalentInfo& palent = (*this)[inx]; // = &(*this)[keys[rgb]]; //inserts new entry if not already there
//            palent = &back(); //point to new palette entry instead of first one
//        }
//        /*std::pair<int, int>*/ PalentInfo& palent = (*this)[rgb]; //*map*/ std::hash_map<node_value, PalentInfo>::operator[](rgb); //insert new entry if not already there
//        if (shared_palette.find(palent.first) != shared_palette.end()) { ++shared_palette[palent.first].second; continue; } //count occurrences
//        palent.second.first = shared_palette.size(); //assign next sequential entry#
//        palent.second.second = 1; //one occurrence
//        shared_palette.insert(palent);
//            if (!palent.second++) shared_palette.list[palent.first = shared_palette.size() - 1] = rgb; //assign next sequential entry#
//        if (!palent.freq++) //rgb value not already in palette
//        {
//            palent->inx = size() - 1; //assign next sequential entry#
//            keys.push_back(rgb);
//            palent->key = rgb; //remember (primary) key that points to this entry
//            palent->len3d = RGB2R(rgb) * RGB2R(rgb) + RGB2G(rgb) * RGB2G(rgb) + RGB2B(rgb) * RGB2B(rgb);
//            dirty = TRUE;
//        }
//        else palent = &(*this)[inx];
        if (++palent.freq > (*this)[max_occur].freq) max_occur = inx;
//        SetDirty(); //entries need to be re-sorted
        if (nodenum >= 256) ++palent.freq16;
        if (nodenum > palent.maxnode) palent.maxnode = nodenum;
//        return palent.second; //new count
//        return palent.inx; //palette entry#
//        palent.freq += more_freq; //allow caller to force sort order
        return inx; //palent.inx;
    }
//    void x_reindex(void)
//    {
//        for (auto it = sorted.begin(); it != sorted.end(); ++it)
//            (*this)[it->second].inx = it - sorted.begin();
//        pal_dump("renumbered indexes");
//    }
    size_t max_occur = 0, first = 0;
    PalentInfo& Bkg(void) { return (*this)[max_occur]; }
    void FindMost(void)
    {
        first = -1;
        max_occur = -1;
        for (auto it = begin(); it != end(); ++it)
        {
            if (it->IsDeleted()) continue;
            if (first == (size_t)-1) first = it - begin();
            if ((max_occur == (size_t)-1) || (it->freq > Bkg().freq)) max_occur = it - begin();
        }
        debug(10, "palette find most: ent[%d], key 0x%x, freq %d, first is %d", max_occur, Bkg().key, Bkg().freq, first);
//        if (maxocc == 0) return;
//        PalentInfo tmp = (*this)[0]; (*this)[0] = (*this)[maxocc]; (*this)[maxocc] = tmp; //move most freq to start of list
//        for (auto it = begin(); it != end(); ++it)
//            if (it->freq > (*this)[maxocc].freq) maxocc = it - begin();
//
//        max_occur = 0;
    }
    size_t Reord(size_t inx, bool delok = false)
    {
        if (inx >= size(true)) { debug(1, "BAD PALENT: %d", inx); inx = 0; }
        inx = (*this)[inx].inx; //points to real (merged/remaining) entry
        if (inx >= size(true)) { debug(1, "BAD PALENT: %d", inx); inx = 0; }
//        if (!delok && (*this)[inx].IsDeleted()) { debug(1, "ORPH PALENT: %d", inx); inx = 0; }
        return inx; //redirect to sorted/merged entry
    }
    void dump(const char* desc)
    {
//        if (!size(true)) { error("empty palette"); return; }
        debug(20, "palette '%s', max occ ent %d", desc, max_occur);
        checkpoint(__LINE__, max_occur, first, size(true));
        Reord(max_occur);
        int total_occ = 0, total_ents = 0;
        for (auto it = begin(); it != end(); ++it)
        {
//            PalentInfo* ptr = &(*this)[Reord(it->inx)]; //redirect to sorted/merged entry
//            if (it->IsDeleted()) continue;
            if (!it->IsDeleted()) total_occ += it->freq;
            ++total_ents;
            debug(20, "palette[%d/%d]: redir %d/%d, key 0x%.6x occurs %d times, last was node# %d, deleted? %d", it - begin(), size(true), it->inx, size(false), it->key, it->freq, it->maxnode, it->IsDeleted());
//            if (isparapal) debug_more(20, "%c[%d..%d]", "RGB?"[(*this)[it->second].maxnode & 3], (*this)[it->second].maxnode & ~3, (*this)[it->second].maxnode | 3);
//            else debug_more(20, "%d", (*this)[it->second].maxnode);
        }
        checkpoint(__LINE__, total_ents, total_occ);
        debug(20, "total %d entries, %d occurrences", total_ents, total_occ);
//        std::string buf;
//        for (auto it = keys.begin(); it != keys.end(); ++it)
//        {
//            buf.reserve(buf.length() + 21);
//            snprintf(reinterpret_cast<char*>(&buf[buf.length()]), 20, ", 0x%x->'%d", it->first, it->second);
//        }
//        debug(20, "aliases: %s", buf.c_str() + 2);
    }
//    inline /*PalentInfo* */ int xalias(node_value rgb, int which)
//    {
//        /*PalentInfo*& palent =*/ keys[rgb] = which; //inserts new entry if not already there
////        palent = &(*this)[which];
//        return which; //palent;
//    }
#if 0
    bool merge(int keep, int drop)
    {
        if (!Contains(keys, (*this)[keep].key)) { error("keep key[%d] not present", keep); return false; }
        if (!Contains(keys, (*this)[drop].key)) { error("drop key[%d] not present", drop); return false; }
        if ((*this)[keep].IsDeleted()) return false;
        if ((*this)[drop].IsDeleted()) return false;
#pragma message WARN("TODO: weighted avg/blend 2 colors?")
        (*this)[keep].merge((*this)[drop]); //combine info
//        for (auto it = keys.begin(); it != keys.end(); ++it)
//        keys[drop.key] = keep.inx; //repoint old key to merged data; don't physically remove from list, to avoid messy reindexing
//        pop_back(); //CAUTION: assumes dropped entry is at end
        return true;
    }
#endif // 0
#if 0
    void old_merge(PalentInfo& keep, PalentInfo& drop)
    {
//#pragma message WARN("TODO: weighted avg/blend 2 colors")
        keep.merge(drop); //combine info
        for (auto it = keys.begin(); it != keys.end(); ++it)
            if ((*this)[it->second].inx == drop.inx) it->second = keep.inx; //repoint dropped entry to merged entry
//        for (auto it = sorted.c.rbegin(); it != sorted.c.rend(); ++it) //reverse iterate; normally will delete least frequent entry first
//        for (auto it = sorted./*c.*/begin(); it != sorted./*c.*/end(); ++it) //reverse iterate; normally will delete least frequent entry first
//            if (*it == &drop) { sorted./*c.*/erase(it); break; } //remove dropped entry from list
//        if (sorted.size() == size())
        for (auto it = begin(); it != end(); ++it)
            if (it->inx > drop.inx) --it->inx; //adjust higher indexes to fill in gap
        for (auto it = begin(); it != end(); ++it)
            if (it->inx == drop.inx) { erase(it); break; }
    }
#endif
//    PalentInfo& at(int which) const { return this->[values[which]]; }
//    PalentInfo& operator[](int which) const { return *(PalentInfo*)(const PalentInfo*)&std::hash_map<node_value, PalentInfo>::at(values[which]); } //operator[](val /*values[which]*/); }
//sort by color for finding closest colors:
//HSV is more appropriate for perceived colors, so use that model to find closest matches
//try to preserve hue as much as possible
#if 0
    std::vector<std::pair<int, ColorDiff>> by_cdiff; //difference between each color pair
    static bool CdiffSorter(const /*PalentInfo*/ std::pair<int, ColorDiff>& lhs, const /*PalentInfo*/ std::pair<int, ColorDiff>& rhs) { return lhs.first /*freq*/ < rhs.first /*freq*/; } //increasing order; used by sort()
//#define keys32(primary, secondary)  (((primary) << 16) | ((secondary) & 0xFFFF))
    void ByColorDiff(bool dumb)
    {
#define HSV2Value(h, s, v)  ((MIN(h, 360) << 20) | (MIN(s, 360) << 10) | MIN(v, 360)) //leave enough room for full hue range, but still fit in 32 bits
#define HSV2H(hsv)  (((hsv) >> 20) & 0x3ff)
#define HSV2S(hsv)  (((hsv) >> 10) & 0x3ff)
#define HSV2V(hsv)  ((hsv) & 0x3ff)
        std::vector<std::pair<int, int>> by_row[dumb? 8: 2]; //use 3 "rows" for smart nodes: hue, saturation, and value (brightness)
        for (size_t i = 0; i < numents(by_row); ++i)
            by_row[i].reserve(this->size()); //pre-allocate to reduce overhead; actual list sizes will be smaller for dumb nodes
//        std::vector<std::pair<int, int>> by_color_element[6]; //by color, all color orders
//generate lists of colors:
//HSV is more relevant to color perception, so use that model for finding "closest" colors
//device response varies; gamma correction is ignored here for simplicity
//background info: http://en.wikipedia.org/wiki/HSL_and_HSV
//        int numdel = 0;
        for (auto it = this->begin(); it != this->end(); ++it) //sort full palette list; assumes nothing deleted yet
        {
            if (it->IsDeleted()) { /*++numdel*/; continue; }
            if (dumb)
            {
                int brightness = RGB2R(it->key), row = RGB2B(it->key);
                by_row[row].emplace_back(HSV2Value(brightness, row, 0), it - this->begin()); //separate by row, link back to original list
                debug(30, "CDiff-dumb1: pal[%d/%d] 0x%.6x => br %d, row %d", it - this->begin(), this->size(), brightness, row);
                continue;
            }
            byte r = RGB2R(it->key), g = RGB2G(it->key), b = RGB2B(it->key);
            int hue = 0, sat = 0, val = MAX(MAX(r, g), b); //set brightness to max color element (grayscale); NOTE: don't use "byte" here; arithmetic needs extra bits!
            if ((r != g) || (r != b)) //generate hue + saturation; does not apply to gray-scale/monochrome
            {
                int chroma = val - MIN(MIN(r, g), b);
                if (g == val) hue = 120 + 60 * (b - r) / chroma; //green is dominant
                else if (r == val) { hue = 0 + 60 * (g - b) / chroma; if (hue < 0) hue += 360; } //red dominant
                else hue = 240 + 60 * (r - g) / chroma; //blue dominant
                sat = 255 * chroma / val;
//                sat = chroma / hsv; //saturation
//                hsv += 0x10000 * hue; //make hue most significant when sorting colors
            }
#if 0
            hue = 4 * hue / 3; //give a little more weight to hue differences
            val = 2 * val / 3; //give less weight to brightness differences; OTOH, want similar high brightness to be clustered near the top end of range
#endif
//            int i = it - this->begin() - numdel; //destination index
//            by_row[0][i].first = RGB2Value(hue, sat, val);
//            by_row[1][i].first = RGB2Value(sat, hue, val);
//            by_row[2][i].first = RGB2Value(val, hue, sat);
//            by_row[0][i].second = by_row[1][i].second = by_row[2][i].second = i; //set sort key and link back to original list
//            by_row[0].emplace_back(keys32(hue, (sat * sat + val * val) / 2), it - this->begin()); //sort by secondary property variations within primary property
//            by_row[1].emplace_back(keys32(sat, (hue * hue + val * val) / 2), it - this->begin()); //don't bother with sqrt (needless overhead for compares)
//            by_row[2].emplace_back(keys32(val, (hue * hue + sat * sat) / 2), it - this->begin());
//sat seems to be least useful, so put it last:
            by_row[0].emplace_back(HSV2Value(hue, val, sat), it - this->begin()); //sort by secondary property variations within primary property
            by_row[1].emplace_back(HSV2Value(val, hue, sat), it - this->begin()); //don't bother with sqrt (needless overhead for compares)
//            by_row[0].emplace_back(RGB2Value(hue, sat, val), it - this->begin());
//            by_row[1].emplace_back(RGB2Value(hue, val, sat), it - this->begin());
//            by_row[2].emplace_back(RGB2Value(sat, hue, val), it - this->begin());
//            by_row[3].emplace_back(RGB2Value(sat, sat, hue), it - this->begin());
//            by_row[4].emplace_back(RGB2Value(val, hue, sat), it - this->begin());
//            by_row[5].emplace_back(RGB2Value(val, sat, hue), it - this->begin());
            debug(30, "CDiff-hsv1: pal[%d/%d] 0x%.6x => r %d, g %d, b %d => h %d, s %d, v %d", it - this->begin(), this->size(), it->key, r, g, b, hue, sat, val);
        }
//sort by perceived color and preserve array indexes:
//this separates the list of colors by hue, sat, and value, allowing color closeness to be checked more easily
//sort in decreasing order so most significant pixels are first
        int num_pairs = 0;
        for (size_t i = 0; i < numents(by_row); ++i)
        {
//            if (!dumb) by_row[i].resize(this->size() - numdel); //fix up pre-allocated size
            sort(by_row[i].begin(), by_row[i].end(), Sorter);
            num_pairs += by_row[i].end() - by_row[i].begin() - 1;
        }
//generate differences:
//put hue, sat, val diffs into one list, but weight them so hue differences are more significant than brightness differences
//dumb (chplex) nodes can also be compared, but they can't cross row boundaries
        by_cdiff.clear();
        by_cdiff.reserve(num_pairs); //pre-allocate to reduce overhead; dumb/chplex is variable size (nodes cannot cross row boundaries), so this is a worst-case size
        debug(10, "#lists: %d, #pairs: %d", numents(by_row), num_pairs);
        for (size_t row = 0; row < numents(by_row); ++row)
            for (auto it = by_row[row].begin(); it + 1 != by_row[row].end(); ++it) //generate difference pairs
            {
//                int i = it - by_row[row].begin();
//                if (dumb)
//                {
//                    if (RGB2B(by_val[i].first) != RGB2B(by_val[i + 1].first)) continue; //can't change row#, so don't even consider this option
//                    byte diff1 = ABS(RGB2R(it[0].first) - RGB2R(it[1].first)), diff2 = ABS((it[0].first & 0xFFFF) - (it[1].first & 0xFFFF)); //, diff3 = ABS(RGB2B(it[0].first) - RGB2B(it[1].first));
//                    byte diff1 = ABS(RGB2R(it[0].first) - RGB2R(it[1].first)), diff2 = ABS(RGB2G(it[0].first) - RGB2G(it[1].first));
                    int diff1 = ABS(HSV2H(it[0].first) - HSV2H(it[1].first)), diff2 = ABS(HSV2S(it[0].first) - HSV2S(it[1].first)), diff3 = ABS(HSV2V(it[0].first) - HSV2V(it[1].first));
//                    by_cdiff[i].first = RGB2Value(rdiff, gdiff, bdiff);
//                    by_cdiff[i].second.type = ColorDiff::DiffType::Val;
//                    by_cdiff[i].second.first = i;
//                    by_cdiff[i].second.second = i + 1;
//for the same value of primary key, sort entries by the largest variation in secondary key:
//this will put "close" entries adjacent in the sorted list
//keys were already weighted, so we don't need to distinguish between secondary keys, just how significantly they differ within primary key
                    by_cdiff.emplace_back(MAX(MAX(diff1, diff2), diff3), ColorDiff(it[0].second, it[1].second, row)); //ColorDiff::DiffType::Val);
                    debug(30, "CDiff2: row %d [%d/%d] 0x%x @%d vs. next 0x%.6x @%d => diff %d,%d,%d", row, it - by_row[row].begin(), by_row[row].size(), it[0].first, it[0].second, it[1].first, it[1].second, diff1, diff2, diff3);
//                    continue;
//                }
//            byte hdiff = (by_hue[i].first - by_hue[i + 1].first) * 4 / 3; //give a little more weight to hue differences
//            byte sdiff = by_sat[i].first - by_sat[i + 1].first;
//            byte vdiff = (by_val[i].first - by_val[i + 1].first) * 2 / 3; //give less weight to brightness differences
//            by_cdiff[3 * i + 0].first = RGB2Value(hdiff , sdiff, vdiff);
//            by_cdiff[3 * i + 1].first = RGB2Value(sdiff, hdiff, vdiff);
//            by_cdiff[3 * i + 2].first = RGB2Value(vdiff, hdiff, sdiff);
//            by_cdiff[3 * i + 0].second.type = ColorDiff::DiffType::Hue;
//            by_cdiff[3 * i + 1].second.type = ColorDiff::DiffType::Sat;
//            by_cdiff[3 * i + 2].second.type = ColorDiff::DiffType::Val;
//            by_cdiff[3 * i + 0].second.first = i;
//            by_cdiff[3 * i + 1].second.first = i;
//            by_cdiff[3 * i + 2].second.first = i;
//            by_cdiff[3 * i + 0].second.second = i + 1;
//            by_cdiff[3 * i + 1].second.second = i + 1;
//            by_cdiff[3 * i + 2].second.second = i + 1;
//            debug(30, "ByColorDiff-hsv2: sorted val[%d/%d] 0x%.6x vs. 0x%.6x => hsv diff 0x%.6x", i, by_val.size() - 1, by_hue[i].first, by_hue[i + 1].first, by_cdiff[3 * i + 0].first);
//            debug(30, "ByColorDiff-hsv2: sorted val[%d/%d] 0x%.6x vs. 0x%.6x => shv diff 0x%.6x", i, by_val.size() - 1, by_sat[i].first, by_sat[i + 1].first, by_cdiff[3 * i + 1].first);
//            debug(30, "ByColorDiff-hsv2: sorted val[%d/%d] 0x%.6x vs. 0x%.6x => vhs diff 0x%.6x", i, by_val.size() - 1, by_val[i].first, by_val[i + 1].first, by_cdiff[3 * i + 2].first);
        }
//sort to get list of differences in increasing order of magnitude:
//multiple axes (hue, sat, val) are mixed together but weighted, in order to try to make it easier to choose the best axis for color reduction
        sort(by_cdiff.begin(), by_cdiff.end(), CdiffSorter); //increasing order
    }
#endif // 0
//    static bool BySecondInt(const /*PalentInfo*/ std::pair<int, int>& lhs, const /*PalentInfo*/ std::pair<int, int>& rhs) { return lhs.second /*freq*/ > rhs.second /*freq*/; } //used by sort()
//    static bool SortByValue(const /*PalentInfo*/ std::pair<int, int>& lhs, const /*PalentInfo*/ std::pair<int, int>& rhs) { return lhs.key > rhs.key; } //used by sort()
//    static bool SortByLen3D(const PalentInfo& lhs, const PalentInfo& rhs) { return lhs.len3d > rhs.len3d; } //used by sort()
//    std::vector<std::pair<int, int>> sorted; //_byfreq, sorted_byvalue; //(sort key, list inx) pairs
    static bool AscSorter(const /*PalentInfo*/ std::pair<int, int>& lhs, const /*PalentInfo*/ std::pair<int, int>& rhs) { return lhs.first /*freq*/ < rhs.first /*freq*/; } //used by sort()
    static bool DescSorter(const /*PalentInfo*/ std::pair<int, int>& lhs, const /*PalentInfo*/ std::pair<int, int>& rhs) { return lhs.first /*freq*/ > rhs.first /*freq*/; } //used by sort()
#if 0
    /*PalentInfo* */ void ByFreq(void) //int which)
    {
        int numdel = 0;
        sorted.resize(this->size());
        for (auto it = this->begin(); it != this->end(); ++it) //sort full palette list
//            if (!it->IsDeleted()) sorted.emplace_back(it->freq, it - this->begin());
        {
            if (it->IsDeleted()) { ++numdel; continue; }
//            sorted.emplace_back(it->freq, it - this->begin());
            int i = it - this->begin() - numdel;
            sorted[i].first = it->freq;
            sorted[i].second = i + numdel; //actual index
        }
        if (numdel) sorted.resize(this->size() - numdel);
//            sorted[it - this->begin()] = std::pair<int, int>(it->freq, it - this->begin());
        /*std::*/sort(sorted.begin(), sorted.end(), Sorter); //preserve array indexes and just sort tags
//        for (auto it = sorted.begin(); it != sorted.end(); ++it) //renumber palette entries with new indexes
//            (*this)[it->second].inx = it - sorted.begin();
        dump("by freq");
//??        if (byfreq_sorted_size /* != */ < this->size()) //sort_order != SortedByFreq)
//        {
//            std::priority_queue<int> sorted;
//            debug(30, "by freq sorted %d ents", sorted_byfreq.size());
//            sorted.empty();
//            /*std::*/sort(begin(), end(), SortByFreq);
//            byfreq_sorted_size = this->size();
//            for (auto it = begin(); it != end(); ++it)
//                sorted.push(&*it); //(it->freq << 8) | it->inx); //set key to freq but tag by index to allow access to PalentInfo later
//            while (sorted.size()) //enum in decreasing frequency order
//            {
//                int inx = sorted.top() & 0xFF;
//    //            debug(5, "sorted pal[%d]: rgb 0x%.6x, count %d\n", sorted.size(), values[inx], this->[values[inx]].freq);
//                /*operator[](inx)*/ (*this)[keys[inx]].inx = this->size() - sorted.size(); //assign new index in decreasing frequency order
//                sorted.pop();
//            }
//            for (auto it = this->begin(); it != this->end(); ++it)
//                keys[it->second.inx] = it->second.key; //move keys to correct order for sequential access
//            sort_order = SortedByFreq;
//        }
//        return (*this)[keys[which]]; //operator[](which);
//        return &this->at(which); ///*.c*/[which];
    }
#endif // 0
    void ByBrightness(void)
    {
//        sorted.clear(); //resize(this->size());
        size_t numdel = 0;
        std::vector<std::pair<node_value, size_t>> sorted(size(true)); //_byfreq, sorted_byvalue; //(sort key, list inx) pairs
//        std::vector<std::pair<int, int>> other_info(this->size(true));
//        sorted.resize(this->size());
        for (auto it = begin(); it != end(); ++it) //sort full palette list
//        if (nodesize == 1) rgb = rgb2mono(rgb, chplex? n / 7 + 1: 0);
//            if (!it->IsDeleted()) sorted.emplace_back(it->key, it - this->begin());
//            sorted[it - this->begin()] = std::pair<int, int>(it->freq, it - this->begin());
        {
//            PalentInfo* ptr = &(*this)[Reord(it->inx)]; //redirect to sorted/merged entry
            int inx = (it->IsDeleted() /*|| !it->key*/)? size(true) - ++numdel: it - begin() - numdel; //exclude deleted ents by moving them to end; don't need to send off channels
//            sorted.emplace_back(it->freq, it - this->begin());
            sorted[inx].first = it->key;
            sorted[inx].second = it - begin(); //ptr - &(*this)[0]; //inx + numdel; //where it came from (actual array index)
//preserve additional info (mainly for debug):
//            other_info[inx].first = it->maxnode;
//            other_info[inx].second = it->freq;
        }
//        if (numdel) sorted.resize(this->size() - numdel);
        /*std::*/sort(sorted.begin(), sorted.end() - numdel, DescSorter); //preserve array indexes and just sort tags
//        for (auto it = sorted.begin(); it != sorted.end(); ++it) //renumber palette entries with new indexes
//            (*this)[it->second].inx = it - sorted.begin();
        for (auto it = sorted.begin(); it != sorted.end(); ++it)
        {
            size_t inx = it - sorted.begin(); //destination slot
//            (*this)[inx].key = it->first;
//            (*this)[it->second].inx = inx; //map original index to new index
//            (*this)[inx].maxnode = (inx < size(true) - numdel)? other_info[it->second].first: -1;
//            (*this)[inx].freq = (inx < size(true) - numdel)? other_info[it->second].second: -1; //kludge: preserve deleted indicator; don't need to preserve actual freq
            (*this)[inx].inx = inx;
        }
//        keys.clear(); //not longer matches, so clear it
        dump("by brightness");
    }
//    /*PalentInfo* */ void xByValue(void) //int which)
//    {
//        sorted.resize(this->size());
//        for (auto it = sorted.begin(); it != sorted.end(); ++it) //re-sort partial, previously sorted list
//            it->first = (*this)[it->second].key; //just update key, keep indexes
//        /*std::*/sort(sorted.begin(), sorted.end(), Sorter); //preserve array indexes and just sort tags
//        pal_dump("by value");

//??        if (byval_sorted_size /* != */ < this->size()) //sort_order != SortedByFreq)
//        {
//            std::priority_queue<int> sorted;
//            debug(30, "by value sorted %d vs %d cached ents", byval_sorted_size, this->size());
//            sorted.empty();
//            /*std::*/sort(begin(), end(), SortByValue);
//            byval_sorted_size = this->size();
//            for (auto it = begin(); it != end(); ++it)
//                sorted.push(&*it); //(it->freq << 8) | it->inx); //set key to freq but tag by index to allow access to PalentInfo later
//            while (sorted.size()) //enum in decreasing frequency order
//            {
//                int inx = sorted.top() & 0xFF;
//    //            debug(5, "sorted pal[%d]: rgb 0x%.6x, count %d\n", sorted.size(), values[inx], this->[values[inx]].freq);
//                /*operator[](inx)*/ (*this)[keys[inx]].inx = this->size() - sorted.size(); //assign new index in decreasing frequency order
//                sorted.pop();
//            }
//            for (auto it = this->begin(); it != this->end(); ++it)
//                keys[it->second.inx] = it->second.key; //move keys to correct order for sequential access
//            sort_order = SortedByFreq;
//        }
//        return (*this)[keys[which]]; //operator[](which);
//        return &this->at(which); ///*.c*/[which];
//    }
#if 0
    PalentInfo* ByLen3D(int which)
    {
        if (bylen3d_sorted_size != this->size()) //sort_order != SortedByFreq)
        {
//            std::priority_queue<int> sorted;
            debug(30, "by len3d sorted %d vs %d ents", bylen3d_sorted_size, this->size());
//            sorted.empty();
            /*std::*/sort(begin(), end(), SortByLen3D);
            bylen3d_sorted_size = this->size();
//            for (auto it = begin(); it != end(); ++it)
//                sorted.push(&*it); //(it->freq << 8) | it->inx); //set key to freq but tag by index to allow access to PalentInfo later
//            while (sorted.size()) //enum in decreasing frequency order
//            {
//                int inx = sorted.top() & 0xFF;
//    //            debug(5, "sorted pal[%d]: rgb 0x%.6x, count %d\n", sorted.size(), values[inx], this->[values[inx]].freq);
//                /*operator[](inx)*/ (*this)[keys[inx]].inx = this->size() - sorted.size(); //assign new index in decreasing frequency order
//                sorted.pop();
//            }
//            for (auto it = this->begin(); it != this->end(); ++it)
//                keys[it->second.inx] = it->second.key; //move keys to correct order for sequential access
//            sort_order = SortedByFreq;
        }
//        return (*this)[keys[which]]; //operator[](which);
        return &this->at(which); ///*.c*/[which];
    }
    std::pair<int, int>& ClosestColors(void) //find closest 2 colors; NOTE: this is expensive
    {
        static std::pair<int, int> closest; //index of entries that are closest
        if (size() <= 2) { closest.first = closest.second = 0; if (size()) ++closest.second; return closest; } //no/one pair found
        closest.first = closest.second = 0; return closest; //TODO
#if 0
//first analyze color distribution:
        std::hash_map<int, int> reds, greens, blues;
        for (auto palent = this->begin(); palent != this->end(); ++palent) //count unique colors
        {
            ++reds[RGB2R(palent->key)];
            ++greens[RGB2G(palent->key)];
            ++blues[RGB2B(palent->key)];
        }
//choose axis with fewest variations:
        if ((reds.size() <= greens.size()) && (reds.size() <= blues.size())) //
        {
            std::hash_map<int, std::vector<int>> byred;
            for (palent it = this->begin(); it != this->end(); ++it) //partition by red
                byred[RGB2R(palent->key)].push((it->key << 8) | it->inx);
            for (auto redptr = byred.begin(); redptr != byred.end(); ++redptr)
            {
                std::hash_map<int, int> redgreens, redblues;
                for (auto green_blue = red->second.begin(); green_blue != red->second.end(); ++green_blue) //count unique colors
                {
                    ++redgreens[RGB2G(palent->key)];
                    ++redblues[RGB2B(palent->key)];
                }
            }
        }
#endif
    }
#endif
#if 0
    std::pair<int, int>& ClosestPair(void) //top level wrapper
    {
        MyQueue<int, less<int>> colors;
        for (auto it = this->begin(); it != this->end(); ++it) //build sorted list of colors
            colors.push((it->key << 8) | it->inx);
        std::pair<int, int>& closest = ClosestPair(colors.c._M_impl._M_start, size());
        if (closest.first != closest.second) //map sorted array ents back to palette order index, put most freq first
        {
            if ((*this)[closest.first & 0xFF].freq < (*this)[closest.second & 0xFF].freq) swap_notemps(closest.first, closest.second);
            closest.first = (*this)[closest.first & 0xFF].inx;
            closest.second = (*this)[closest.second & 0xFF].inx;
        }
        return closest;
    }
    std::pair<int, int>& ClosestPair(const int* ents, int numents) //find closest 2 points (recursive); NOTE: this is expensive
    {
        static std::pair<int, int> closest; //keep first, merge second
        if (numents <= 2) { closest.first = closest.second = 0; if (numents) ++closest.second; return closest; } //no/one pair found
        int mid = numents / 2;
        while (RGB2R(ents[mid] >> 8) == RGB2R(ents[mid + 1] >> 8)) ++mid;
        std::pair<int, int> lower = ClosestPair(ents, mid), upper = ClosestPair(ents + mid, numents - mid);
        return closest;
    }
#endif
#if 0
    PalentInfo& ByColor(int which)
    {
        if (sort_order != SortedByColor)
        {
            std::priority_queue<int> sorted;
            for (auto it = this->begin(); it != this->end(); ++it)
                sorted.push((it->second.diff(BLACK) << 8) | it->second.inx); //set key to valuefreq but tag by index to allow access to PalentInfo later
            while (sorted.size()) //enum in decreasing frequency order
            {
                int inx = sorted.top() & 0xFF;
    //            debug(5, "sorted pal[%d]: rgb 0x%.6x, count %d\n", sorted.size(), values[inx], this->[values[inx]].freq);
                operator[](inx).inx = this->size() - sorted.size(); //assign new index in decreasing frequency order
                sorted.pop();
            }
            for (auto it = this->begin(); it != this->end(); ++it)
                values[it->second.inx] = it->second.key; //move keys to correct order for sequential access
            sort_order = SortedByColor;
        }
        return operator[](which);
    }
#endif
    size_t ByColorDiff(size_t maxents, RenXt_Stats::PaletteTypes paltype)
    {
//        sorted.clear(); //resize(this->size());
        bool mono = (paltype == RenXt_Stats::PaletteTypes::Mono), parapal = (paltype == RenXt_Stats::PaletteTypes::Parallel);
        debug(10, "sort by color %d ents vs %d, mono? %d, parapal? %d", size(true), end() - begin(), mono, parapal);
        if (parapal) return 0; //TODO
        std::vector<std::pair<uint32_t, uint32_t>> by_hsv(size(true)); //[3]; //_byfreq, sorted_byvalue; //(sort key, list inx) pairs
        //for (int i = mono? 2: 0; i < 3; ++i)
//        by_hsv.resize(this->size());
//        std::vector<std::pair<int, int>> other_info(this->size());
//        sorted.resize(this->size());
//        debug(10, "hsv len %d", by_hsv.size());
        size_t numdel = 0;
        checkpoint(__LINE__, size(true));
        for (auto it = begin(); it != end(); ++it) //sort full palette list
//        if (nodesize == 1) rgb = rgb2mono(rgb, chplex? n / 7 + 1: 0);
//            if (!it->IsDeleted()) sorted.emplace_back(it->key, it - this->begin());
//            sorted[it - this->begin()] = std::pair<int, int>(it->freq, it - this->begin());
        {
//            PalentInfo* ptr = &(*this)[Reord(it->inx)]; //redirect to sorted/merged entry
            size_t inx = it->IsDeleted()? size(true) - ++numdel: it - begin() - numdel; //exclude deleted entries from sort by moving them to end
//preserve additional info (mainly for debug):
//            other_info[inx].first = it->maxnode;
//            other_info[inx].second = it->freq;
//            debug(10, "wr inx %d", inx);
//            sorted.emplace_back(it->freq, it - this->begin());
            by_hsv[inx].second = it - begin(); //ptr - &(*this)[0]; //inx + numdel; //where it came from (actual array index)
//            if (mono) { by_hsv[2][inx].first = it->key; continue; } //phase angle dimming only uses brightness
//http://en.wikipedia.org/wiki/HSL_and_HSV
//also http://stackoverflow.com/questions/1678457/best-algorithm-for-matching-colours/1678498#1678498
            byte r = RGB2R(it->key), g = RGB2G(it->key), b = RGB2B(it->key);
            if (mono) { by_hsv[inx].first = 2 * r * r +  g * g + b * b; continue; } //phase angle dimming only uses brightness, but include row# factor to enforce row separation
            byte maxbr = std::max<byte>(r, std::max<byte>(g, b));
//TODO: move this into RGB class
            byte deltabr = maxbr - std::min<byte>(r, std::min<byte>(g, b));
            byte val = maxbr; //NOTE: this should be /255 to make value 0..1, but it's relative so just leave as int for better perf
            byte sat = maxbr? 255 * deltabr / maxbr: 0; //should also be 0..1, but convert to 0..255 as int; NOTE: /255 cancels out between numerator and denominator here anyway
            uint16_t hue = (maxbr && deltabr)? ((maxbr == r)? (360 + 60 * (g - b) / deltabr) % 360: (maxbr == g)? 120 + 60 * (b - r) / deltabr: 240 + 60 * (r - g) / deltabr): 0; //0..360
//            it->hsv = HSV2Value(hue, sat, val); //save HSV values in case entries need to be merged; NOTE: assumes sat <= 255, val <= 255, hue can be larger
            it->hsv.h = hue;
            it->hsv.s = sat;
            it->hsv.v = val;
//??            hue /= 2; sat /= 2; //suggested weights
//            by_hsv[2][inx].first = (val << 20) | (hue << 10) | sat; //brightness value = primary sort key
//            if (!maxbr) { by_hsv[0][inx].first = 0; by_hsv[1][inx].first = 0; continue; }
//            by_hsv[1][inx].first = sat
//            by_hsv[0][inx].first = hue
//            by_hsv[1][inx].first = (sat << 20) | (hue << 10) | val; //saturation = primary sort key
//            by_hsv[0][inx].first = (hue << 20) | (val << 10) | sat; //hue = primary sort key
            by_hsv[inx].first = 2 * hue * hue + sat * sat / 2 + val * val; //don't need to take sqrt since it's all relative; give hue higher weight
            debug(10, "ByColor: hsv[%d/%d] key 0x%x => hue %d, sat %d, val %d, freq %d, #del %d", inx, size(true), it->key, hue, sat, val, it->freq, numdel);
        }
//first sort by raw values:
//        if (numdel) sorted.resize(this->size() - numdel);
//        for (int i = mono? 2: 0; i < 3; ++i)
//	{
        /*std::*/sort(by_hsv.begin(), by_hsv.end() - numdel, DescSorter); //preserve array indexes and just sort tags
        debug(10, "sorted %d active - %d del items by hsv desc", by_hsv.size(), numdel);
        checkpoint(__LINE__, numdel);
//	}
//calculate relative differences:
//TODO: apply different weights to hue/sat/brightness?
//TODO: weight more frequent entries heavier?
        size_t numclose = 0;
//        size_t numents = size(true) - numdel - 1; //there will be one less entry after taking diffs
//        for (int i = mono? 2: 0; i < 3; ++i)
        for (auto it = by_hsv.begin(); it != by_hsv.end() - numdel - 1; ++it)
        {
            uint32_t svfirst = it->first, svsecond = it->second;
            it->first -= (it + 1)->first; //diff with next smaller entry
            if (it->first < 10) ++numclose; //TODO: go ahead and merge these entries now
            if ((*this)[it->second].freq > (*this)[(it + 1)->second].freq) //remember both indices, but favor most frequent entry and delete the other one; NOTE: assumes < 64K entries
                it->second = it->second << 16 | (it + 1)->second;
            else
                it->second |= (it + 1)->second << 16;
            debug(10, "diff[%d/%d] %d - %d = %d, inx keep %d, freq %d, inx drop %d, freq %d", it - by_hsv.begin(), by_hsv.size() - numdel - 1, svfirst, (it + 1)->first, it->first, it->second >> 16, std::max<int>((*this)[svsecond].freq, (*this)[(it + 1)->second].freq), it->second & 0xffff, std::min<int>((*this)[svsecond].freq, (*this)[(it + 1)->second].freq));
        }
//combine into 1 list before deciding which entries to merge:
//        if (!mono)
//        {
////            by_hsv[2].resize(3 * (this->size() - numdel - 1));
//            by_hsv[2].insert(by_hsv[2].begin() + numents, by_hsv[0].begin(), by_hsv[0].end()); //- numdel);
//            by_hsv[2].insert(by_hsv[2].begin() + 2 * numents, by_hsv[1].begin(), by_hsv[1].end()); //- numdel);
//            numents *= 3;
//        }
        /*std::*/sort(by_hsv.begin(), by_hsv.end() - numdel - 1, AscSorter); //preserve array indexes and just sort tags
//now merge pairs with smallest difference:
//        for (auto it = sorted.begin(); it != sorted.end(); ++it) //renumber palette entries with new indexes
//            (*this)[it->second].inx = it - sorted.begin();
        checkpoint(__LINE__);
        size_t num_merged = 0;
        debug(10, "sort %d diff ents, %d were close already", by_hsv.size() - numdel - 1, numclose);
        for (auto it = by_hsv.begin(); it != by_hsv.end() - numdel - 1; ++it)
        {
            size_t keepinx = it->second >> 16, dropinx = it->second & 0xffff;
            debug(10, "combined list[%d/%d]: diff %d, inx keep %d, drop %d, #del %d, #merged %d, #rem %d vs. max %d, need merge? %d, already del? %d", it - by_hsv.begin(), by_hsv.size() - numdel - 1, it->first, keepinx, dropinx, numdel, num_merged, size(true) - numdel - num_merged, maxents, (size(true) - numdel - num_merged) > maxents, (*this)[keepinx].IsDeleted() || (*this)[dropinx].IsDeleted());
            if (this->size(true) - numdel - num_merged <= maxents) break;
#pragma message("TODO: iterate reduction loop rather than chaining, to pick closer matches")
            std::vector<byte> seen(size(true));
            for (;;)
            {
                if (keepinx >= size(true))
                {
                    debug(1, "bad keep inx: %d/%d", keepinx, size(true));
                    break;
                }
                assert(keepinx < size(true)); //debug(1, "BAD %d/%d", keepinx, size(true));
                if (!(*this)[keepinx].IsDeleted()) break;
                seen[keepinx] = true;
//TODO                ++stats
                int svkeepinx = keepinx;
                keepinx = (*this)[keepinx].inx;
                debug(10, "keep inx %d was already merged/deleted, use entry %d instead", svkeepinx, keepinx);
                if (seen[keepinx])
                {
                    error("inf keep chain detected");
                    break;
                }
            }
//            assert(!(*this)[keepinx].IsDeleted());
            if ((*this)[keepinx].IsDeleted()) { debug(1, "BAD keep inx %d", keepinx); continue; }
//            if ((*this)[keepinx].IsDeleted() || (*this)[dropinx].IsDeleted()) continue;
            for (;;)
            {
                if (dropinx >= size(true))
                {
                    debug(1, "bad drop inx: %d/%d", dropinx, size(true));
                    break;
                }
                assert(dropinx < size(true)); //debug(1, "BAD %d/%d", dropinx, size(true));
//TODO                ++stats
                if (!(*this)[dropinx].IsDeleted()) break;
                seen[dropinx] = true;
                int svdropinx = dropinx;
                dropinx = (*this)[dropinx].inx;
                debug(10, "drop inx %d was already merged/deleted, use entry %d instead", svdropinx, dropinx);
                if (seen[dropinx])
                {
                    error("inf drop chain detected");
                    break;
                }
            }
//            assert(!(*this)[dropinx].IsDeleted());
            if ((*this)[dropinx].IsDeleted()) { debug(1, "BAD drop inx %d", dropinx); continue; }
//            byte keep_r = RGB2R((*this)[keepinx].key), keep_g = RGB2G((*this)[keepinx].key), keep_b = RGB2B((*this)[keepinx].key);
//            byte drop_r = RGB2R((*this)[dropinx].key), drop_g = RGB2G((*this)[dropinx].key), drop_b = RGB2B((*this)[dropinx].key);
//            if (mono) if ((keep_g != drop_g) || (keep_b != drop_b)) continue; //can't merge different rows
//            byte blend_r = (keep_r * (*this)[keepinx].freq + drop_r * (*this)[dropinx].freq) / ((*this)[keepinx].freq + (*this)[dropinx].freq);
//            byte blend_g = (keep_g * (*this)[keepinx].freq + drop_g * (*this)[dropinx].freq) / ((*this)[keepinx].freq + (*this)[dropinx].freq);
//            byte blend_b = (keep_b * (*this)[keepinx].freq + drop_b * (*this)[dropinx].freq) / ((*this)[keepinx].freq + (*this)[dropinx].freq);
            node_value blended_rgb;
            PalentInfo::HSV blended_hsv;
            if (mono)
            {
                checkpoint(__LINE__);
                byte keep_br = RGB2R((*this)[keepinx].key), keep_row = RGB2G((*this)[keepinx].key), keep_notrow = RGB2B((*this)[keepinx].key);
                byte drop_br = RGB2R((*this)[dropinx].key), drop_row = RGB2G((*this)[dropinx].key), drop_notrow = RGB2B((*this)[dropinx].key);
                if ((keep_row != drop_row) || (keep_notrow != drop_notrow)) continue; //can't merge across rows
                byte blend_br = (keep_br * (*this)[keepinx].freq + drop_br * (*this)[dropinx].freq) / ((*this)[keepinx].freq + (*this)[dropinx].freq);
                blended_rgb = RGB2Value(blend_br, keep_row, keep_notrow);
//                debug(20, "mono blend: %d %d %d", blend_br, keep_row, keep_notrow);
            }
            else
            {
                checkpoint(__LINE__);
//                uint16 keep_h = HSV2H((*this)[keepinx].hsv), keep_s = HSV2S((*this)[keepinx].hsv), keep_v = HSV2V((*this)[keepinx].hsv);
//                uint16 drop_h = HSV2H((*this)[dropinx].hsv), drop_s = HSV2S((*this)[dropinx].hsv), drop_v = HSV2V((*this)[dropinx].hsv);
                if ((*this)[keepinx].freq + (*this)[dropinx].freq < 1) debug(1, "BAD FREQ: [%d] %d + [%d] %d", keepinx, (*this)[keepinx].freq, dropinx, (*this)[dropinx].freq);
                uint16_t keep_h = (*this)[keepinx].hsv.h, keep_s = (*this)[keepinx].hsv.s, keep_v = (*this)[keepinx].hsv.v;
                uint16_t drop_h = (*this)[dropinx].hsv.h, drop_s = (*this)[dropinx].hsv.s, drop_v = (*this)[dropinx].hsv.v;
                uint16_t blend_h = (keep_h * (*this)[keepinx].freq + drop_h * (*this)[dropinx].freq) / ((*this)[keepinx].freq + (*this)[dropinx].freq);
//                debug(10, "keep h s v %d, %d, %d, drop h s v %d, %d, %d", keep_h, keep_s, keep_v, drop_h, drop_s, drop_v);
                byte blend_s = (keep_s * (*this)[keepinx].freq + drop_s * (*this)[dropinx].freq) / ((*this)[keepinx].freq + (*this)[dropinx].freq);
                byte blend_v = (keep_v * (*this)[keepinx].freq + drop_v * (*this)[dropinx].freq) / ((*this)[keepinx].freq + (*this)[dropinx].freq);
                blended_hsv.h = blend_h; //HSV2Value(blend_h, blend_s, blend_v);
                blended_hsv.s = blend_s;
                blended_hsv.v = blend_v;
                debug(20, "hsv blend: %d %d %d", blended_hsv.h, blended_hsv.s, blended_hsv.v);
                checkpoint(__LINE__);
//TODO: move this into RGB class
                if (!blend_s) blended_rgb = RGB2Value(blend_v, blend_v, blend_v); //grayscale
                else
                {
//                    h /= 60; // sector 0 to 5
//                    i = floor( h );
//                    f = h - i; // fractional part of h
                    int part = blend_v * blend_s / 255; //TODO: give this a better name
                    byte p = blend_v - part; //v * ( 1 - s );
                    byte q = blend_v - part * (blend_h % 60) / 60; //v * ( 1 - s * f );
                    byte t = blend_v - part + part * (blend_h / 60); //v * ( 1 - s * ( 1 - f ) );
                    switch (blend_h /60) //special cases by color sector
                    {
                        case 1: blended_rgb = RGB2Value(q, blend_v, p); break;
                        case 2: blended_rgb = RGB2Value(p, blend_v, t); break;
                        case 3: blended_rgb = RGB2Value(p, q, blend_v); break;
                        case 4: blended_rgb = RGB2Value(t, p, blend_v); break;
                        case 5: blended_rgb = RGB2Value(blend_v, p, q); break;
                        default:
                        case 0: blended_rgb = RGB2Value(blend_v, t, p); break;
                    }
                }
//                debug(20, "hsv -> rgb blend: 0x%x", blended_rgb);
                checkpoint(__LINE__);
            }
//            int svkeepfreq = (*this)[keepinx].freq, svdropfreq = (*this)[dropinx].freq;
            if (!(*this)[keepinx].merge((*this)[dropinx])) { debug(1, "DELETE %d FAILED", dropinx); continue; }
//            debug(10, "merge: keep [%d] rgb 0x%x / hsv %d, %d, %d, freq %d, del? %d, drop [%d] rgb 0x%x / hsv %d, %d, %d, freq %d, del? %d => merged rgb 0x%x / hsv %d, %d, %d, freq %d", keepinx, (*this)[keepinx].key, (*this)[keepinx].hsv.h, (*this)[keepinx].hsv.s, (*this)[keepinx].hsv.v, svkeepfreq, (*this)[keepinx].IsDeleted(), dropinx, (*this)[dropinx].key, (*this)[dropinx].hsv.h, (*this)[dropinx].hsv.s, (*this)[dropinx].hsv.v, svdropfreq, (*this)[dropinx].IsDeleted(), blended_rgb, blended_hsv.h, blended_hsv.s, blended_hsv.v, (*this)[keepinx].freq);
            checkpoint(__LINE__);
            (*this)[keepinx].key = blended_rgb; //RGB2RGB(blend_r, blend_g, blend_b);
            (*this)[keepinx].hsv = blended_hsv;
            ++num_merged;
        }
        checkpoint(__LINE__);
//prune/reorder list:
        debug(10, "merged %d entries, reorder remaining %d = %d entries", num_merged, size(true) - numdel - num_merged, size(false));
#if 0
        struct SaveInfo
        {
            node_value key;
            PalentInfo::HSV hsv;
            int maxnode, freq;
            int inx;
        };
        std::vector</*std::pair<int, int>*/ SaveInfo> remaining(maxents); //_byfreq, sorted_byvalue; //(sort key, list inx) pairs
//        sorted.resize(this->size());
        for (auto it = this->begin(); it != this->end(); ++it) //consolidate full palette list
//        if (nodesize == 1) rgb = rgb2mono(rgb, chplex? n / 7 + 1: 0);
//            if (!it->IsDeleted()) sorted.emplace_back(it->key, it - this->begin());
//            sorted[it - this->begin()] = std::pair<int, int>(it->freq, it - this->begin());
        {
            int inx = it->IsDeleted()? this->size() - ++numdel: it - this->begin() - numdel; //deleting an entry will cause orphaned nodes, so just move deleted entries to end
//            sorted.emplace_back(it->freq, it - this->begin());
            remaining[inx].key = it->key;
            remaining[inx].inx = it - this->begin(); //inx + numdel; //new index
//preserve additional info (mainly for debug):
            remaining[inx].hsv = it->hsv;
            remaining[inx].maxnode = it->maxnode;
            remaining[inx].freq = it->freq;
        }
        debug(10, "make reordered list");
        for (auto it = remaining.begin(); it != remaining.end(); ++it)
        {
            size_t inx = it - remaining.begin();
            (*this)[inx].key = it->key;
            (*this)[it->inx].inx = inx; //map original index to new index
//            (*this)[inx].inx = it->second;
            (*this)[inx].maxnode = (inx < size() - numdel)? remaining[it->inx].maxnode: -1;
            (*this)[inx].freq = (inx < size() - numdel)? other_info[it->second].second: -1; //kludge: preserve deleted indicator; don't need to preserve actual freq
        }
#endif
#if 0
        std::vector<PalentInfo> newlist(this->size(true));
        debug(10, "make reordered list %d ents, move %d del ents to end", this->size(true), numdel);
#pragma message("TODO: fix up palette emit code and avoid palette shuffling here")
        numdel = 0;
        for (auto it = this->begin(); it != this->end(); ++it) //consolidate full palette list
        {
            int inx = it->IsDeleted()? this->size(true) - ++numdel: it - this->begin() - numdel; //deleting an entry will cause orphaned nodes, so just move deleted entries to end
            newlist[inx] = *it;
            newlist[it - this->begin()].inx = inx; //inx + numdel; //allow inverted node lists to find shuffled entries
        }
        this->swap(newlist); //this->assign(newlist.begin(), newlist.end()); //*this = newlist;
#endif
//        keys.clear(); //no longer matches, so clear it
        dump("by color diff");
        return num_merged;
    }
//reduce palette size if needed:
//palette size cannot exceed 16 for smart pixels (due to RenardXt firmware design limitation)
    bool reduce(size_t maxpal, RenXt_Stats* stats, RenXt_Stats::PaletteTypes paltype)
    {
        bool reduced = false;
//        size_t svsize = size();
        for (size_t retry = 0, numpal = size(false); (int)retry < -RenXt_palovfl; ++retry) //try to merge palette entries to redice size
        {
            debug(5, "palette merge: try# %d/%d, cur pal size: %d, max allowed %d, reduce? %d", retry + 1, -RenXt_palovfl, numpal, maxpal, numpal > maxpal);
            if (numpal <= maxpal) break; //no need to merge entries
//        std::vector<std::vector<<std::pair<int, int>>> closest_pairs; //first entry is (color order, 2nd + 3rd diff), remaining entries are pairs of indexes into palette
            int num_merged = 0, num_failed = 0;
            if (numpal > stats->maxpal[paltype]) stats->maxpal[paltype] = numpal;
            ++stats->reduce[paltype]; //[numpal - maxpal];
#if 0
        prop->palette.ByColorDiff(IsDumb(prop->desc.node_type)); //convert to HSV to make color "closeness" easier to eval
#pragma message("TODO?: parallel reduce")
        for (auto it = prop->palette.by_cdiff.begin(); (it != prop->palette.by_cdiff.end()) && (numpal > prop->desc.maxpal); ++it) //merge enough entries to trim palette size as required
        {
            if (it->second.drop == prop->palette.max_occur)
            {
                debug(20, "preserving bkg [%d] 0x%x", prop->palette.max_occur, prop->palette[prop->palette.max_occur].key);
                continue;
            }
            int svinx1 = prop->palette[it->second.keep].inx, svdel1 = prop->palette[it->second.keep].IsDeleted(), svinx2 = prop->palette[it->second.drop].inx, svdel2 = prop->palette[it->second.drop].IsDeleted();
//            if (it - prop->palette.by_cdiff.begin() > 2 * prop->palette.by_cdiff.size() / 3) break; //got far enough thru this list; regen and try again
            if (prop->palette.merge(it->second.keep, it->second.drop))
            {
                ++num_merged; //--numpal;
//                int leaf = it->second.keep, next;
//                while ((next = prop->palette[leaf].inx) != leaf) leaf = next;
//                prop->palette[it->second.drop].inx = leaf;
            }
            else ++num_failed;
            debug(60, "merge# %d[%d/%d], diff 0x%x (%d @%d->%d? %s, %d @%d->%d? %s), row %d, #pal remaining %d vs. max %d", retry, it - prop->palette.by_cdiff.begin(), prop->palette.by_cdiff.size(), it->first, it->second.keep, svinx1, prop->palette[it->second.keep].inx, /*prop->palette[it->second.keep].IsDeleted()*/ svdel1? "del": "ok", it->second.drop, svinx2, prop->palette[it->second.drop].inx, /*prop->palette[it->second.drop].IsDeleted()*/ svdel2? "del": "ok", it->second.row, numpal - num_merged, prop->desc.maxpal);
//            if (num_failed > num_merged) break;
        }
#endif // 0
#if 1
//            try {
            num_merged = ByColorDiff(maxpal, paltype);
//            } catch (void* exc)
//            {
//                debug(10, "catch color diff"); //: %s", exc.ToString());
//            }
#endif // 1
            numpal -= num_merged;
            debug(10, "#pal ents merged: %d (%d%%), failed: %d (%d%%), new size: %d vs. %d", num_merged, 100 * num_merged / (num_merged + num_failed), num_failed, 100 * num_failed / (num_merged + num_failed), numpal, maxpal);
            if (!num_merged) { debug(1, "giving up"); break; }
            else reduced = true;
        }
        return reduced;
//        size_t remaining_ents = 0; //size();
//        for (auto it = this->begin(); it != this->end(); ++it)
//            if (!it->IsDeleted())
//                if (++remaining_ents > maxpal)
//                {
//                    ++stats->reduce_failed[paltype];
//                    break;
//                }
//        if (size(false) > maxpal) ++stats->reduce_failed[paltype];
//#endif // 0
//if the fancy quantization did not reduce palette, try more primitive techniques:
//see http://en.wikipedia.org/wiki/Color_quantization
//example images at top right show how well a small palette of only 16 colors can perform

//good explanation here: http://www.imagemagick.org/Usage/quantize/
//ImageMagick notes:
//http://www.imagemagick.org/script/binary-releases.php#windows
//xLights is 32-bit, so decided to go with Q8-x86-static (static to avoid version problems)
#ifdef LEPT
#pragma message("LEPT IS ON?")
    while ((prop->palette.size() > prop->desc.maxpal) && (RenXt_palovfl > 0)) //merge less frequent entries; dummy "while" for flow control
    {
        PIX* pix;
//            l_int16 d = 32;
//            l_int32 width, height; //, xres = 0, yres = 0;
//            l_int32 pixWpl, pixBpl;
//            PIXCMAP* cmap;
//            l_uint32* line;
//            l_uint32* pword;
//            l_int32 format = IFF_BMP; //1 //i, j, k;
//            l_uint8 pel[4];
//            PIX* pixc;

//            d = 32; xres = 0; yres = 0;
//            l_int32 width = 32, height = 32;
        if ((pix = pixCreate(prop->desc.width, prop->desc.height, 32 /*d*/)) == NULL) { debug(10, "lept error"); break; }
//            format = IFF_BMP; //1
        pixSetInputFormat(pix, IFF_BMP /*format*/);
//            pixSetXRes(pix, (l_int32)((l_float32)xres / 39.37 + 0.5));  /* to ppi */
//            pixSetYRes(pix, (l_int32)((l_float32)yres / 39.37 + 0.5));  /* to ppi */
//            pixBpl = 4 * pixWpl;
//            cmap = NULL;
//            pixSetColormap(pix, cmap);
        inptr = prop->inptr;
        l_int32 src_pixWpl = pixGetWpl(pix);
        l_uint32* src_line = pixGetData(pix) + src_pixWpl * (prop->desc.height - 1);
        for (l_int32 y = 0; y < prop->desc.height; ++y, src_line -= src_pixWpl)
            for (l_int32 x = 0; x < prop->desc.width; ++x)
            {
                l_uint32* pword = src_line + x;
//                    if (fread(&pel, 1, 3, fp) != 3) readerror = 1;
//                    readpixel(y * width + j, pel);
//    COLOR_RED = 0,
//    COLOR_GREEN = 1,
//    COLOR_BLUE = 2,
//    L_ALPHA_CHANNEL = 3
//apply little endian swap here:
//leptonica probably doesn't care about color order, but go ahead and apply reordering here in case the algorithms favor green (our eyes more sensitive to it), etc
                node_value rgb = rgb_reorder(prop->desc.order, inptr); inptr += 3;
                *((l_uint8 *)pword + 3- COLOR_RED) = RGB2R(rgb); //pel[0];
                *((l_uint8 *)pword + 3- COLOR_GREEN) = RGB2G(rgb); //pel[1];
                *((l_uint8 *)pword + 3- COLOR_BLUE) = RGB2B(rgb); //pel[2];
//                    *((l_uint8 *)pword + L_ALPHA_CHANNEL) = pel[3];
//              if (extrabytes) for (k = 0; k < extrabytes; k++) ignore = fread(&pel, 1, 1, fp);
            }
//            pixEndianByteSwap(pix);
//            pixSaveTiled(pixs, pixa, 1, 1, SPACE, 32);
        PIX* pixc = NULL;
        switch (RenXt_palovfl >> 4)
        {
            case 5: //Median cut quantizer (no/dither; 5 sigbits)
            case 6: //Median cut quantizer (no/dither; 6 sigbits)
                debug(5, "quantize: MedianCutQuantGeneral(dither? %d, sig bits %d, max sub %d, chk b/w %d)", RenXt_palovfl & 1, RenXt_palovfl >> 4, 1, 1);
                pixc = pixMedianCutQuantGeneral(pix, RenXt_palovfl & 1 /*dither*/, 0 /*out depth*/, prop->desc.maxpal /*max colors*/, RenXt_palovfl >> 4 /*sig bits*/, 1 /*max sub*/, 1 /*check bw*/);
                break;
            case 2: //Median cut quantizer (mixed color/gray)
                debug(5, "quantize: MedianCutQuantMixed(#colors %d, #gray %d, dark thresh %s, light thres %d, diff thresh %d)", 20, 10, 0, 0, 0);
                pixc = pixMedianCutQuantMixed(pix, 20, 10, 0, 0, 0);
                break;
            case 8: //Simple 256 cube octcube quantizer
                debug(5, "quantize: FixedOctcubeQuant256(dither? %d)", RenXt_palovfl & 1);
                pixc = pixFixedOctcubeQuant256(pix, RenXt_palovfl & 1 /*dither */);
                break;
//            case 9: //2-pass octree quantizer
//                debug(5, "quantize: OctreeColorQuant(#colors %d, dither? %d", 16, RenXt_palovfl & 1);
//                pixc = pixOctreeColorQuant(pix, /*128*/ 16, RenXt_palovfl & 1 /*dither */);
//                break;
            case 10: //Simple adaptive quantization to 4 or 8 bpp, specifying ncolors
                debug(5, "quantize: OctreeQuantNumColors(max colors %d, sub sample %d)", prop->desc.maxpal, 0);
                pixc = pixOctreeQuantNumColors(pix, prop->desc.maxpal, RenXt_palovfl & 15);   /* fixed: 16 colors */
                break;
            case 4: //Quantize to fully populated octree (RGB) at given level
                debug(5, "quantize: FixedOctcubeQuantGenRGB(level %d)", (RenXt_palovfl & 3) + 2);
                pixc = pixFixedOctcubeQuantGenRGB(pix, (RenXt_palovfl & 3) + 2);  /* level 2...5 */
                break;
//            case 15: //Generate 32 bpp RGB image with num colors <= 256
//                debug(5, "OQNC(%d)", 16);
//                pixt = pixOctreeQuantNumColors(pixs, /*256*/ 16, 0);   /* cmapped version */
//                pix32 = pixRemoveColormap(pixt, REMOVE_CMAP_BASED_ON_SRC);
            case 3: //Quantize image with few colors at fixed octree leaf level
                debug(5, "quantize: FewColorsOctcubeQuant1(level %d)", (RenXt_palovfl & 3) + 2);
                pixc = pixFewColorsOctcubeQuant1(pix, (RenXt_palovfl & 3) + 2);   /* level 2...5 */
                break;
            case 14: //Quantize image by population
                debug(5, "quantize: OctreeQuantByPopulation(level %d, dither? %d)", (RenXt_palovfl & 1) + 3, RenXt_palovfl & 4);
                pixc = pixOctreeQuantByPopulation(pix, (RenXt_palovfl & 1) + 3, (RenXt_palovfl & 4)? 1: 0);  /* level 3..4, no /dither */
                break;
            case 1: //Mixed color/gray octree quantizer
//                pixSaveTiled(pixt, pixa, 1, 1, SPACE, 0);
                debug(5, "quantize: OctcubeQuantMixedWithGray(depth %d, gray levels %d, delta %d)", 8, 64, (RenXt_palovfl & 7) * 10);
                pixc = pixOctcubeQuantMixedWithGray(pix, /*depth*/ 8, /*gray levels*/ 64, /*delta*/ (RenXt_palovfl & 7) * 10 /*10,30,50*/);  /* max delta = 10,30,50 */
                break;
            default:
                debug(5, "unknown quantization method: 0x%x", RenXt_palovfl);
        }
//            PixSave32(pixa, pixc, rp);
//            pixEndianByteSwap(pixc);
        prop->lept_pix = pixConvertTo32(pixc); //PIX* pix32 //is this needed?
//            pixSaveTiled(pix32, pixa, 1, 0, SPACE, 0);

        prop->palette.clear(); //regenerate palette info
//            fxpix = prop->cpyptr;
        src_pixWpl = pixGetWpl(pix);
        src_line = pixGetData(pix) + src_pixWpl * (prop->desc.height - 1);
        l_int32 dest_pixWpl = pixGetWpl(prop->lept_pix);
        l_uint32* dest_line = pixGetData(prop->lept_pix) + dest_pixWpl * (prop->desc.height - 1);
        for (l_int32 y = 0; y < prop->desc.height; ++y, dest_line -= dest_pixWpl, src_line -= src_pixWpl)
            for (l_int32 x = 0; x < prop->desc.width; ++x)
            {
                l_uint32* pword = dest_line + x;
                byte color[3];
//                    if (fread(&pel, 1, 3, fp) != 3) readerror = 1;
                color[0] = *((l_uint8 *)pword + 3- COLOR_RED);
                color[1] = *((l_uint8 *)pword + 3- COLOR_GREEN);
                color[2] = *((l_uint8 *)pword + 3- COLOR_BLUE);
//                debug(90 - 20, "map: dest(%x,%d) = 0x%.6x", x, y, rgb);
//                    pel[3] = *((l_uint8 *)pword + L_ALPHA_CHANNEL);
//                    writepixel(i * width + j, pel);
                node_value dest_rgb = RGB2Value(color[0], color[1], color[2]);
#if 1
//                l_uint32* svpword = pword;
                pword = src_line + x;
                color[0] = *((l_uint8 *)pword + 3- COLOR_RED);
                color[1] = *((l_uint8 *)pword + 3- COLOR_GREEN);
                color[2] = *((l_uint8 *)pword + 3- COLOR_BLUE);
                node_value src_rgb = RGB2Value(color[0], color[1], color[2]);
                debug(80, "map node[%d] (%d,%d) 0x%.6x -> 0x%.6x", y * prop->desc.width + x, x, y, src_rgb, dest_rgb);
#endif
                prop->palette.occur(dest_rgb, y * prop->desc.width + x);
//        if (extrabytes) for (k = 0; k < extrabytes; k++) ignore = fread(&pel, 1, 1, fp);
            }
//            if (shared_palette.map.find(rgb) == shared_palette.map.end()) shared_bpp = MAXINT; //force private palette to be used
//            else shared_bpp = MAX(shared_bpp, NumBits(shared_palette.map[rgb].first));

//            PIXCMAP* cmaps = pixGetColormap(pixc); //pix32);
//            l_int32 ncolor = pixcmapGetCount(cmaps);
//            debug(10, "palette reduced to %d colors", ncolor);
//            for (int i = 0; i < ncolor; ++i)
//            {
//                l_int32 rval, gval, bval; //, byte, qbit;
//                pixcmapGetColor(cmaps, i, &rval, &gval, &bval);
//                printf("color[%d/%d] 0x%x 0x%x 0x%x\n", i, ncolor, rval, gval, bval);
//            }
        pixDestroy(&pixc);
//        pixDestroy(&reduced);
        pixDestroy(&pix);

//        prop->palette.ByFreq(0); //sort by freq (decreasing order)
//            prop->palette.ByLen3D(0); //sort by freq (decreasing order)
//        prop->palette.ByFreq(); //sort by freq if not already sorted (decreasing order)
        prop->palette.ByFreq(); //sort by freq if not already sorted (decreasing order)
        break;
    }
#endif

//        /*if (prop_palette.sort_order != prop_palette.SortedByFreq)*/ /*PalentInfo& palent =*/ prop->palette.ByFreq(0); //sort by freq (decreasing order)
//    size_t svsize = prop->palette.size(); //errmsg info only
#if 0
        while ((prop->palette.size() > 16) && (RenXt_palovfl > 0)) //merge less frequent entries
        {
            std::pair<int, int> closest = prop->palette.ClosestColors(); //indexes of 2 closest colors
            if (closest.first == closest.second) break; //no pair found
            if (prop->palette[closest.first].freq < prop->palette[closest.second].freq) swap_notemps(closest.first, closest.second); //put most freq first
            PalentInfo& palent_keep = prop->palette[closest.first], palent_drop = prop->palette[closest.second];
            node_value diff = palent_keep.diff(palent_drop.key);
            debug(5, "palette reduce: keep palent[%d] rgb 0x%.6x, freq %d, merge/drop [%d] rgb 0x%.6x, freq %d, diff %d,%d,%d below threshold %d,%d,%d", palent_keep.inx, palent_keep.key, palent_keep.freq, palent_drop.inx, palent_drop.key, palent_drop.freq, RGB2R(diff), RGB2G(diff), RGB2B(diff), RGB2R(RenXt_palovfl), RGB2G(RenXt_palovfl), RGB2B(RenXt_palovfl));
            if ((RGB2R(diff) > RGB2R(RenXt_palovfl)) || (RGB2G(diff) > RGB2G(RenXt_palovfl)) || (RGB2B(diff) > RGB2B(RenXt_palovfl))) break;
            prop->palette.merge(palent_keep, palent_drop); //map less frequent entry to more frequent entry (to minimize color degradation)
        }
#endif
//    if (prop->palette.size() > prop->desc.maxpal) //&& (RenXt_palovfl < 0)) //drop less frequent entries
//    {
//        pal_dump();

//    if (prop->palette.size() <= prop->desc.maxpal) prop->palette.ByFreq(); //sort it before encoding (didn't need to sort before now because palette was small enough)
//    if (prop->palette.size() != svsize) //errmsg info only
//CAUTION: sorted list might be smaller than palette list; sorted list is pruned if needed
//        for (auto it = prop->palette.sorted.begin(); it != prop->palette.sorted.end(); ++it)
//            debug(20, "reduced palette[%d/%d]: 0x%.6x occurs %d times, last was node# %d", it - prop->palette.sorted.begin(), prop->palette.sorted.size(), prop->palette[it->second].key, prop->palette[it->second].freq, prop->palette[it->second].maxnode);

//        if (prop->palette.size() > 256) return error("palette[%d] too complex: %d palette entries for %d nodes", prop - props.begin(), prop->palette.size(), prop->num_nodes), -1;
//        if (prop->palette.size() == 1) //set all nodes to same value
//        {
//            prop->datasize = 1+3 + 1; //SETPAL(1) + SETALL commands
//            continue;
//        }

//        if (IsDumb(prop->desc.node_type)); //nodesize == 1) //dumb/monochrome
//TODO: merge if too long
//slready dumped        if (remaining_ents != svsize) dump("reduced palette");
    }
};


class PropInfo
{
public:
    bool isparapal;
    Palette palette, indirpal; //palette entries for this prop; parallel palette is banked
    RenXt_Stats::PaletteTypes paltype;
//    std::unordered_map<uint32_t, std::pair<int, int>> indirpal; //indirect palette index#s (only for parallel node groups); first = count, second = max node#
//    uint16_t parapal[32];
    std::vector<int> nodeinx; //color index for each node = list of palette index#s
    const byte* inptr; //ptr to node values
//    int num_nodes;
    enum EncodingType { None = 0, Private_Inverted = -99, Shared_Inverted = 99, Private_1bpp = -1, Shared_1bpp = +1, Private_2bpp = -2, Shared_2bpp = +2, Private_4bpp = -4, Shared_4bpp = +4 , Private_53bpp = -8, Shared_53bpp = 8} enc_type; //NOTE: lower 2 bits of 5.3 value must be 0 so firmware will not unpack bitmap bits
#define IsShared(enc_type)  (enc_type > 0)
    int datasize; //estimated packet data size using best encoding method
    int staddr; //prop/controller start address
//    const byte* cpyptr;
    RenXt_Prop desc; //width, height, node_type, numnodes, num_ctlr from config file (in lieu of UI in sequencer software)
//    NOPE(std::hash_map<node_value, node_value> reduced); //map to reduced colors; can't do this (pixels might be dithered after reduction)
//    int cpylen;
//    int width, height;
//    PIX* lept_pix; //color-quantized leptonica bitmap + palette
//    byte node_type;
//    int num_ctlr;
public:
//    PropInfo(void): lept_pix(0) {}
//    ~PropInfo(void) { if (lept_pix) pixDestroy(&lept_pix); lept_pix = 0; }
public:
//analyze smart nodes by prop:
//most logic is reused for dumb/monochrome nodes also
    void AnalyzeNodes(int inx, RenXt_Stats* stats)
    {
        Palette para_ovfl, para_raw; //second bank of parallel palette, and raw palette for parallel debug
        const byte* inptr = this->inptr;
        int nodesize = (this->desc.node_type && IsDumb(this->desc.node_type))? 1: 3;
        int nodech = (this->desc.order == MONO_RGBW)? 4: 1;
        bool chplex = IsDumb(this->desc.node_type) && IsChplex(this->desc.node_type); //(prop->desc.node_type == RENXt_CHPLEX(0xCC)) || (prop->desc.node_type == RENXt_CHPLEX(0xCA));
        checkpoint(__LINE__);
        this->isparapal = !IsDumb(this->desc.node_type) && IsParallel(this->desc.node_type) && (this->desc.node_type != RENXt_GECE(PARALLEL)); //not needed for GECE
//    if (prop->desc.maxpal > 16) prop->desc.maxpal = 16; //firmware limitation
//    if (prop->desc.maxpal_parallel > 2*31) prop->desc.maxpal_parallel = 2*31; //firmware limitation
//        *paltype = (nodesize == 1)? RenXt_Stats::PaletteTypes::Mono: this->isparapal? RenXt_Stats::PaletteTypes::Parallel: RenXt_Stats::PaletteTypes::Normal;
        debug(10, "analyze: type 0x%x, units %d, ch/node %d, dumb? %d, chplex? %d, parallel? %d, active high? %d", this->desc.node_type, nodesize, nodech, IsDumb(this->desc.node_type), chplex, this->isparapal, IsActiveHigh(this->desc.node_type));
//    int maxpalents = IsDumb(prop->desc.node_type)? 240/3: (1<<4); //to fit in 256 bytes RAM
        if (this->desc.numnodes < 1) //pass thru raw data
        {
            this->datasize = -this->desc.numnodes * nodesize;
            return;
        }
//    int nodesize = IsDumb(prop->desc.node_type)? 1: 3;
        int num_ctlr = divup(this->desc.numnodes, this->desc.ctlrnodes);
//    if (IsDumb(prop->desc.node_type) && (prop->desc.numnodes > 8*7)) error("dumb nodes > 56: %d", prop->desc.numnodes);
        debug(10, "node list resize(%d): nodech %d * (#nodes %d + para? %d pad %d)", nodech * (this->desc.numnodes + (this->isparapal? padlen(this->desc.numnodes, 4): 0)), nodech, this->desc.numnodes, this->isparapal, this->isparapal? padlen(this->desc.numnodes, 4): 0); //reduce memory allocation overhead; don't leave partial node group at end (parallel palette only
        this->nodeinx.resize(nodech * (this->desc.numnodes + (this->isparapal? padlen(this->desc.numnodes, 4): 0))); //reduce memory allocation overhead; don't leave partial node group at end (parallel palette only)
        checkpoint(__LINE__, nodeinx.size());
        MyHashMap<node_value, /*PalentInfo* */ size_t> keys[4]; //allow fast lookups and aliases into palette list during initial construction
        checkpoint(__LINE__);
        if (isparapal) palette.occur(keys[0], 0, 0); //always include a null palette entry at start to allow self-referencing indir entty
        for (int n = 0; n < (int)this->nodeinx.size(); ++n) //generate color palette and index all nodes by color
        {
//            if ((prop == props.begin()) && (n >= 1<<4)) break; //max 16 entries in shared palette
//TODO: parallel past end can match any value; for now, just use 0
            int rgb_type = this->desc.order;
            checkpoint(__LINE__, n, nodeinx.size(), nodech);
            if (nodech == 4) rgb_type = ((n & 3) == 3)? MONO_ANY: MONO_RED; //kludge: take first byte for R, G, B, but use any for W (looks like 2 RGB nodes in xLights)
            node_value rgb = (n < nodech * this->desc.numnodes)? rgb_reorder(rgb_type, (inptr += 3, inptr - 3)): 0;
            if ((nodech == 4) && ((n & 3) != 3)) inptr -= 2; //only consume 1 byte for R, G, B, but 3 for W
            checkpoint(__LINE__, rgb);
            if (this->isparapal) //pivot bits for node group and arrange into 4-byte parallel monochrome palette entries
            {
                static byte monochrome[3][4]; //store R, G, B as monochrome color elements so they can be reused more (parallel only)
                monochrome[0][n & 3] = RGB2R(rgb);
                monochrome[1][n & 3] = RGB2G(rgb);
                monochrome[2][n & 3] = RGB2B(rgb);
                para_raw.occur(keys[3], rgb, n); //for debug only
                if ((n & 3) != 3) continue; //get remaining nodes in group
                checkpoint(__LINE__, n);
//                union { uint32_t together; byte separate[4]; } parapal; //doesn't work with intel (needs big endian, not little)
                node_value rgb_4way[3];
                byte parapalent[4];
                int found = 0;
//            uint_least16_t indirpal = 0;
//                debug(20, "horiz rot");
                for (int i = 0; i < 3; ++i) //generate R, G, B monochrome, 4-way parallel elements
                {
                    SetParallelPalette(parapalent, monochrome[i][0], monochrome[i][1], monochrome[i][2], monochrome[i][3]);
                    rgb_4way[i] = (parapalent[0] << 24) | (parapalent[1] << 16) | (parapalent[2] << 8) | parapalent[3]; //force 32-bit value to be stored as big endian
                    checkpoint(__LINE__, i);
                    if (keys[0].Contains(rgb_4way[i])) ++found;
                }
                int palbank = (keys[0].size() + 3 - found > 32)? 1: 0; //R, G, B must be in same bank (top address bit is shared)
                Palette* banks[2] = {&this->palette, &para_ovfl};
                checkpoint(__LINE__, palbank);
                for (int i = 0; i < 3; ++i) //generate R, G, B monochrome, 4-way parallel elements
                {
//                    ++parallel[combine.together].first;
//                parallel[combine.together].push_back(n+i); //count or remember where used (debug)
                    this->nodeinx[n - 3 + i] = (palbank? 32: 0) + banks[palbank]->occur(keys[palbank], rgb_4way[i], n - 3 + i); //kludge: treat 4-way R, G, B elements as separate nodes with those colors (only uses 3 out of 4); tag node group# with r, g, b
//                indirpal *= 64; indirpal += prop->nodeinx[n - 3 + i]; //generate packed 1+5+5+5 bit indirect palette index
                    debug(90, "para-node[%d.%d/%d] '%d (x%x x%x x%x) => 0x%.6x, inx %d, #occur %d", n, i, this->desc.numnodes, inptr - this->inptr - 3, inptr[-3], inptr[-2], inptr[-1], rgb, this->nodeinx[n], (*banks[this->nodeinx[n] / 32])[this->nodeinx[n] % 32].freq);
                }
//            std::pair<int, int> indir_info& = prop->indirpal[RGB2Value(prop->nodeinx[n - 3], prop->nodeinx[n - 3 + 1], prop->nodeinx[n - 3 + 2])];
//            ++indir_info.first; //count occurrences
//            if (n > indir_info.second) indir_info.second = n; //track max node#
                checkpoint(__LINE__, keys[0].size(), keys[1].size(), keys[2].size());
                this->nodeinx[n] = this->indirpal.occur(keys[2], RGB2Value(this->nodeinx[n - 3], this->nodeinx[n - 3 + 1], this->nodeinx[n - 3 + 2]), n); //use 4th node in parallel node group to hold indirect palette entry
//                debug(10, "node[%d] has inx %d, key 0x%x", n, nodeinx[n], indirpal[nodeinx[n]].key);
                continue;
            }
            checkpoint(__LINE__, chplex, rgb, n, nodeinx.size(), keys[0].size());
            if (nodesize == 1) rgb = rgb2mono(rgb, chplex? n / 7 + 1: 0); //chplex rows 1..8
            this->nodeinx[n] = this->palette.occur(keys[0], rgb, n);
            checkpoint(__LINE__);
            debug(90, "node[%d/%d] '%d (x%x x%x x%x) => 0x%.6x, inx %d, #occur %d, chplex? %d, row %d", n, this->desc.numnodes, inptr - this->inptr - 3, inptr[-3], inptr[-2], inptr[-1], rgb, this->nodeinx[n], this->palette[this->nodeinx[n]].freq, chplex, chplex? n / 7 + 1: 0);
//        inptr += 3;
//nodesize; //NOTE: monochrome channels are stored as RGB so NC effects will work without changes
//            std::pair<int, int>& palent = prop_palette.map[rgb]; //insert new entry if not already there
//            if (palent.second++) continue; //rgb value was already in palette; don't repeat it
//            palent.first = prop_palette.list.size(); //assign next sequential entry#
//            prop_palette.list.push_back(rgb);
//            if (shared_palette.map.find(rgb) == shared_palette.map.end()) shared_bpp = MAXINT; //force private palette to be used
//            else shared_bpp = MAX(shared_bpp, NumBits(shared_palette.map[rgb].first));
        }

//        int prop_bpp = NumBits(prop_palette.list.size() - 1);
//choose encoding type based on palette size:
//        prop->enc_type = PropInfo::Inverted;
        if (para_ovfl.size(true)) //combine into one palette but preserve bank gap
        {
            checkpoint(__LINE__, para_ovfl.size(true));
            debug(10, "pad palette with %d bank ents, then append %d ents", 32 - this->palette.size(true), para_ovfl.size(true));
            for (int i = -1; this->palette.size(true) < 32; --i)
                this->palette.emplace_back(i, i); //pad bank 0 with dummy values
            this->palette.insert(this->palette.end(), para_ovfl.begin(), para_ovfl.end());
        }
        debug(2, "analyze prop[%d]: '%s' adrs 0x%x..0x%x, #ctlr %d, #nodes %d, pal size %d, indir size %d, max pal %d s/%d p, palovfl %d, bkg inx %d occurs %d", inx, this->desc.propname, this->staddr, this->staddr + num_ctlr - 1, num_ctlr, this->desc.numnodes, this->palette.size(true), this->indirpal.size(true), this->desc.maxpal, this->desc.maxpal_parallel, RenXt_palovfl, this->palette.max_occur, this->palette[this->palette.max_occur].freq);
        if (isparapal) para_raw.dump("bare para pal deref"); //for debug only
        this->palette.dump("raw palette");
        if (this->isparapal) this->indirpal.dump("raw indirect");
//        if (!prop->palette.size()) continue; //no node values to send?
//        prop->palette.ByLen3D(0); //sort by freq (decreasing order)

//this is fairly expensive, but not as much as full quantization, so try it first
//first try to combine colors that are similar in order to reduce palette size:
 //       Palette& sortpal = this->isparapal? this->indirpal: this->palette;
        size_t numpal = this->palette.size(true), maxpal = this->isparapal? std::min<int>(this->desc.maxpal_parallel, 2*31): std::min<int>(this->desc.maxpal, 16);
        if (!isparapal) this->palette.reduce(maxpal, stats, paltype); //reduce palette size if needed
        else if (numpal > maxpal) //need to merge original palette entries then regen parallel combinations
        {
//            if (para_raw.reduce(maxpal, stats, paltype)) //need to regenerate parallel entries
            debug(1, "TODO: parapal reduce");
//TODO: iterate over 4-way ents, gen hsv sums, sort by diffs, 4-way merge ents
        }
        checkpoint(__LINE__);
//        for (auto it = this->nodeinx.begin(); it != this->nodeinx.end(); ++it)
//            this->palette.Reord(*it);
//            if ((*it >= this->palette.size(true)) || (this->palette[*it].inx >= (int)this->palette.size(true))) { debug(10, "BAD NODE[%d/%d] INX: points to palette[%d] beyond end %d", it - this->nodeinx.begin(), this->nodeinx.size(), *it, this->palette.size(true)); *it = 0; }
//            else if (this->palette[this->palette[*it].inx].IsDeleted()) debug(10, "ORPHANED NODE[%d/%d] points to palette[%d] points to [%d]", it - this->nodeinx.begin(), this->nodeinx.size(), *it, this->palette[*it].inx);
        if (this->palette.size(false) > maxpal) //firmware limitation
        {
            ++stats->reduce_failed[(nodesize == 1)? RenXt_Stats::PaletteTypes::Mono: this->isparapal? RenXt_Stats::PaletteTypes::Parallel: RenXt_Stats::PaletteTypes::Normal]; //[numpal - maxpal];
            error("Too many palette entries: %d (reduced from %d), limit is %d", this->palette.size(false), numpal, maxpal);
            return;
        }

//then choose encoding type based on palette; in order of preference: set_all + inverted node lists, bitmap @1 bpp, bitmap @2 bpp, bitmap @4 bpp
//NOTE: escape sequences and padding are ignored in initial size calculations, for simplicity
//begin by trying to convert first (most freq) palette entry to set_all and use inverted node lists for all other palette entries:
//        int datasize_best = 0;
//        enum { Inverted, Private_1bpp = -1, Shared_1bpp = +1, Private_2bpp = -2, Shared_2bpp = +2, Private_4bpp = -4, Shared_4bpp = +4 } enc_type = Inverted;
        if (IsDumb(this->desc.node_type)) //nodesize == 1) //dumb/monochrome
        {
            this->palette.ByBrightness(); //must be in decreasing brightness order for phase angle dimming
            this->enc_type = PropInfo::Shared_Inverted; //no palette needed; display events are always encoded as inverted lists (for more efficient phase angle dimming)
            this->datasize = 3 * this->palette.size(false) + 1; //3 bytes per display event (delay, row, cols) + eof marker
//        for (auto palent = prop->palette.sorted.begin(); palent != prop->palette.sorted.end(); ++palent)
//            prop->datasize += 3 * divup(prop->palette[palent->second].freq, 8); //3 bytes per display event; CAUTION: not quite correct, but close
            debug(3, "estimated data size for dumb lists: %d", this->datasize);
            return;
        }

        Palette& sortpal = this->isparapal? this->indirpal: this->palette;
        this->datasize = 0;
//TODO: enc type by controller, not prop
//TODO: use configurable threshold for shared vs. private?
        this->enc_type = ((num_ctlr > 1) && (sortpal.size(false) > 1))? PropInfo::Shared_Inverted: PropInfo::Private_Inverted;
//first calculate data size using inverted lists:
        checkpoint(__LINE__);
        if (!IsShared(this->enc_type)) //add private palette size
        {
            this->datasize = 1 + sortpal.size(false) * (this->isparapal? 4: 3); //parallel palette entries are 4 bytes, series are 3 bytes each
            if (this->isparapal)
            {
                if (this->indirpal.size(false)) this->datasize += 4 * divup(2 * this->indirpal.size(false) + 2, 4); //need to include indirect palette entries
                this->datasize += padlen(this->datasize - 1, 24); //parallel palette will be multiple of 24 bytes
            }
#if 1 //TODO: take this out if SetAll is interleaved
            this->datasize += divup(this->desc.numnodes, 32 /*5 MIPS, or 48 for 8 MIPS*/); //PIC takes ~ 50 instr (~11 usec @5 MIPS or ~7 usec @8 MIPS) to set 4 node-pairs (8 nodes), and chars arrive every ~ 44 usec (8N2 is 11 bits/char), so delay next opcode to give set-all time to finish
#endif // 1
        }
        debug(3, "palette data size: %d (%d entries + %d indir)", this->datasize, this->palette.size(false), this->indirpal.size(false));
//    mainpal.ByFreq(); //sort by freq (decreasing order) to make inverted lists more compact (using SetAll for bkg color)
        sortpal.FindMost(); //find most frequent entry for use with SetAll
        for (auto palent = sortpal.begin(); palent != sortpal.end(); ++palent) //skip bkg color (SetAll handles that one)
        {
            if (palent->IsDeleted()) continue; //skip merged/deleted entries
            if (palent == sortpal.begin() + sortpal.max_occur) continue; //most freq takes no additional space
//        if (palent == prop->palette.sorted.begin()) prop->datasize += 1; //1 bytes for SET_ALL command
//        if (prop->palette.isparapal)
//            prop->datasize += 1 + prop->indirpal[palent->second];
            this->datasize += 1 + palent->freq + NodeBank(palent->maxnode); //1 byte for NODELIST command + 1 byte/node + 1 byte/bank select
        }
        debug(3, "estimated data size for %s setall/inverted lists: %d", IsShared(this->enc_type)? "shared": "private", this->datasize);
        if (sortpal.size(false) < 2) { debug(3, "only one pal ent; just use inverted list"); return; } //just use inverted lists (SetAll); need estimated size so this needs to come after above loop
//next try 4/2/1 bpp bitmap (if applicable):
        checkpoint(__LINE__);
        for (int bpp = this->isparapal? 6: 4; bpp; bpp /= 2)
        {
//TODO: allow different enc type for each controller
            if (this->palette.size(false) > (size_t)(1 << bpp)) break; //TODO: will try to merge/drop colors during second pass if needed
            int datasize_thisway = 0;
            if (!IsShared(this->enc_type)) //add private palette size
            {
                datasize_thisway = 1 + this->palette.size(false) * (this->isparapal? 4: 3); //size for full bitmap @4/2/1 bpp
                if (this->isparapal) datasize_thisway += padlen(datasize_thisway - 1, 24);
            }
            int nodebytes = this->isparapal? divup(this->desc.numnodes, 4) * 2: divup(this->desc.numnodes * bpp, 8);
            datasize_thisway += 3 + nodebytes + padlen(nodebytes, 4);
//            error("%d %d %d", prop->palette.size() * 3, divup(prop->num_nodes * bpp, 8), datasize_other);
            if (datasize_thisway < this->datasize) //this encoding scheme is more compact; use it
            {
                this->datasize = datasize_thisway;
                this->enc_type = /*-bpp*/ (bpp == 6)? PropInfo::Private_53bpp: (bpp == 4)? PropInfo::Private_4bpp: (bpp == 2)? PropInfo::Private_2bpp: (bpp == 1)? PropInfo::Private_1bpp: PropInfo::None;
                this->indirpal.clear(); //palette indirection won't be used
            }
            debug(3, "estimated data size for %s %dbpp bitmap: %d, better? %d", IsShared(this->enc_type)? "shared": "private", bpp, datasize_thisway, ABS(this->enc_type) == bpp);
            if (this->isparapal) break; //only 5.3 bpp is supported so far
        }
        if (ABS(this->enc_type) != ABS(PropInfo::Shared_Inverted)) return;
        if (this->isparapal && this->indirpal.empty()) return; //no need to reorder palette
        if (sortpal.max_occur != sortpal.first) //put most frequent (background) color first
        {
            PalentInfo swapent = sortpal[sortpal.first];
            sortpal[sortpal.first] = sortpal.Bkg();
            sortpal.Bkg() = swapent;
            size_t swapinx = sortpal[sortpal.first].inx;
            sortpal[sortpal.first].inx = sortpal.Bkg().inx;
            sortpal.Bkg().inx = swapinx;
            debug(10, "swapped first [%d], key 0x%x, freq %d and most freq [%d] key 0x%x, freq %d", sortpal.first, sortpal[sortpal.first].key, sortpal[sortpal.first].freq, sortpal.max_occur, sortpal.Bkg().key, sortpal.Bkg().freq);
        }
        else debug(10, "most freq is first, no need to swap");
#if 0
        PalentInfo tmp = sortpal[0];
        sortpal[0] = sortpal[sortpal.max_occur];
        sortpal[sortpal.max_occur] = tmp;
        int nswap = 0;
        std::unordered_map<int, int> debug;
//        if (this->isparapal)
//            for (auto nit = this->indirpal.begin(); nit != this->indirpal.end(); ++nit)
//            {
//            }
//        else
        checkpoint(__LINE__);
        if (sortpal[0].freq >= sortpal[sortpal.max_occur].freq) return; //no need to rearrange palette entries
        for (auto nit = this->nodeinx.begin(); nit != this->nodeinx.end(); ++nit)
        {
            if (this->isparapal) nit += 3; //only last node in each node group points to indirect palette entry
            if (!*nit || (*nit == sortpal.max_occur))
            {
                ++nswap;
                *nit ^= sortpal.max_occur;
                if (++debug[*nit] < 5) //cut down on debug verbosity
                    debug(80, "swap: node[%d/%d] inx was %d, is now %d", nit - this->nodeinx.begin(), this->nodeinx.size(), *nit ^ sortpal.max_occur, *nit);
            }
//                else debug(80, "no swap: node[%d/%d] inx %d vs. 0 or most freq %d", n, this->nodeinx.size(), this->nodeinx[n], sortpal.max_occur);
        }
        debug(10, "swapped? %d sort pal [0] with most freq [%d], fixed up %d node entries", sortpal.max_occur, nswap); //, this->nodeinx.size() / (this->isparapal? 4: 1));
#endif // 0
    }
//convert rgb value to monochrome:
//tag dumb color (brightness) with chipiplexed row# to force row uniqueness
//node_value rgb2mono(const byte* rgb, int rownum)
    static node_value rgb2mono(node_value rgb, int rownum)
    {
//    int brightness = MAX(MAX(rgb[0], rgb[1]), rgb[2]); //use strongest color element as monochrome brightness
        int brightness = RGB2R(rgb); //MAX(MAX(RGB2R(rgb), RGB2G(rgb)), RGB2B(rgb));
        int more_row = 0xff - rownum * 0x11; //scale up row# to fill address space; NOTE: this will sort higher rows first
//    rownum = 0x99 - rownum; //kludge: sort lower rows first to work like Vixen 2.x chipiplexing plug-in
//    if (brightness && (prop->desc.numnodes == 56))? (n / 7): 0; //chipiplexed row# 0..7 (always 0 for pwm); assume horizontal matrix order
        return RGB2Value(brightness, brightness? more_row: 0, brightness? rownum: 0); //tag dumb color with chipiplexed row# to force row uniqueness
    }
//overloaded:
//node_value rgb2mono(node_value rgb, int rownum)
//{
//    return rgb2mono(RGB2R(rgb), RGB2G(rgb), RGB2B(rgb));
//}

};


#if 0
//set debug level:
//returns prev level
//IFCPP(extern "C") __declspec(dllexport)
int RenXt_debug(int level)
{
    int retval = WantDebug;
    WantDebug = level;
    error("debug level %d", level);
    return  retval;
}
#endif

#ifndef _MAX_PATH
 #define _MAX_PATH  256
#endif // _MAX_PATH

//#ifndef __WXMSW__
//#define GetModuleHandle(ignored)  0
//
//int GetModuleFileName(int ignored, char* buf, size_t buflen)
//{
////    static std::string retbuf;
////    if (retbuf.size() < buflen) retbuf.resize(buflen);
//    memset(buf, 0, buflen); //readlink doesn't null terminate
//    return readlink("/proc/self/exe", buf, buflen - 1); //leave one place for null terminator
////    perror("readlink");
//}
//#endif // __WXMSW__

//find INI file:
//IFCPP(extern "C")
#include <wx/stdpaths.h>
bool find_file(const char* inipath, const char* curpath, FileInfo& infile)
{
    std::string fullpath(curpath);
    std::string::size_type ofs;
    char delim = '/';

//get my full path in case file name was not fully qualified and need to search for it:
#if 1
    if (((ofs = fullpath.find("/")) == std::string::npos)) //|| (ofs > 0))
        if (((ofs = fullpath.find("\\")) == std::string::npos)) //|| (ofs > 2))
#endif
        {
//            if (ofs != std::string::npos) delim = fullpath.at(ofs);
            debug(9, "orig path was %s", curpath);
//            fullpath.resize(_MAX_PATH + 1);
//            GetModuleFileName(GetModuleHandle(NULL), (char*)fullpath.c_str(), _MAX_PATH);
//            fullpath.resize(strlen(fullpath.c_str()));
            fullpath = wxStandardPaths::Get().GetExecutablePath();
            curpath = fullpath.c_str();
            debug(9, "orig path now %s", curpath);
        }
    if ((ofs = fullpath.find("/")) == std::string::npos) ofs = fullpath.find("\\");
    if (ofs != std::string::npos) delim = fullpath.at(ofs);

//    infile.stream.open(inipath);
//    infile.path = inipath;
//	while (!infile.stream.is_open()) //try parent location
    std::string parent(curpath);
    for (;;)
    {
        debug(9, "parent path was %s", parent.c_str());
#if 0 //won't link
        boost::filesystem::path path(curpath);
        boost::filesystem::path parent = path.branch_path();

        if (parent.string().empty()) return error("failed to open file"), 0; //no more choices
        curpath = parent.string().c_str();
        debug(9, "cur path is now %s", curpath);
        boost::filesystem::path newfile = operator/(boost::filesystem::path(inifile), parent);
        debug(9, "re-trying path %s", newfile.string().c_str());
        infile.open(newfile.string());
#else
//        static std::string parent(curpath); //CAUTION: make this static so it isn't deallocated each time thru loop
//        for (int i = parent.length() - 1; i >= 0; --i)
//            if ((parent.at(i) == '/') || (parent.at(i) == '\\')) { delim = parent.at(i); parent.erase(i); break; }
        while (!parent.empty()) //remove last part
        {
            char ch = parent.back();
            parent.pop_back();
//            debug(20, "trunc char '%c', new parent = '%s'", ch, parent.c_str());
            if ((ch == '/') || (ch == '\\')) break;
        }
        if (parent.empty()) return error("failed to open file"), false; //no more choices
//        curpath = parent.c_str();
        debug(9, "parent path now %s", parent.c_str()); //curpath);
        std::string newfile = parent /*curpath*/; newfile += delim; newfile += inipath;
        debug(9, "re-trying path %s", newfile.c_str());
        infile.stream.open(newfile);
        infile.path = newfile;
    	if (infile.stream.is_open()) break;
#endif //0
    }
    return true;
}


//open a port:
//parameters:
// port = comm port to use
// baud = baud rate
// data_parity_stop = #data bits, parity, #stop bits (normally 8N1)
// pad_rate = how often to send pad char (0 for no padding); max and still be useful is ~20%?
//return value:
// > 0 => port opened; buffer size
// = 0 => was already open
// < 0 => error#
//IFCPP(extern "C")
int RenXt_open(const char* port, int baud, const char* data_parity_stop, int pad_rate, int fps)
{
    MyPort newport;
    if (!newport.SetName(port)) return -1;
    if (!newport.SetBaud(baud)) return -1;
    if (!newport.SetByteConfig(data_parity_stop)) return -1;
    if (!newport.SetFps(fps)) return -1;
    if (!newport.SetPadrate(pad_rate)) return -1;
//    int oldsize = OpenPorts.size();
//    if (OpenPorts.find(port) != OpenPorts.end())
    MyPort& port_info = OpenPorts[port]; //creates new entry if not there
    if (port_info.IsOpen()) //check for consistent settings
    {
        debug(10, "port %s already open", port);
        return port_info.IsCompatible(baud, data_parity_stop, pad_rate, fps)? 0: -1;
    }
//    if (OpenPorts.size() == oldsize) //was already defined
    port_info = newport;
    return port_info.open()? port_info.io.frbuf.size(): -1;
}


//reopen port using prev settings:
//parameters:
// port = comm port to use
//return value:
// > 0 => port opened; buffer size
// = 0 => was already open
// < 0 => error#
//IFCPP(extern "C")
int RenXt_reopen(const char* port)
{
    if (!OpenPorts.Contains(port)) //OpenPorts.find(port) == OpenPorts.end())
        return error("Port '%s' hasn't been opened\n", port), -1;
    MyPort& port_info = OpenPorts[port]; //creates new entry if not there
    if (port_info.IsOpen())
        return error("Port '%s' is already open\n", port), 0;
    return port_info.open()? port_info.io.frbuf.size(): -1;
}


#if 0 //TODO
void pal_dump(void)
{
    std::string buf;

        while (prop->palette.sorted.size() > prop->desc.maxpal)
        {
            PalentInfo* palent = &prop->palette[prop->palette.sorted.back().second]; //.SortedLast(); //.ByFreq(palsize - 1);
//keep dropping until it fits within palette
//NO            if (palent->freq > -RenXt_palovfl) break;
            debug(5, "palette reduce: drop palent[%d] rgb 0x%.6x, freq %d below threshold %d, merged with palent[%d] rgb 0x%.6x, max pal %d vs. list len %d", palent->inx, palent->key, palent->freq, -RenXt_palovfl, prop->desc.maxpal - 1, prop->palette[prop->desc.maxpal - 1].key, prop->desc.maxpal, prop->palette.sorted.size());
            prop->palette.merge(prop->palette[prop->desc.maxpal - 1], *palent); //merge dropped color with least-used fg color (last preserved entry) in order to preserve uniqueness; don't want to merge with bkg in case black
}
#endif


#if 0
//parallel example:
''[0]: xff 0 0, xff 0 0, 0 xff 0, 0 xff 0,// xff 0 0, xff
''[x10]: 0 0, 0 xff 0, 0 xff 0,// xff 0 0, xff 0 0, xff 0
''[x20]: 0, xff 0 0,// 0 xff 0, 0 xff 0, xff 0 0, xff 0 0,

''[x30]: 0 xff 0, 0 xff 0, xff 0 0, xff 0 0,// xff 0 0, xff
''[x40]: 0 0, 0 xff 0, 0 xff 0,// xff 0 0, xff 0 0, 0 xff
''[x50]: 0, 0 xff 0,// xff 0 0, xff 0 0, xff 0 0, xff 0 0,

''[x130]: 0 0 0, 0 0 0, 0 0 0, 0 0 0,// 0 0 0 0

[DEBUG @785] palette sorted by freq[2/4]: 0x000000 occurs 243 times, inx 2, last was node# B[380..383]
[DEBUG @785] palette sorted by freq[0/4]: 0xcccccccc occurs 20 times, inx 0, last was node# G[96..99]
[DEBUG @785] palette sorted by freq[1/4]: 0x33333333 occurs 20 times, inx 1, last was node# R[96..99]
[DEBUG @785] palette sorted by freq[3/4]: 0xffffffff occurs 5 times, inx 3, last was node# R[88..91]

bitmap: 5.3 bpp, (1, 2, 0), (1, 2, 0), (3, 0, 0), (2, 1, 0), ..., (0, 0, 0), ...
inverted: [0, (1, 2, 0), (2, 1, 0), (3, 0, 0)], SetAll(0), List(1), 0, 1, ..., List(2), 3, 4, ..., List(3), 2, ..., List(0)
#endif // 0


//get node value or value after quantization:
#if 0
node_value GetNodeValue(const byte*& inptr, PropInfo* prop, int n, bool first)
{
#ifdef LEPT
    if (prop->lept_pix)
    {
    static l_int32 pixWpl;
    static l_uint32* line;
    if (first)
    {
        pixWpl = prop->lept_pix? pixGetWpl(prop->lept_pix): 0;
        line = prop->lept_pix? pixGetData(prop->lept_pix) + pixWpl * (prop->desc.height - 1): 0;
    }
//get quantized/mapped/reduced color:
//                    int y = n / prop->desc.width; x = n % prop->desc.width;
//                    for (l_int32 y = 0; y < prop->desc.height; ++y, line -= pixWpl)
//                        for (l_int32 x = 0; x < prop->desc.width; ++x)
    l_uint32* pword = line - pixWpl * (n / prop->desc.width) + (n % prop->desc.width);
    byte color[3];
//    debug(44, "\tlept node[%d] (%d,%d) from @0x%x", n, (n % prop->desc.width), (n / prop->desc.width), pword);
//                    if (fread(&pel, 1, 3, fp) != 3) readerror = 1;
    color[0] = *((l_uint8 *)pword + 3- COLOR_RED);
    color[1] = *((l_uint8 *)pword + 3- COLOR_GREEN);
    color[2] = *((l_uint8 *)pword + 3- COLOR_BLUE);
//                    pel[3] = *((l_uint8 *)pword + L_ALPHA_CHANNEL);
//                    writepixel(i * width + j, pel);
//    debug(20, "lept mapped node[%d] from 0x%.6x to 0x%.6x", n, retval, RGB2Value(color[0], color[1], color[2]));
    return RGB2Value(color[0], color[1], color[2]);
    }
#endif
    inptr += 3;
    return rgb_reorder(prop->desc.order, inptr - 3);
}
#endif


//#define ZOMBIE_RECOVER  3
#define WANT_COMM_DEBUG  20 //1 //how often to check controller status

//encode smart nodes by controller:
//some logic is reused for dumb/monochrome nodes also
bool EncodeNodes(MyOutStream& out, PropInfo* prop, RenXt_Stats* stats, int inx, int seqnum)
{
    const byte* inptr = prop->inptr;
    int nodesize = IsDumb(prop->desc.node_type)? 1: 3;
    bool dumb = prop->desc.node_type && IsDumb(prop->desc.node_type), chplex = IsChplex(prop->desc.node_type); //== RENXt_CHPLEX(0xCC)) || (prop->desc.node_type == RENXt_CHPLEX(0xCA));
    bool is_pwm = dumb && (prop->desc.numnodes != 56); //TODO: need to make this selectable
//    bool pseudo_pwm = dumb && chplex && (prop->desc.numnodes != 56); //TODO: need to make this selectable
    bool has_data = false;
    checkpoint(__LINE__);
//    prop->paltype = RenXt_Stats::PaletteType::Normal;
//    int num_ctlr = divup(ABS(prop->desc.numnodes), prop->desc.ctlrnodes);
//    if (IsDumb(prop->desc.node_type)) //prop->palette.ByValue(); //re-sort by brightness (decreasing order)
//    if (IsDumb(prop->desc.node_type) && prop->palette.by_cdiff.empty()) prop->palette.ByColorDiff(true); //sort by brightness diff
//    else prop->palette.ByFreq(); //sort by freq (decreasing order)
//    const node_value bkg = !IsDumb(prop->desc.node_type)? prop->palette[prop->palette.sorted[0].second].key: 0; //first palette entry treated as bkg color (typically black, but doesn't need to be); bkg always black for dumb pixels
//        /*const*/ Palette* palptr = &prop->palette;
    for (int adrs = prop->staddr, stnode = 0; stnode < ABS(prop->desc.numnodes) /*adrs < prop->staddr + num_ctlr*/; ++adrs, stnode += prop->desc.ctlrnodes)
    {
        int svused = out.used; //for debug only
#ifdef ZOMBIE_RECOVER
        bool need_config = !(seqnum % ZOMBIE_RECOVER);
#else
        bool need_config = !seqnum; //once only at start frame; TODO: use non-empty send count rather than seqnum?
#endif // ZOMBIE_RECOVER
        if (need_config) has_data = true; //need to send even if no data, in order to preserve adrs order; //{ if (!has_data) debug(10, "has data: refresh"); has_data = true; } //need to send this one even if no real data
        debug(3, "prep to send prop[%d] '%s', #ctlr %d, adrs 0x%x, #nodes %d of %d, nodesize %d, ramscale %d, dumb? %d, chplex? %d, pwm? %d, enc type %d %s, seqnum %d", inx, prop->desc.propname, divup(ABS(prop->desc.numnodes), prop->desc.ctlrnodes), adrs, MIN(ABS(prop->desc.numnodes) - stnode, prop->desc.ctlrnodes), prop->desc.numnodes, nodesize, prop->desc.ramscale, dumb, chplex, is_pwm, prop->enc_type, IsShared(prop->enc_type)? "shared": "private", seqnum);
//        out.start_pkt(__LINE__, adrs); //split packet because adrs changes
//        out.DelayNextOpcode(0); //reset wait state on first opcode for this processor
//NOTE: this will always address each processor, even if there's no data to send
//this is redundant, but is useful for checking if processors are still responding
//        out.start_pkt(__LINE__, adrs, refresh? (prop->desc.node_type & 0xF): 0, (refresh && prop->desc.noderam /*&& !dumb*/)? prop->desc.noderam: 0); //ABS(prop->desc.numnodes) / 2); //split packet because adrs changed; send node config before any other opcodes
        if (need_config) out.SendConfig(adrs, prop->desc.node_type & 0xF, prop->desc.noderam); //NOTE: must be sent before any node-related opcodes
//TODO: skip addresses with no data to send (discard after highest, but keep placeholders before lowest)
#ifdef WANT_COMM_DEBUG //firmware/comm DEBUG and/or status check
//TODO: use non-empty send count rather than seqnum
        if (!(seqnum % WANT_COMM_DEBUG))
        {
            if (!need_config) out.BeginOpcode(adrs, +1); //, 1); //, 2+1+(5+1)+1, 0, 1); //NOTE: must include trailing Sync; include leading Sync + adrs since this is first opcode for this adrs
//            out.start_pkt(__LINE__, adrs); //kludge: split packet in case bad rcv
            out.emit_opc(RENXt_ACK); //check listener/packet status
            out.BeginOpcodeData(); //remainder of opcode bytes will be returned from processor
            out.emit(RENXt_NOOP, 5+1+1+1+1); //placeholders for status bits, i/o errs, proto errs, node bytes; CAUTION: must allow enough esc placeholders to protect next opcode
//            out.emit_raw(RENARD_SYNC); //kludge: send extra Sync in case prev byte was Escape
//            out.start_pkt(__LINE__, adrs); //start new opcode for palette; ;;;;Sync above requires adrs and start of new opcode pkt
//            myadrs_stofs = out.used;
        }
#endif // 1
//        size_t myadrs_stofs = out.used;
        if ((prop->desc.numnodes < 1) || (prop->enc_type == PropInfo::None)) //pass thru raw or non-RGB data
        {
//            out.BeginOpcode(__LINE__, adrs); //NOTE: assumes Sync + adrs were set above; if not, they need to be emitted here as part of opcode
#ifndef WANT_COMM_DEBUG
//            out.emit_raw(RENARD_SYNC);
//            out.emit(adrs); //CAUTION: must send all active addresses in order to maintain correct addressing; lowest address indicates start of frame
            out.start_pkt(adrs); //NOTE: include leading Sync + adrs since this is first opcode for this adrs
#endif // WANT_COMM_DEBUG
            out.BeginOpcode(adrs, +1);
            if (prop->desc.numnodes < 0) out.emit_buf(inptr, -prop->desc.numnodes * nodesize); //send raw/macro data
            continue;
        }
//        int num_nodes = (prop->desc.numnodes > 0)? prop->desc.numnodes / 3: 0; //propdesc->numnodes; //channels vs. raw/macro data; assume RGB channels

#if 0 //firmware/comm DEBUG
        start_pkt(out, adrs); //kludge: split packet in case bad rcv
        out.emit(RENXt_ACK); //check listener/packet status
        out.emit(0, 4); //placeholders
#endif // 1
        checkpoint(__LINE__);
        bool adrs_private = false;
        if (!IsShared(prop->enc_type)) //send palette (private or first-time shared)
        {
//        if (!prop->enc_type) continue;
//        out.emit_raw(RENARD_SYNC); //start of packet for this controller
//            out.BeginOpcode(__LINE__, adrs); //CAUTION: might be shared even though tagged with only one adrs
            adrs_private = (ABS(prop->desc.numnodes) <= prop->desc.ctlrnodes);
//no            if (!adrs_sent) out.CancelOpcode();
//NOTE: ADRS_ALL should not be used in first frame because processors are not configured yet
//            out.BeginOpcode((adrs_sent || !seqnum)? adrs: ADRS_ALL); //send palette to multiple processors (not on first frame)
//            out.start_pkt(__LINE__, adrs_sent? adrs: ADRS_ALL, refresh? (prop->desc.node_type & 0xF): 0, (refresh && prop->desc.noderam /*&& !dumb*/)? prop->desc.noderam: 0); //ABS(prop->desc.numnodes) / 2);
//            if (refresh) has_data = true; //need to send even if no data, in order to preserve adrs order; //{ if (!has_data) debug(10, "has data: refresh"); has_data = true; } //need to send this one even if no real data
//            out.emit_raw(RENARD_SYNC);
//            out.emit(adrs_sent? adrs: ADRS_ALL); //share palette with other controllers on this prop
//            palptr = &prop->palette;
            int palents = prop->palette.size(false);
            if (prop->isparapal)
            {
                if (prop->indirpal.size(false)) palents += divup(2 * prop->indirpal.size(false) + 2, 4); //include indirect palette entries (offset by 1/2 entry)
                palents = divup(palents, 6); //convert 4-byte to 24-byte units
            }
//#if 1
//            out.emit_raw(RENARD_PAD); //0); //kludge: Gdoor is dropping a byte occasionally, so try to avoid that here be inserting a benign pad byte so that palette will be received correctly; else palette corruption will cause lots of flashing
//#endif
//            out.BeginOpcode(__LINE__, adrs);
            out.BeginOpcode(adrs_private? adrs: ADRS_ALL, +1); //send palette to multiple processors
            out.emit_opc(RENXt_SETPAL(palents));
//TODO: add start ofs for segmented palettes
            ++stats->num_palettes[prop->paltype];
            stats->total_palents[prop->paltype] += palents;
            debug(10, "!shared palette; send %d ents, pal size: %d, indir: %d", palents, prop->palette.size(false), prop->indirpal.size(false));
//TODO: overlay sh pal with private palette?  cache palette + don't resend?
//TODO: overlay indir entries with full palette (especially entry#0)
            if (prop->indirpal.size(false)) //send indirect palette entries first
            {
//TODO: if !indirpal[0] don't need to pre-pad + reloc
                out.emit_uint16(0); //null pad indirect entry #-1 (allows self-referencing entry#0 black)
                for (auto it = prop->indirpal.begin(); it != prop->indirpal.end(); ++it)
                {
                    if (it->IsDeleted()) continue; //skip merged/deleted entries
//                    int reorder = it - prop->indirpal.begin();
//                    if (!reorder || (reorder == prop->indirpal.max_occur)) reorder ^= prop->indirpal.max_occur; //move most freq to start of list
                    byte r = RGB2R(it->key), g = RGB2G(it->key) % 32, b = RGB2B(it->key) % 32; //unpack palette indices for each color element
                    int reloc = divup(prop->indirpal.size(false) + 1, 2); //palette index values will shift down by the space occupied by the indirect palette
                    r += reloc; g += reloc; b += reloc; //CAUTION: assumes no wrap
                    uint16_t newkey = (r << 10) + (g << 5) + b; //repack into 5.3 bits per value; NOTE: top bit of r will be set for second palette bank
                    out.emit_uint16(newkey); //prop->indirpal[reorder].key);
                    debug(10, "send indir palent[%d/%d]: key 0x%x (r %d+%d, g %d+%d, b %d+5d)", it - prop->indirpal.begin(), prop->indirpal.size(false), newkey, r - reloc, reloc, g - reloc, reloc, b - reloc, reloc); //it->key, it->inx);
                }
                if (!(prop->indirpal.size(false) & 1)) out.emit_uint16(0); //pad to even 4-byte boundary
            }
//            Palette& sortpal = prop->isparapal? prop->indirpal: prop->palette;
            checkpoint(__LINE__);
//            bool first = true;
            for (auto palit = prop->palette.begin(); palit != prop->palette.end(); ++palit) //NOTE: must send colors in index order
            {
                if (palit->IsDeleted()) continue; //skip deleted/merged entries
//                if (((palit - prop->palette.begin()) & 3) == 3) continue; //
//                std::stringstream links;
//                int leaf = palent->second, next;
//                while ((next = prop->palette[leaf].inx) != leaf)
//                {
//                    links << " -> " << next;
//                    leaf = next;
//                }
//                prop->palette[it->second.drop].inx = leaf;
//                debug(14, "send palent[%d/%d] 0x%.6x inx %d, del? %d, relabelled = %d", palent - prop->palette.begin(), prop->palette.size(), prop->palette[palent->second].key, prop->palette[palent->second].inx, prop->palette[palent->second].IsDeleted(), palent - prop->palette.sorted.begin());
//                int reorder = palit - prop->palette.begin();
//                if (!prop->isparapal)
//                    if (!reorder || (reorder == prop->palette.max_occur)) reorder ^= prop->palette.max_occur; //move most freq to start of list
                node_value color = palit->key; //prop->palette[reorder].key; //prop->palette[palent->second].key;
                if (IsGECE(prop->desc.node_type)) color = RGB2IBGRZ(color); //convert to GECE IBGR format after palette quantization
                if (prop->isparapal) out.emit_uint32(color); //4 bytes each
                else out.emit_rgb(color); //3 bytes each
//                prop->palette[palent->second].inx = palent - prop->palette.sorted.begin(); //relabel with actual index# used
                debug(14, "send palent[%d/%d] key 0x%.6x, orig inx %d, is del? %d", palit - prop->palette.begin(), prop->palette.size(false), palit->key, palit->inx, palit->IsDeleted());
//                palent->inx = palent - prop->palette.begin(); //relabel with actual index# used
            }
            checkpoint(__LINE__);
            if (prop->isparapal) out.SetOpcodeTerminator(RENARD_SYNC); //kludge: firmware wants 24 bytes for each entry but this requires 12 bytes padding on average, so just cancel and start another opcode
//            out.BeginOpcode(__LINE__, adrs); //CAUTION: might be rewritten later, but needs to start now so Sync + adrs are included
//            out.EndOpcode(__LINE__);
            debug(3, "sent %d private palette entries, %d indir to adrs 0x%x", prop->palette.size(false), prop->indirpal.size(false), adrs_private? adrs: ADRS_ALL);
//#endif
            prop->enc_type = (PropInfo::EncodingType)-prop->enc_type; //mark as shared
//            *&reinterpret_cast<int>(prop->enc_type) = -prop->enc_type;
//            prop->enc_type = (prop->enc_type == PropInfo::Private_4bpp)? PropInfo::Shared_4bpp: (prop->enc_type == PropInfo::Private_2bpp)? PropInfo::Shared_2bpp: (prop->enc_type == PropInfo::Private_1bpp)? PropInfo::Shared_1bpp: (prop->enc_type == PropInfo::Private_Inverted)? PropInfo::Shared_Inverted: PropInfo::None*/; //now it looks like shared; don't need to share it again
        }
//        if (!adrs_sent) start_pkt(adrs); //num_ctlr > 1) //need to address each controller separately
//        /*if (adrs_private)*/ out.BeginOpcode(adrs, +1); //split up opcodes for easier re-send (palette might be large)
//        else if (myadrs_stofs != out.used) out.start_pkt(__LINE__, adrs); //start new opcode just for this processor
//        {
//            out.BeginOpcode(__LINE__, adrs); //CAUTION: might be rewritten later, but needs to start now so Sync + adrs are included
//            out.CancelOpcode(); //kludge: replace opcode started above with new one containing config info here
//            out.start_pkt(__LINE__, adrs, refresh? (prop->desc.node_type & 0xF): 0, (refresh && prop->desc.noderam /*&& !dumb*/)? prop->desc.noderam: 0); //ABS(prop->desc.numnodes) / 2);
//        }
//        {
//            out.emit_raw(RENARD_SYNC);
//        if (!prop->enc_type) continue;
//        out.emit_raw(RENARD_SYNC); //start of packet for this controller
//            out.emit(adrs); //CAUTION: must send all active addresses in order to maintain correct addressing; lowest address indicates start of frame
//        }
//        if (!seqnum && //extra initialization info for first frame; resend 1x/sec in case it gets trashed
//            (prop->desc.node_type & 0xF)) //set node type in case not already set in eeprom
//            {
//                out.emit(RENXt_SETTYPE(0)); //override value in eeprom
//                out.emit(RENXt_SETTYPE(prop->desc.node_type));
//            }

//now encode the node values:
//        if (prop == props.begin()) //shared palette; don't send node data
//        {
//            out.emit_buf(prop->nodeptr, prop->num_nodes); //send extra raw/macro data
//            debug(1, "sent %d extra bytes", out.used - svused);
//            continue;
//        }
//TODO: don't resend node data if only the color palette changed
        checkpoint(__LINE__);
        if (prop->enc_type == PropInfo::Shared_Inverted) //send out inverted node lists
        {
            Palette& sortpal = prop->isparapal? prop->indirpal: prop->palette;
            debug(10, "shared inverted: %d node lists, dumb? %d", sortpal.size(false), dumb);
            checkpoint(__LINE__);
//            if (!seqnum && prop->desc.noderam && !dumb) //set node memory size for set-all (not used for dumb pixels)
//            {
//                out.emit(RENXt_NODEBYTES); //set node count to next byte (prep for clear-all or set-all)
//                out.emit(prop->desc.noderam); //ABS(prop->desc.numnodes) / 2);
//            }
//            out.emit(bkg);
//#if 1
//            start_pkt(out, adrs); //check if still listening
//#endif // 1
//            std::unordered_map</*node_value*/ int, std::vector<int>> node_lists; //inverted node lists (increasing node# order); are bitmap or rescan better?
//            std::vector<node_value> node_keys; //hash map does not preserve entry order, so keep a separate ordered list
//            line -= pixWpl * stnode / prop->desc.width; //start on stnode's line
//NOTE: can't use node lists from analysis step in case dithering occurred
//            debug(10, "if %d || (%d > 1) for %d .. %d or %d", dumb, sortpal.size(false), stnode, prop->desc.numnodes, stnode + prop->desc.ctlrnodes);
            if (dumb || (sortpal.size(false) > 1)) //separate nodes by color if they are not all the same
            for (int n = stnode; (n < prop->desc.numnodes) && (n < stnode + prop->desc.ctlrnodes); ++n) //CAUTION: might span controllers
            {
                checkpoint(__LINE__, n, prop->desc.numnodes, stnode + prop->desc.ctlrnodes);
                if (prop->isparapal) n += 3; //last node in group is the only one indexed
//                node_value rgb = (nodesize == 1)? rgb2mono(inptr, (prop->desc.numnodes == 56)? n / 7 + 1: 0): rgb_reorder(prop->desc.order, inptr);
//                node_value rgb = GetNodeValue(inptr, prop, n, n == stnode);
//                if (nodesize == 1) rgb = rgb2mono(rgb, chplex? n / 7 + 1: 0);
////                else if (!prop->lept_pix)
//                {
//                    if (!prop->palette.contains(rgb)) { error("node [%d/%d] color 0x%.6x not in rgb map!", n - stnode, prop->desc.numnodes, rgb); continue; }
//                    debug(1, "rgb node %d val 0x: key => inx %d = rgb 0x%.6x", n, rgb, prop->palette.keys[rgb], prop->palette[prop->palette.keys[rgb]].key);
//                    rgb = prop->palette[prop->palette.keys[rgb]].key;
//                }
//                if (rgb == bkg) continue; //exclude bkg color from inverted lists
//                int svlen = node_lists.size();
                if ((n < stnode) || (n >= stnode + (int)prop->nodeinx.size())) { debug(1, "WHOOPS: %d - %d vs %d", n, stnode, prop->nodeinx.size()); continue; }
                size_t inx = prop->nodeinx[n - stnode]; //use node# relative to this controller
//NO; already adjusted node indices                inx = sortpal[inx].inx; //map back to original index
//                if (inx >= sortpal.size(true)) { debug(1, "WHOOPS: %d vs. %d", inx, sortpal.size(true)); inx = 0; }
//                inx = sortpal[inx].inx; //map to updated index
                inx = sortpal.Reord(inx);
//                if ((inx < 0) || (inx >= (int)sortpal.size(false))) { debug(1, "WHOOPS: %d vs. %d", inx, sortpal.size(true)); continue; }
                if (dumb && !sortpal[inx].key) continue; //dumb are off unless turned on; don't need list
//                if ((inx == /*prop->palette[prop->palette.sorted.front().second].inx*/ prop->palette.max_occur) && !dumb && Contains(node_lists, inx)) //don't need node list for bkg color, so just create 1 entry as placeholder and skip remainder (list could be large)
//                {
//                    if (prop->palette.sorted.size() > 1) continue;
//                    break; //don't need the rest of list once placeholder has been added
//                }
//                debug(10, "add node[%d] orig inx %d to node list[%d], key 0x%x, len now is %d", n - stnode, prop->nodeinx[n - stnode], inx, sortpal[inx].key, sortpal[inx].node_list.size());
                checkpoint(__LINE__, adrs, inx, n, stnode, prop->desc.numnodes, prop->desc.ctlrnodes, sortpal.size(true));
                checkpoint(__LINE__, sortpal[inx].node_list.size(), 102);
                sortpal[inx].node_list.push_back(n - stnode); //use node# relative to this controller
//                if (node_lists.size() != svlen) node_keys.push_back(rgb);
                checkpoint(__LINE__);
            }
//            if (node_lists.empty()) node_lists[0].clear(); //kludge: dedup already decided that we need to send something, so create dummy node list for first and only palette entry
            checkpoint(__LINE__);
            debug(5, "generated %d inverted node lists for nodes %d..%d, set-all delay count %d", sortpal.size(false), stnode, MIN(prop->desc.numnodes, stnode + prop->desc.ctlrnodes) - 1, /*prop->lept_pix? 1: 0,*/ divup(MIN(prop->desc.numnodes - stnode, prop->desc.ctlrnodes), 40));
//            if (node_lists.size() != sortpal.size) debug(1, "heavy danger: %d node lists, %d pal ents", node_lists.size(), sortpal.size());
//NOTE: node lists generated from Leptonica data should already to reduced, so there should be <= 16 of them
//non-Leptonica color reduction need to be applied manually, above, to get the same results
            std::unordered_map<int, int> colmasks; //on columns by active row
            checkpoint(__LINE__);
            int tailpad = 0, brightness = 255; //start with max/total brightness (dumb nodes only); AC phase angle requires this, but DC/PWM can be in any order
//            int padlen = 0;
#pragma message WARN("TODO: SSR doublers, variable DC dim cycle, dumb set-all")
            for (auto it = sortpal.begin(); it != sortpal.end(); ++it)
            {
                if (it->IsDeleted()) continue; //skip deleted/merged entries
#if 1 //debug only
                std::stringstream buf;
                for (auto nit = it->node_list.begin(); nit != it->node_list.end(); ++nit)
                {
                    checkpoint(__LINE__);
//                    if (buf.capacity() < buf.size() + 10) buf.resize(buf.size() + 100);
//                    int buflen = snprintf(&buf[buf.size()], 10, ", %d", *nit);
//                    buf.resize(buf.size() + buflen);
//                    debug(20, "5buf size %d: %s", buf.size(), buf.c_str());
                    buf << ", " << *nit;
                }
                debug(20, "node list[%d]: pal key 0x%x, pal orig inx %d, %d ents: %s", it - sortpal.begin(), it->key, it->inx, it->node_list.size(), !buf.str().empty()? buf.str().c_str() + 2: "(none)");
#endif // 1
                if (it - sortpal.begin() == (int)sortpal.first) //don't need first list; use set-all instead (assumed to have the highest occurrences)
                {
                    if (!dumb)
                    {
                        checkpoint(__LINE__);
//no                        int delay = divup(MIN(prop->desc.numnodes - stnode, prop->desc.ctlrnodes), prop->isparapal? 128 /*8 MIPS: 2-3 usec == 8 nodes*/: 32 /*5 MIPS, or 48 for 8 MIPS*/); //PIC takes ~ 50 instr (~11 usec @5 MIPS or ~7 usec @8 MIPS) to set 4 node-pairs (8 nodes), and chars arrive every ~ 44 usec (8N2 is 11 bits/char), so delay next opcode to give set-all time to finish
                        int delay = 1 + (prop->isparapal? divup(prop->desc.noderam, 128/2 /*8 MIPS: 2-3 usec == 8 nodes*/): (prop->desc.noderam > 10)? divup(prop->desc.noderam - 10, 5 /*5 MIPS, or 48 for 8 MIPS*/): 0); //PIC takes ~ 50 instr (~11 usec @5 MIPS or ~7 usec @8 MIPS) to set 4 node-pairs (8 nodes), and chars arrive every ~ 44 usec (8N2 is 11 bits/char), so delay next opcode to give set-all time to finish; use actual memory size allocated, for more precise delays
//#if 1
//                        out.emit_raw(RENARD_PAD); //kludge: Gdoor is dropping a byte occasionally, so try to avoid that here be inserting a benign pad byte so that palette will be received correctly
//#endif
                        out.BeginOpcode(adrs, +1);
                        out.SetOpcodeDuration(IsGECE(prop->desc.node_type)? 0: delay);
                        out.emit_opc(RENXt_SETALL(0)); //fill with bkg color first; this sets "most" of the nodes, reducing the remaining number yet to be set
//                        debug(10, "has data: SetAll");
                        has_data = true;
//NO- NodeFlush follows!                        if (it + 1 == prop->palette.sorted.end()) continue; //only need wait states if there's more data for this processor
                        if (IsGECE(prop->desc.node_type)) continue; //GECE I/O finishes asynchronously; don't need delay here
//                        /*if (!IsGECE(prop->desc.node_type))*/ out.DelayNextOpcode(delay); //update wait state on *next* opcode
//latest: 5 MIPS: 48 => 8, 36 => 5, 20 => 2, 10 => 0
//timing: 16F1827 at 8 MIPS is taking 2 - 3 char times to set 640 nodes, so denominator above can be ~ 210
//???check this: 16F688 at 4.5 MIPS takes 2 - 3 char times for 40 nodes or 13 chars for 240 nodes
                        debug(10, "first: set all, %d max nodes, using delay %d for %d node bytes", MIN(prop->desc.numnodes - stnode, prop->desc.ctlrnodes), delay, prop->desc.noderam);
//TODO: overlap delay NOOPs with next packet
//                        out.BeginOpcode(__LINE__, adrs, delay, true);
//                        out.EndOpcode(__LINE__); //mark end of opcode before noop padding so it can be dropped during interleave pass
//                        out.emit(RENXt_NOOP, delay); //NOTE: opcode interleave later in pipeline will drop this
//NOTE: after wait state is complete, Sync *must* follow to put opcode parser back into known state
//this will be handled by opcode interleave
//                        out.BeginOpcode(__LINE__, adrs); //start a new opcode for node lists
//not helpful                        out.start_pkt(adrs); //kludge: split packet in case bad rcv (in case wait state was not long enough); NOTE: firmware enforces this
#if 0 //perf tuning for SetAll wait states
//read updated stats
                        out.BeginOpcode(__LINE__, adrs, 5+1);
                        out.emit(RENXt_ACK); //check listener/packet status
                        out.emit(0, 5+3); //placeholders for status bits, i/o errs, proto errs, node bytes, wait count; allow for 1 esc
//                        out.emit_raw(RENARD_SYNC); //kludge: send one more Sync in case prev byte was Escape
#endif // 1
                        continue;
//            prop->palette.reindex();
//TODO: overlap processing delay with next PIC?
//            int listcount = 0;
                    }
                    checkpoint(__LINE__);
//#if 1
//                    out.emit_raw(RENARD_PAD); //kludge: Gdoor is dropping a byte occasionally, so try to avoid that here be inserting a benign pad byte so that palette will be received correctly
//#endif
                    out.BeginOpcode(adrs, +1);
                    out.emit_opc(RENXt_DUMBLIST); //start of dumb pixel display event list (chplex/pwm)
//                    debug(10, "has data: DumbList");
                    has_data = true;
//use phase angle dimming model for DC as well (to simplify the logic):
                    int delay = brightness - RGB2R(it->key); //dummy event is needed to delay first on
                    if (!it->key) delay = 0; //don't need delay if node list is empty / all off
//                    if (IsGECE(prop->desc.node_type)) continue; //GECE I/O finishes asynchronously; don't need delay here
//TODO?                    if (is_pwm) delay = 0; //combine leading and trailing null events if not phase angle (AC) dimming
                    int listlen = 3 * (sortpal.size(false) - 1 + 1 + (delay? 1: 0)) + 1; // + 1; //handle eol marker as part of padding; NOTE: don't need event for full off, but need one to pad out dimming cycle
                    tailpad = padlen(listlen, 4);
                    debug(20, "first dumb: cur br %d, first %d, duration %d, listlen %d, padlen %d", brightness, RGB2R(it->key), delay, listlen, tailpad);
                    out.emit(divup(listlen, 4)); //quad bytes
                    out.emit(0); //no skip
                    if (delay) { out.emit(delay); out.emit(0, 2); } //first event (no rows, no cols) to delay first on
                    brightness -= delay;
                }
//                (it == prop->palette.sorted.begin()) //don't need first list; use set-all instead (assumed to have the highest occurrences)
//                if (!Contains(node_lists, it->inx))
//                {
//                    if (!dumb || sortpal[it->second].key) error("can't find any nodes for list[%d] inx %d", it - sortpal.begin(), it->second);
//                    continue;
//                }
//                std::vector<int>& nodelist = node_lists[it->second];
//                debug(10, "dumb inverted encode: by brightness[%d/%d] key 0x%.6x, val inx %d => val 0x%.6x, val inx %d has %d nodes", it - prop->palette.sorted.begin(), prop->palette.sorted.size(), it->first, it->second, prop->palette[it->second].key, prop->palette[it->second].inx, nodelist.size());
                checkpoint(__LINE__);
                debug(10, "emit node list[%d/%d] key 0x%.6x, inx %d has %d nodes", it - sortpal.begin(), sortpal.size(false), it->key, it->inx, it->node_list.size());
                if (dumb) //dumb pixel display event list (chplex/pwm)
                {
//                    int delay = brightness - RGB2R(it->first);
//                    if ((delay < 1) && (it != prop->palette.sorted.begin())) delay = 1; //must be at least 1 timeslot
                    int duration = /*RGB2R(it->first)*/ brightness - ((it + 1 != sortpal.end())? RGB2R(it->key): 0); //NOTE: dimming might be behind schedule, so use actual brightness rather than desired brightness here
                    if (duration < 1) duration = 1; //must be >= 1 timeslot for triacs to turn on; if we fall behind we can catch up later
                    out.emit(duration); brightness -= duration;
                    int row = chplex? RGB2B(it->key): 0; //node_lists[it->first].front() / 7: 0;
//                    int colmask = 0;
                    if (!(is_pwm && chplex)) colmasks[row] = 0; //not cumulative (not pseudo-pwm)
                    for (auto nodeptr = it->node_list.begin(); nodeptr != it->node_list.end(); ++nodeptr)
                    {
                        int colnum = chplex? *nodeptr % 7 + 1: *nodeptr; //((prop->desc.numnodes == 56)? 7: 8);
                        if (/*(prop->desc.numnodes == 56)*/ chplex && (colnum >= row)) ++colnum; //skip missing charlieplex diagonal
                        colmasks[row] |= 0x100 >> colnum;
                        debug(90, "node[%d/%d] node %d => row %d, col %d cols 0x%x", nodeptr - it->node_list.begin(), it->node_list.size(), *nodeptr, row, colnum, colmasks[row]);
                    }
                    out.emit(chplex? 0x100 >> row: row); //convert to mask if chipiplexed; leave 0 for pwm (no row)
                    out.emit(colmasks[row]); //cumulative column map for this row
                    debug(5, "out dumb evt: next brightness %d, row 0x%x, %d cols 0x%x, duration %d", brightness, chplex? 0x100 >> row: row, it->node_list.size(), colmasks[row], duration);
//                    brightness -= delay; //RGB2R(it->first);
                    continue;
                }
//                    if (it == prop->palette.sorted.begin() + 1) start_pkt(out, adrs); //kludge: for new pkt
                int node_esc = RENXt_NODELIST(it - sortpal.begin()); //it->inx); //prop->palette[prop->palette.keys[it->first]].inx); //esc code to start next list or switch banks
                out.BeginOpcode(adrs, 10); //split node lists into multiple opcodes to allow easier re-send
                out.SetOpcodeTerminator(RENXt_NODELIST_END); //end of list; need this because next esc code is repeated
//TODO: use NodeRange for groups of consecutive node#s
                out.emit_opc(node_esc);
                size_t split_stofs = out.used, split_len = -1;
#ifdef SPLITABLE
//                split_len = std::max<size_t>(it->node_list.size(), SPLITABLE);
//                while (split_len > SPLITABLE) split_len >>= 1; //if we need to split it, try to make even chunks
                split_len = divup(it->node_list.size(), SPLITABLE); //#chunks to split into
                split_len = divup(it->node_list.size(), split_len); //try to make chunk size fairly even
#endif // SPLITABLE
                int prevbank = NodeBank(0), prevofs = NodeOffset(0);
//no!                int node2grp = prop->isparapal? 4: 1;
                debug(10, "send node list esc 0x%x, now send %d nodes, split every %u bytes", node_esc, it->node_list.size(), split_len);
                checkpoint(__LINE__);
                for (auto /*std::vector<int>::iterator*/ nodeptr = it->node_list.begin(); nodeptr != it->node_list.end(); ++nodeptr)
                {
#ifdef SPLITABLE
                    if (out.used - split_stofs > split_len) //split into smaller node lists to allow pad or rexmit; excl "=" to allow stofs slop
                    {
//                        out.emit(RENXt_NODELIST_END); //end of list; need this because next esc code is repeated
//                        out.EndOpcode(__LINE__);
//                        out.emit_raw(RENARD_PAD);
                        debug(10, "flush node list x%x and start another one", node_esc);
                        out.BeginOpcode(adrs, +0); //split node lists into multiple opcodes to allow easier re-send
                        out.SetOpcodeTerminator(RENXt_NODELIST_END); //end of list; need this because next esc code is repeated
                        out.emit_opc(node_esc); //start another list for same color
                        split_stofs = out.used;
                        prevbank = NodeBank(0); //reset bank tracking
                        prevofs = NodeOffset(0);
                    }
#endif // SPLITABLE
//                    debug(10, "node[%d/%d]: bank %d vs. prev bank %d, ofs %d vs. prev %d", nodeptr - it->node_list.begin(), it->node_list.size(), NodeBank(*nodeptr), prevbank, NodeOffset(*nodeptr), prevofs);
                    for (int newbank = NodeBank(*nodeptr); newbank > prevbank; ++prevbank) //check for bank switch
                    {
                        bool implicit = (newbank == prevbank + 1) && (NodeOffset(*nodeptr) <= prevofs) && NodeOffset(*nodeptr); //kludge: disable implicit bank switch on first node
                        debug(5, "out byte[%d]: new node %d, prev node %d, implicit bank switch? %d", out.used, *nodeptr, MakeNode(prevbank, prevofs), implicit);
                        if (!implicit) out.emit(node_esc); //explicit jump to next bank
                    }
//obsolete??                        if (!*nodeptr) out.emit(node_esc, 7); //kludge: incorrect bank switch on node 0
                    prevofs = NodeOffset(*nodeptr);
                    out.emit(prevofs);
                }
                debug(10, "sent %d bytes for color 0x%.6x inx %d", out.used - svused, it->key, node_esc & 0xF);
                svused = out.used;
//#ifdef SPLITABLE //allow lists to be split up
//                out.emit(RENXt_NODELIST_END); //end of list
////                out.EndOpcode(__LINE__);
//#endif // 1
            }
            if (dumb) //dumb pixel display event list (chplex/pwm)
            {
//                if (brightness) //fill in remainder of pwm dimming cycle (not needed for AC or other synced dimming); for simplicity, just send it rather than keeping track of whether controller sees sync signal
//                {
//                    out.emit(brightness);
//                    out.emit(0); //no rows; TODO: is this needed?
//                    out.emit(0); //no culmns
//                }
                debug(10, "dumb tail pad %d + eof", tailpad);
                /*if (tailpad)*/ out.emit(0, tailpad + 1); //end of list marker + quad-byte padding
//                out.EndOpcode(__LINE__);
            }
            else
//            {
                debug(10, "smart end node lists");
//            if (node_lists.size()) out.emit(RenXt_NODELIST_END); //finish off last list - redundant (end of packet anyway)
//#ifndef SPLITABLE
//                if (sortpal.size(false) > 1)
//                /*if (node_lists.size() > 1)*/ out.emit_opc(RENXt_NODELIST_END); //end of lists
//                out.EndOpcode(__LINE__);
//#endif
//            }
//            debug(1, "sent %d bytes for dumb evt list", out.used - svused);
//            svused = out.used;
#if 0 //firmware/comm DEBUG
            out.emit(RENXt_ACK); //check listener/packet status
            out.emit(0);
#endif
#if 0 //firmware/comm DEBUG
            start_pkt(out, adrs); //kludge: split packet in case bad rcv
#endif // 1
//            if (!dumb /*|| sortpal.size()*/)
            out.BeginOpcode(adrs, +100);
            out.SetOpcodeDuration(-1); //execution time depends on hardware so set it to max
            out.emit_opc(RENXt_NODEFLUSH); //flush changes; must be last opcode for this processor
            if (dumb) debug(10, "sent %d bytes for dumb evt list", out.used - svused);
            if (dumb) svused = out.used;
            continue;
        }

//NO #pragma message WARN("TODO: send in reverse controller order?")
//send out 1/2/4bpp bitmap:
//        out.emit(bmplen / ram_scale);
        checkpoint(__LINE__);
        int curbyte = 0, ppb = 8 / ABS(prop->enc_type); //2/4/8 pixels per byte (6:1, 12:1, or 24:1 base compression rate)
        size_t limit = 4 * ppb * divup(MIN(prop->desc.numnodes - stnode, prop->desc.ctlrnodes), ppb * 4); //bump loop limit to finish last byte
//        if (prop->desc.isparapal) limit = 4 * divup(MIN(prop->desc.numnodes - stnode, prop->desc.ctlrnodes), 4); //4-way parallel node groups
        size_t split_stofs = out.used, split_len = -1;
#ifdef SPLITABLE
//        split_len = std::max<size_t>(divup(limit, ppb), SPLITABLE); //node groups are 2 bytes each
//        while (split_len > SPLITABLE) split_len >>= 1; //if we need to split it, try to make even chunks
        split_len = divup(divup(limit, ppb), SPLITABLE); //#chunks to split into
        split_len = divup(divup(limit, ppb), split_len); //try to make chunk size fairly even
#endif // SPLITABLE
        debug(5, "send %dbpp bitmap, ppb %d, #nodes %d scale 4, actual bmplen %d, split len %d", prop->enc_type, ppb, MIN(prop->desc.numnodes - stnode, prop->desc.ctlrnodes), limit, split_len);
//#if 1
//        out.emit_raw(RENARD_PAD); //kludge: Gdoor is dropping a byte occasionally, so try to avoid that here be inserting a benign pad byte so that palette will be received correctly
//#endif
        out.BeginOpcode(adrs, +1);
        out.emit_opc(RENXt_BITMAP(prop->enc_type BPP));
//NOTE: firmware does a NodeFlush after receiving the bitmap, so wait states are needed following the bitmap
//        debug(10, "has data: Bitmap");
        has_data = true;
//overlapped I/O:
//rcv 2 nodes @4bpp == 44 usec < send 2 nodes == 60 usec (2*30), so start sending immediately
//rcv 4 nodes @2bpp == 44 usec < send 4 nodes == 120 usec (4*30)
//rcv 8 nodes @1bpp == 44 usec < send 8 nodes == 240 usec (8*30)
//        int byte_grps = limit / ppb;
//        if (prop->desc.noderam)
//        out.emit(limit / ppb / 4); //no I/O delay needed because rcv rate is faster than send rate in all cases
//        out.emit(/*hold 1 +*/ divup(prop->desc.noderam * prop->enc_type, 4)); //no I/O delay needed because rcv rate is faster than send rate in all cases
//        out.emit(divup(prop->desc.noderam * prop->enc_type, 4)); //no I/O delay needed because rcv rate is faster than send rate in all cases
//        out.emit(hold + divup(prop->desc.noderam * prop->enc_type, 4)); //no I/O delay needed because rcv rate is faster than send rate in all cases
//1 == 4 bytes == 8/16/32 nodes
//#nodes / ppb / 4
        out.SetOpcodeTerminator(RENARD_SYNC); //NOTE: need to terminate with Sync and stretch length to prevent auto-flush
        out.emit(divup(limit, ppb * 4) + 1); //no I/O delay needed because rcv rate is faster than send rate in all cases; kludge: +1 to prevent auto-flush
        out.emit(0); //TODO: skip first part of bitmap if it didn't change
        split_stofs = out.used; //measure split from first byte in bitmap
//                debug(5, "generated %d inverted node lists for nodes %d..%d", node_lists.size(), stnode, MIN(prop->num_nodes, stnode + nodes_per_ctlr) - stnode - 1);
        for (size_t i = 0, chunklen = 0; i < limit; ++i, chunklen += prop->isparapal? 2: 1)
        {
            if (prop->isparapal) i += 3; //only last node in each node group has color index data
#if 0
            PIX* svpix = prop->lept_pix; prop->lept_pix = 0;
            const byte* svptr = inptr;
            node_value orig_rgb = GetNodeValue(inptr, prop, i + stnode, !i);
            prop->lept_pix = svpix; inptr = svptr;
            int x = (i + stnode) % prop->desc.width, y = (i + stnode) / prop->desc.width;
#endif
//            node_value rgb = bkg;
//            if (i + stnode < prop->desc.numnodes) rgb = GetNodeValue(inptr, prop, i + stnode, !i);
//            if (prop->palette.keys.find(rgb) == prop->palette.keys.end()) { debug(1, "ERROR: rgb 0x%.6x not found in palette", rgb); rgb = bkg; }
//            int svbyte = curbyte;
            size_t inx = (i < prop->nodeinx.size())? prop->nodeinx[i]: 0; //prop->palette[0].second; //bkg color
//            if (inx >= sortpal.size(true)) { debug(1, "WHOOPS: node[%d/%d] points to bad palent %d/%d", i, prop->nodeinx.size(), inx, sortpal.size(true)); inx = 0; }
//            inx = sortpal[inx].inx;
//            if (inx >= sortpal.size(false)) { debug(1, "WHOOPS: node[%d/%d] points to bad palent %d/%d", i, prop->nodeinx.size(), inx, sortpal.size(false)); inx = 0; }
            inx = prop->palette.Reord(inx);
#if 0
            std::stringstream status;
            for (int fido = 0; fido < 10; ++ fido)
            {
                if (fido + 1 == 10) { status << " LOOP"; break; }
                if ((inx < 0) || (inx >= prop->palette.size())) { status << inx << " BAD"; break; }
                if (prop->palette[inx].IsDeleted())
                {
                    inx = prop->palette[inx].inx;
                    continue;
                }
                if (prop->palette[inx].inx == inx)
                {
                    status << " -> " << inx << "!";
                    break;
                }
                status << " -> " << inx;
                inx = prop->palette[inx].inx;
                break;
            }
#endif
#if 0
            std::vector<int> link_stack;
            while (prop->palette[inx].IsDeleted())
            {
                status << " -> (" << inx << ")";
                link_stack.push_back(inx);
                inx = prop->palette[inx].inx;
            }
            status << " -> " << inx;
            while (!link_stack.empty())
            {
                prop->palette[link_stack.back()].inx = inx;
                link_stack.pop_back();
            }
            inx = prop->palette[inx].inx;
            status << " = " << inx;
#endif // 0
            checkpoint(__LINE__);
            debug(90, "enc node[%d/%d] inx %d", i, limit, inx); //status.str().c_str() + 4); //inx, prop->palette[inx].inx, prop->palette[inx].IsDeleted());
            curbyte <<= prop->enc_type;
//            debug(90, "bitmap %dbpp: shift in node[%d] (%d,%d) 0x%.6x -> 0x%.6x, inx %d, flush? %d", prop->enc_type, i + stnode, x, y, orig_rgb, rgb, (rgb != bkg)? prop->palette.keys[rgb]: -1, !((i + 1) % ppb));
            /*if (rgb != bkg)*/ curbyte |= /*prop->palette[prop->palette.keys[rgb]].*/ inx; //prop->palette[prop->palette.keys[rgb]]->inx;
//            debug(14, "bitmap enc[%d/%d]: cur byte 0x%x | node %d => byte 0x%x, flush? %d", i, limit, svbyte, palent.inx, curbyte, !((i + 1) % ppb));
            if ((i + 1) % ppb) continue; //don't have all the bits for this byte yet
#ifdef SPLITABLE
            if (out.used - split_stofs >= split_len) //split into smaller bitmaps to allow pad or rexmit; excl "=" to allow stofs slop
                if (!((out.used - split_stofs) % 4)) //CAUTION: must split on quad-byte boundary
                {
//                out.EndOpcode(__LINE__);
//                out.emit_raw(RENARD_PAD);
//                out.start_pkt(__LINE__, adrs);
//                    out.terminate_pkt(adrs); //cancel Bitmap prematurely
                    out.BeginOpcode(adrs, +0); //split into multiple opcodes for easier re-send
                    out.emit_opc(RENXt_BITMAP(prop->enc_type BPP)); //start another bitmap
                    out.SetOpcodeTerminator(RENARD_SYNC); //NOTE: need to terminate with Sync and stretch length to prevent auto-flush
                    out.emit(divup(limit - i, ppb * 4) + 1); //no I/O delay needed because rcv rate is faster than send rate in all cases; kludge: +1 to prevent auto-flush
//NO: firmware not using quad-bytes                    out.emit(i / ppb / 4); //resume bitmap at next quad byte
                    out.emit(divup(i / ppb, /*4*/ 1)); //resume bitmap at next quad byte
                    split_stofs = out.used;
                }
#endif // SPLITABLE
//1 bpp: 2468 1357 => 0008 0007, 0006 0005, 0004 0003, 0002 0001
//2 bpp: 2244 1133 => 0044 0033, 0022 0011
            if (prop->isparapal) //2 bytes per node group
            {
                out.emit_uint16(prop->palette[inx].key);
                continue;
            }
            if (prop->enc_type == 1) //8765 4321 =-> 2468 1357
                curbyte = ((curbyte & 0x80) >> 3) | ((curbyte & 0x40) >> 6) | (curbyte & 0x20) | ((curbyte & 0x10) >> 3) | ((curbyte & 0x08) << 3) | (curbyte & 0x04) | ((curbyte & 0x02) << 6) | ((curbyte & 0x01) << 3);
            else if (prop->enc_type == 2) //4433 2211 => 2244 1133
                curbyte = ((curbyte & 0xc0) >> 2) | ((curbyte & 0x30) >> 4) | ((curbyte & 0x0c) << 4) | ((curbyte & 0x03) << 2);
            out.emit(curbyte); curbyte = 0; //flush current group of nodes
        }
//        out.terminate_pkt(adrs); //kludge: cancel Bitmap prematurely to inhibit auto-flush
//        out.BeginOpcode(__LINE__, adrs, 1, -1); //wait states depend on hardware so set it to max
        out.BeginOpcode(adrs, +100);
        out.SetOpcodeDuration(-1); //execution time depends on hardware so set it to max
        out.emit_opc(RENXt_NODEFLUSH); //flush changes; must be last opcode for this processor
        debug(10, "sent %d bytes for %dbpp bitmap, %d nodes (%d..%d), ppb %d, limit %d", out.used - svused, prop->enc_type, prop->desc.numnodes, stnode, MIN(prop->desc.numnodes, stnode + prop->desc.ctlrnodes - 1), ppb, limit);
//BITMAP opcode will flush if scaled length is set correctly    out.emit(RENXt_NODEFLUSH); //flush changes
//TODO: reduce bitmap bpp and convert infrequent entries to inverted node lists to reduce palette size
#if 0
        out.emit(RENXt_ACK);
        out.emit(0);
#endif
#if 0
        start_pkt(out, adrs); //kludge: split packet in case bad rcv
        out.emit(RENXt_NODEFLUSH); //flush changes
#endif // 1
    }
    return has_data;
}


//encode Renard nodes (channel triplets) into RenardXt format:
//parameters:
// inbuf = raw Renard channels (bytes); RGB nodes are triplets in R, G, B order; monochrome nodes are 1 byte
// proplen = null-terminated list of prop sizes; each prop is a separate controller (with a distrinct address); first is shared palette
// outbuf = RenardXt-encoded byte stream; should use one per COM port
// outlen = size of outbuf; must be <= max #bytes that can be sent at given baud rate and frame rate
// pad_rate = how often to send pad char (0 for no padding)
//return value:
// actual size of outbuf used (# bytes)
//IFCPP(extern "C")
#define AUTO_DETECT_LEN  4 //#Sync bytes to send for baud rate auto-detect
//IFCPP(extern "C")
static byte svinbuf[8200];

int RenXt_encode(const /*byte*/ void* inbuf, const /*byte*/ void* prev_inbuf, size_t inlen, const RenXt_Prop* propdesc, RenXt_Stats* stats, byte* outbuf, size_t outlen, int pad_rate, int seqnum)
{
#define rpt_error()  ++stats->num_error; stats->error_frame = seqnum
    const byte* inptr = (const byte*)(const void*)inbuf; //kludge: gcc not allowing inline cast to l-value, so use alternate var
    MyOutStream out(outlen, pad_rate);
//    const int* svproplen = proplen;
    std::vector<PropInfo> props; //private palette and encoding info for each prop
//    Palette shared_palette;
    RenXt_errors = 0;
    ++stats->num_encodes;
    stats->enc_frame = seqnum;
    stats->total_inbytes += inlen;
    if (!seqnum) time(&stats->started);
//TODO: use non-empty send count rather than seqnum
    if (!(seqnum % 20)) { time_t now; time(&now); stats->elapsed = now - stats->started; }
    memcpy(svinbuf, inbuf, std::min<int>(inlen, sizeof(svinbuf)));

    checkpoint(-1);
    checkpoint(__LINE__);
//    int numprops = 0;
    if (!propdesc || !propdesc->numnodes) { rpt_error(); return error("No input at frame %d?", seqnum), -1; } //no input?
    for (const RenXt_Prop* pptr = propdesc;; ++pptr) //, ++numprops);
        if (!pptr->numnodes) //end of list
        {
            props.resize(pptr - propdesc /*+ 1*/); //allocate all entries at same time; first entry is for shared palette
            break;
        }
    debug(1, "RenXt_encode(inbuf 0x%x, prev 0x%x ('%d), inlen %d, propdesc %d:{%d,%d,%d,...}, outbuf 0x%x, outlen %d, pad_rate %d, seqnum %d), debug level %d", inbuf, prev_inbuf, prev_inbuf? (const byte*)prev_inbuf - (const byte*)inbuf: -1, inlen, props.size(), propdesc[0].numnodes, (props.size() > 0+1)? propdesc[1].numnodes: -1, (props.size() > 1+1)? propdesc[2].numnodes: -1, outbuf, outlen, pad_rate, seqnum, RenXt_debug_level);
//#define TRAP_FRAME  220
    int svlevel = RenXt_debug_level;
#ifdef TRAP_FRAME
    if (seqnum == TRAP_FRAME) RenXt_debug_level = -99;
#endif // 1

//first pass: analyze nodes for each prop, generate palette and choose encoding type:
//    int ctlr = 0; //next prop/controller address
//    props.resize(numprops - 1); //numprops = 0;
    for (auto prop = props.begin() /*+ 1*/; prop != props.end(); ++prop, ++propdesc) //while (*++proplen != -1)
    {
        prop->inptr = inptr; //save node data ptr for second pass
        prop->desc = *propdesc; //width, height, node_type, numnodes, num_ctlr
        if (prop->desc.node_type & 0xF0) prop->desc.node_type >>= 4; //kludge: adjust param to internal format
//        prop->desc.age = -1; //no cached data yet
//        if (prop->desc.ctlrnodes < 1) prop->desc.ctlrnodes = ABS(prop->desc.numnodes); //determines how channels for this prop are spread across controllers
        prop->enc_type = PropInfo::None;
        prop->paltype = RenXt_Stats::PaletteTypes::Normal; //default
        int nodesize = IsDumb(prop->desc.node_type)? 1: 3;
        IFDEBUG(PropInfo* prop_debug = &*prop; debug(5, "analyze prop[%d] '%s': type 0x%x, dumb? %d, %d nodes, max %d nodes/ctlr, order 0x%x, frameset '%s'", prop - props.begin(), prop_debug->desc.propname, prop_debug->desc.node_type, IsDumb(prop_debug->desc.node_type), prop_debug->desc.numnodes, prop_debug->desc.ctlrnodes, prop_debug->desc.order, prop_debug->desc.frameset)); //helps debugger with data types
//        int prevadrs = /*(prop != props.begin())?*/ (prop - 1)->address; //: 0; //props.size()? props.back().address: 0; //prev controller address
//        props.resize().push_back();
//        PropInfo& prop = props[numprops++]; //props.back();
//assign next controller address:
//        prop->datasize = 0;
        checkpoint(__LINE__);
        prop->staddr = (prop == props.begin())? 1: prop[-1].staddr + divup(ABS(prop[-1].desc.numnodes), prop[-1].desc.ctlrnodes); //assign next sequential address
//        while (IsRenardSpecial(prop->address)) ++prop->address; //don't use special values for controller addresses (want a single byte with no esc codes)
        if (prop->staddr + divup(ABS(prop->desc.numnodes), prop->desc.ctlrnodes) >= /*0xFF*/ RENARD_SPECIAL_MIN) { rpt_error(); return error("Too many controllers in frame %d (8-bit address exhausted): prop %d/%d, stadrs 0x%x, #ctlr %d, numnodes %d", seqnum, prop - props.begin(), props.size(), prop->staddr, divup(ABS(prop->desc.numnodes), prop->desc.ctlrnodes), prop->desc.numnodes), -1; }
//extract palette:
//        int shared_bpp = 0;
//        prop->node_type = propdesc->node_type;
//        prop->num_nodes = (prop->desc.numnodes > 0)? prop->desc.numnodes / 3: 0; //propdesc->numnodes; //channels vs. raw/macro data; assume RGB channels
//        prop->width = propdesc->width;
//        prop->height = propdesc->height;
//        prop->cpyptr = inptr;
//        prop->cpylen = ABS(propdesc->numnodes);

//caching (optional):
        const byte* prevptr = prev_inbuf? (const byte*)prev_inbuf + (prop->inptr - (const byte*)inbuf): 0;
        int cpylen = ABS(prop->desc.numnodes) * 3; //nodesize;
//        for (int i = 0; i < cpylen; ++i)
//            if (inptr[i] != prevptr[i]) { debug(10, "diff '%d: ^0x%x 0x%x != prev '%d: ^0x%x 0x%x, checking %d bytes", &inptr[i] - (const byte*)inbuf, &inptr[i], inptr[i], &prevptr[i] - (const byte*)inbuf, &prevptr[i], prevptr[i], cpylen); break; }
//            else if (i + 1 == cpylen) debug(1, "NO DIFF %d bytes", cpylen);
        if (seqnum && prevptr && !memcmp(inptr, prevptr, cpylen)) //node data did not change from last time; no need to resend (nodes are persistent)
        {
            debug(4, "dedup %d: prop[%d] '%s', adrs 0x%x.., #channels %d * %d, no change for seq# %d: '%d == '%d", cpylen, prop - props.begin(), prop->desc.propname, prop->staddr, prop->desc.numnodes, nodesize, seqnum, inptr - (const byte*)inbuf, prevptr? prevptr - (const byte*)inbuf: -1);
//            prop->enc_type = PropInfo::None;
//            prop->cpylen = 0; //no need to update delta cache
            prop->datasize = 0;
#if 1
            inptr += cpylen; //just skip over these channels
            continue;
#endif
        }
        checkpoint(__LINE__);
        debug(10, "dedup %d: prop[%d] '%s', adrs 0x%x.., #channels %d * %d, '0x%x -> '0x%x changed for seq# %d", cpylen, prop - props.begin(), prop->desc.propname, prop->staddr, prop->desc.numnodes, nodesize, inptr - (const byte*)inbuf, prevptr? prevptr - (const byte*)inbuf: -1, seqnum);
        if (prevptr) memcpy((non_const byte*)prevptr, inptr, cpylen);
        inptr += cpylen;

//        if (IsDumb(prop->desc.node_type)) AnalyzeDumb(&*prop, prop - props.begin());
        checkpoint(__LINE__);
        /*else*/ prop->AnalyzeNodes(prop - props.begin(), stats);
    }

//second pass: apply additional compression if needed:
    for (;;)
    {
        size_t total_datasize = 0; //out.used; //NOTE: might already be padded, but assume not to account for incomplete/partial padding
        for (auto prop = props.begin() /*+ 1*/; prop != props.end(); ++prop)
            total_datasize += prop->datasize;
        if (pad_rate) total_datasize += divup(total_datasize, pad_rate);
        total_datasize += divup(3 * total_datasize, 256); //kludge: assume uniform byte distribution for simpler estimate of esc codes (3/256)
        debug(5, "total estimated data: %d vs. outbuf %d, need more compression? %d", total_datasize, outlen, (total_datasize > outlen * 9/10));
        if (total_datasize <= outlen * 9/10) break;
//apply more compression:
#if 0
        PropInfo* worst_prop = 0;
        for (auto prop = props.begin(); prop != props.end(); ++prop)
        {
            if (prop->enc_type == Inverted) continue;
        }
#endif
        error("TODO: apply more compression in frame %d: est outlen %d vs. max %d", seqnum, total_datasize, outlen);
        break;
    }

    checkpoint(__LINE__);
    out.emit_raw(RENARD_SYNC); //kludge: send extra Sync at start in case processor is too busy to catch it the first time
#if 1 //inject an increasing number to make log/trace debug easier
//    out.start_pkt(__LINE__, 0xfe);
    out.BeginOpcode(0xfe, 0); //kludge: use slot 0 rather than allocating 254 entries; this will also put it first, making it easier to see in debug
//    out.emit(RENXt_SETPAL(1)); //dummy envelope
    out.emit_uint16(seqnum);
//    out.emit(0);
//    out.emit_rgb(prop->palette[palent->second].key);
//
#endif // 0
#ifdef ZOMBIE_RECOVER //reset zombied controllers
//TODO: use non-empty send count rather than seqnum
    if (!(seqnum % ZOMBIE_RECOVER))
    {
        out.emit_raw(RENARD_SYNC);
        out.emit(ADRS_ALL);
//        out.emit(RENXt_RESET);
//        out.emit(~RENXt_RESET);
        out.emit(0x72); out.emit(0x40); out.emit(0x74); out.emit(6); out.emit(255); //reset addresses
    }
#endif
#if 0
    start_pkt(out, 0xfe);
    out.emit(RENXt_SETPAL(1)); //dummy envelope
    out.emit_int16(seqnum);
    out.emit(0);
//    out.emit_rgb(prop->palette[palent->second].key);
#endif // 0

//encode node data for all props and update dedup cache:
    int need_to_send = 0; //false;
    for (auto prop = props.begin() /*+ 1*/; prop != props.end(); ++prop)
//    for (auto prop = props.end() - 1 /*+ 1*/; prop != props.begin() - 1; --prop)
//    {
//        IFDEBUG(PropInfo* prop_debug = &*prop; prop_debug->desc.numnodes = prop->desc.numnodes);
//        if (IsDumb(prop->desc.node_type)) EncodeDumb(out, &*prop, prop - props.begin());
        /*else*/ if (EncodeNodes(out, &*prop, stats, prop - props.begin(), seqnum)) need_to_send = prop - props.begin() + 1; //true;
//    }
//    need_to_send -= out.used;
    out.EndOpcode();
    if (out.used) out.emit_raw(RENARD_SYNC, 2); //kludge: send out final sync pair to mark end of last packet; this forces controllers back into a known state for next packet, in case any were left part way thru parsing an opcode
//    int padlen = (out.used && (out.used < outlen))? outlen - out.used: 0;
//    if (padlen) out.emit(0, outlen - out.used); //kludge: force subsequent frames to be spread out; otherwise PICs might still be processing previous frame when next is sent if they are too short
    int wantpad = (out.used && (out.used < 200))? 200 - out.used: 0;
    if (!need_to_send)
    {
        debug(10, "no need to send %d bytes", out.used);
        ++stats->null_frames;
        return 0;
    }

#define WANT_OPCODE_INTERLEAVE
#ifdef WANT_OPCODE_INTERLEAVE //interleave opcodes to overlap wait states
 #pragma message WARN("interleaving opcodes")
    MyOutStream& wantout = out;
 #define out  xx //kludge: catch mistakes
    checkpoint(__LINE__);
    std::vector<size_t> opclen_byadrs(wantout.opcodes.size()); //total size of opcodes for each adrs; maybe keep opcodes in relative order by adrs?
//    size_t has_opc = 0; //#adrs with pending opcodes
//    std::vector<MyOutStream::Opcode*> allopc;
    size_t numadrs = 0, numopc = 0, opctotlen = 0, max_opc = 0;
    for (byte adrs = 0; adrs < wantout.opcodes.size(); ++adrs)
    {
        opclen_byadrs[adrs] = -opctotlen;
        for (auto it = wantout.opcodes[adrs].begin(); it != wantout.opcodes[adrs].end(); ++it)
            opctotlen += it->enclen;
        if (opclen_byadrs[adrs] += opctotlen) ++numadrs;
        if (wantout.opcodes[adrs].size() > max_opc) max_opc = wantout.opcodes[adrs].size();
        numopc += wantout.opcodes[adrs].size(); //total #opcodes
    }
//    showbuf("raw opcodes from first pass:", &wantout.frbuf[0], wantout.used, false);
    debug(10, "opc prior to interleave: %u adrs, %u opcodes, max opc %u, %u total len", numadrs, numopc, max_opc, opctotlen);
    for (auto itslot = wantout.opcodes.begin(); itslot != wantout.opcodes.end(); ++itslot)
        for (auto itopc = itslot->begin(); itopc != itslot->end(); ++itopc)
            debug(20, "adrs[%d/%d] x%x, opc[%d/%d] stofs x%x, enclen %d, data ofs %d, duration %d, terminator x%x, opcodes: x%x x%x x%x x%x x%x ...", itslot - wantout.opcodes.begin(), wantout.opcodes.size(), itopc->adrs, itopc - itslot->begin(), itslot->size(), itopc->stofs, itopc->enclen, itopc->data_ofs, itopc->duration, itopc->ends_with, safe_byte(wantout.frbuf, itopc->stofs, wantout.frbuf.size()), safe_byte(wantout.frbuf, itopc->stofs + 1, wantout.frbuf.size()), safe_byte(wantout.frbuf, itopc->stofs + 2, wantout.frbuf.size()), safe_byte(wantout.frbuf, itopc->stofs + 3, wantout.frbuf.size()), safe_byte(wantout.frbuf, itopc->stofs + 4, wantout.frbuf.size()));
//    std::vector<Opcode*> scheduled_opc; //execution order after interleaving opcodes
//    scheduled_opc.reserve(numopc); //reduce memory mgmt overhead
//    std::vector<__gnu_cxx::__normal_iterator<MyOutStream::Opcode*, std::vector<MyOutStream::Opcode>>> opc_pending(numadrs);
//TODO: sort by length; send adrs with most data first? doesn't help much, and need to preserve adrs order anyway
    class IOReq
    {
    public:
//        byte adrs;
        size_t stofs;
//        int len; //> 0 => literal, < 0 => varying data, == 0 => eof
        size_t wrlen, cmplen, duration;
        byte adrs, ends_with;
    public:
        IOReq(byte adrs, size_t stofs, size_t wrlen, size_t cmplen, size_t duration, byte ends_with): stofs(stofs), wrlen(wrlen), cmplen(cmplen), duration(duration), adrs(adrs), ends_with(ends_with) {}
    };
//    std::vector<byte> outbytes;
    std::vector<IOReq> ioreq;
    size_t flush_opcofs = 0; //start of flush opcodes; these must come last and are contingent upon successful completion of earlier opcodes
    MyOutStream interleaved(outlen, 0); //padding was NOT handled in previous pass
    interleaved.emit_raw(RENARD_SYNC); //kludge: put an extra Sync at beginning in case processors were not paying attention
    std::vector<size_t> opc_pending(wantout.opcodes.size()); //numadrs);
//    for (int i = 0; i < opc_pending.size(); ++i)
//        if (opc_pending[i]) { error("needed init"); break; }
//    for (int i = 0; i < opc_pending.size(); ++i)
//        opc_pending[i] = 0;
    max_opc += 4; //kludge: allow a few extra times around the loop
//    ioreq.emplace_back(svofs, outbytes.used - svofs); svofs = outbytes.used;
//rearrange opcodes to use interleave as wait states:
//schedule opcodes to be executed, reorder to overlap + minimize wait states:
//    size_t prev_adrs = -1;
    while (numadrs) //>= 1 adrs has more opcodes to schedule
    {
        if (!max_opc--) { error("inf loop?"); break; } //paranoid check
        checkpoint(__LINE__);
        size_t min_delay = -1; //controls how long to delay earliest delayed opcode
        for (size_t slot = 0; slot < wantout.opcodes.size(); ++slot)
        {
//            bool need_adrs = true; //emit adrs when switching processors
            for (auto it = wantout.opcodes[slot].begin() + opc_pending[slot]; it != wantout.opcodes[slot].end(); ++it, ++opc_pending[slot])
            {
//                bool is_last = /*(adrs+1 == wantout.opcodes.size()) &&*/ (it + 1 == wantout.opcodes[adrs].end()); //NOTE: don't need special case for last (flush) if there's only one opcode
//                if ((it != wantout.opcodes[adrs].begin()) && ((it - 1)->intlv_stofs == (size_t)-1)) error("prev opcode %d not scheduled", it - wantout.opcodes[adrs].begin() - 1);
//                size_t delay_until = (it->waitst && (it != wantout.opcodes[adrs].begin()))? (it - 1)->intlv_stofs + (it - 1)->enclen + it->waitst: 0;
//                bool is_last = (it + 1 == wantout.opcodes[slot].end()); //flush opcode comes last, depends on all previous opcodes
//                size_t debug_which = it - wantout.opcodes[slot].begin();
                size_t opc_sep = 0; //((it->adrs != prev_adrs) && (it != wantout.opcodes[slot].begin()))? 1+1: ((it - 1)->ends_with == RENARD_SYNC)? 1+1: (it - 1)->ends_with? 1: 0; //#bytes separating prev opcode from next
                if (ioreq.size() && ioreq.back().ends_with && (ioreq.back().ends_with != RENARD_SYNC)) ++opc_sep;
                if (!ioreq.size() || (ioreq.back().adrs != it->adrs) || (ioreq.back().ends_with == RENARD_SYNC)) opc_sep += 2;
//                bool is_first = (it == wantout.opcodes[slot].begin()); //first opcode for this processor
//                if (it->adrs != prev_adrs) //need to insert Sync + adrs, possibly other terminator
//                {
//                    if (!is_first && (it - 1)->ends_with && ((it - 1)->ends_with != RENARD_SYNC)) ++opc_sep;
//                    opc_sep += 2;
//                }
//                else //only need to ins separator
//                {
//                    if (!is_first && (it - 1)->ends_with && ((it - 1)->ends_with != RENARD_SYNC)) opc_sep += 2;
//                }
                size_t delay_until = (it != wantout.opcodes[slot].begin())? (it - 1)->intlv_stofs + (it - 1)->enclen + (it - 1)->duration: 0; //when previous opcode finished execution
                bool defer_flush = (it + 1 == wantout.opcodes[slot].end()) && (wantout.opcodes[slot].size() > 1) && !flush_opcofs; //it->duration == (size_t)-1); //don't need to defer singletons to flush group
//TODO: just ins noop if waitst < +2
//                if ((delay_until > outbytes.used + (need_adrs? 2: 0)) || (is_last && (wantout.opcodes[adrs].size() > 1) && !flush_opcofs)) //can't execute this opcode yet; try to interleave other opcodes instead of just waiting; CAUTION: assumes last opcode is a flush
                if ((delay_until > interleaved.used + opc_sep) || defer_flush) //need to delay this opcode until previous finishes
                {
                    debug(20, "opc delay: postpone adrs x%x, opc[%u/%u], old stofs x%x, enc len %u, data ofs %d, duration %d delays until +%u=%u vs. %u now, defer flush? %d", it->adrs, it - wantout.opcodes[slot].begin(), wantout.opcodes[slot].size(), it->stofs, it->enclen, it->data_ofs, it->duration, delay_until - (interleaved.used + opc_sep), delay_until, interleaved.used + opc_sep, defer_flush);
//                    if ((it->waitst == (size_t)-1) && !is_last) error("node flush wasn't last opcode for adrs x%x", adrs);
                    if (delay_until < min_delay) min_delay = delay_until;
                    break; //interleave: defer remaining opcodes until after opcodes for other processors
                }
#if 0 //not correct, and not needed since opcodes were already split  def SPLITABLE
                if (it->enclen + outbytes.used - svofs > SPLITABLE) //split into resendable segments
                {
                    if (svofs) //flush previous
                    {
                        debug(20, "opc encode: size split at adrs x%x, opc[%u/%u]", adrs, it - wantout.opcodes[adrs].begin(), wantout.opcodes[adrs].size());
                        outbytes.emit_raw(adrs? adrs: 0xfe);
                        ioreq.emplace_back(svofs, outbytes.used - svofs, outbytes.used - svofs);
//#if 0 //Pad doesn't help?
                        outbytes.emit_raw(RENARD_PAD);
//#endif // 0
                        svofs = outbytes.used;
                        need_adrs = true;
                    }
                }
#endif // SPLITABLE
                checkpoint(__LINE__);
//                if (opc_sep) //(adrs != prev_adrs) && (wantout.frbuf[it->stofs] != RENARD_SYNC))
//                {
//#if 1 //an occasional Pad might help; unknown, but seems like a good idea to put one before each new set of opcodes
//                    if (ioreq.size() && !flush_opcofs) outbytes.emit_raw(RENARD_PAD); //RENXt_NOOP); //kludge: distribute a little timing slop between processors
//                    else if ((min_waitst == (size_t)-1) && is_last && ioreq.size()) { outbytes.emit_raw(RENARD_PAD); ++min_waitst; } //this one is mainly for easier debug of trace/log
//#endif // 0
//                    if (it->ends_with == RENARD_SYNC)
//                    {
//                        interleaved.emit_raw(RENARD_SYNC);
//                        interleaved.emit(it->adrs); //need to re-adrs processor after Sync
//                        prev_adrs = it->adrs;
//                    }
//                    else interleaved.emit(it->ends_with);
//                }
//                need_adrs = false;
                if (ioreq.size()) //check if prev opcode needs terminator
                    if (ioreq.back().ends_with && (ioreq.back().ends_with != RENARD_SYNC))
                    {
                        interleaved.emit(ioreq.back().ends_with); //terminate prev opcode (probably not really necessary since Sync will cancel it anyway)
//                        if (ioreq.back().cmplen != -1) error("terminated opcode can't have data");
                        ++ioreq.back().wrlen; //= interleaved.used - ioreq.back().stofs;
//                        ++it->intlv_stofs; //= interleaved.used; //delayed/dependent opcodes need new execution schedule to resolve wait states correctly; save new opcode start
                    }
                it->intlv_stofs = interleaved.used; //delayed/dependent opcodes need new execution schedule to resolve wait states correctly; save new opcode start
                if (!ioreq.size() || (ioreq.back().adrs != it->adrs) || (ioreq.back().ends_with == RENARD_SYNC))
                {
                    interleaved.emit_raw(RENARD_SYNC);
                    interleaved.emit(it->adrs); //need to re-adrs processor after Sync
                }
                interleaved.emit_rawbuf(&wantout.frbuf[it->stofs], it->enclen);
//                if ((it->data_ofs == (size_t)-1) && !it->waitst && !is_last) continue; //flush for readback verify or to use interleave as wait state; NOTE: wait state does not really need to be flushed, but it might interfere with next adrs so flush anyway
                debug(20, "opc encode: flush at adrs x%x, opc[%u/%u], old stofs x%x, new stofs x%x, enc len %u, data ofs %d, duration %d, delay until %d vs. cur delay st %u, is last? %d", it->adrs, it - wantout.opcodes[slot].begin(), wantout.opcodes[slot].size(), it->stofs, it->intlv_stofs, it->enclen, it->data_ofs, it->duration, delay_until, interleaved.used, it + 1 == wantout.opcodes[slot].end());
//                char desc[80];
//                sprintf(desc, "opc bytes[%u..%u] => %u..%u", it->stofs, it->stofs + it->enclen - 1, svofs, outbytes.used - 1);
//                showbuf(desc, &wantout.frbuf[it->stofs], it->enclen, false);
                size_t cmplen = interleaved.used - it->intlv_stofs;
                if (it->data_ofs != (size_t)-1) cmplen += -it->enclen + it->data_ofs; //exclude ret data from verify compare
                ioreq.emplace_back(it->adrs, it->intlv_stofs, interleaved.used - it->intlv_stofs, cmplen, it->duration, it->ends_with);
//#if 0 //Pad doesn't help?
//                if (outbytes.used - svofs > 4) outbytes.emit_raw(RENARD_PAD); //don't bother padding between short opcodes
//#endif // 0
//                svofs = outbytes.used;
//                if (it->waitst == (size_t)-1) { if (!flush_opcofs) flush_opcofs = ioreq.size(); continue; } //wait state won't be satisfied until next frame
                if (it + 1 == wantout.opcodes[slot].end()) --numadrs; //continue; } //keep track of remaining processors
//                else if ((it + 1)->waitst) (it + 1)->waitst += outbytes.used; //update with actual start time
//                debug(20, "reducing wait states on remaining %d opcodes by %d", wantout.opcodes[adrs].end() - it - 1, ioreq.back().enclen);
//                for (auto itdep = it + 1; itdep != wantout.opcodes[adrs].end(); ++itdep) //reduce remaining wait states by length of this opcode
//                    itdep->waitst = (itdep->waitst <= outbytes.used)? 0: itdep->waitst - ioreq.back().enclen;
//                if (!it->waitst) continue;
//                if (it->waitst == (size_t)-1) //NOTE: h/w flush should have been last opcode, but do additional check just in case
//                {
//                    error("node flush wasn't last opcode for adrs x%x", adrs);
//                    continue;
//                }
//                if (it+1 == wantout.opcodes[adrs].end() continue;
//                opc_pending.emplace_back(adrs, it - wantout.opcodes[adrs].begin());
//                want_more = true;
//                if (outbytes.used + it->waitst < min_waitst) min_waitst = outbytes.used + it->waitst; //next opcode can't be executed until after this delay
//                opc_pending[adrs] = ++it; //update resume ptr since we're jumping out of loop prematurely
//                break; //interleave: defer remaining opcodes until after other processors
            }
        }
        checkpoint(__LINE__);
        debug(20, "opc postponed: return to %d defered opcode sets, min delay %d, current delayed state %d, flush opc ofs %d", numadrs, min_delay, interleaved.used, flush_opcofs);
        if (!numadrs) break;
        /*if (!flush_opcofs)*/ flush_opcofs = ioreq.size(); //assume last round of opcodes are flushes
        if (min_delay <= interleaved.used) continue; //no wait state or it was satisfied by opcode interleave
//        if (min_waitst == (size_t)-1) { if (!flush_opcofs) flush_opcofs = ioreq.size(); continue; } //wait state won't be satisfied until next frame
//        {
//            for (auto it = opc_pending[adrs]; it != wantout.opcodes[adrs].end(); opc_pending[adrs] = ++it)
//            {
//                checkpoint(__LINE__);
//                if (it->waitst > outbytes.used) //can't execute this one yet; try to interleave other opcodes instead of just waiting
//        }
        debug(20, "opc encode: append %u-%u=%u bytes for wait state to opc# %u", min_delay, interleaved.used, min_delay - interleaved.used, ioreq.size());
        checkpoint(__LINE__, min_delay, interleaved.used, min_delay - interleaved.used);
        if (min_delay - interleaved.used > 1000) { error("bad len: %u - %u", min_delay, interleaved.used); break; }
        ioreq.back().wrlen += min_delay - interleaved.used; //kludge: add null padding to previous opcode
        interleaved.emit(0, min_delay - interleaved.used); //send nulls to delay next opcode
//        ioreq.emplace_back(svofs, outbytes.used - svofs, cmplen);
    }
    size_t padofs = interleaved.used + 2;
    if (ioreq.size() && ioreq.back().ends_with)
    {
        interleaved.emit_raw(ioreq.back().ends_with); //terminate last opcode
        ++ioreq.back().wrlen;
    }
    interleaved.emit_raw(RENARD_SYNC, padofs - interleaved.used); //make sure last opcode is finished; add an extra one in case of dangling Escape
//    showbuf("raw opcodes from first pass:", &wantout.frbuf[0], wantout.used, false);
    debug(20, "final interleaved opc: non-flush opc %d, flush opc %d, total opc bytes %d", flush_opcofs, ioreq.size() - flush_opcofs, interleaved.used);
    checkpoint(__LINE__, ioreq.size());
    for (auto it = ioreq.begin(); it != ioreq.end(); ++it) //for debug only
        debug(20, "opc[%u/%u]: adrs x%x, stofs x%x, len %u, non-data cmp len %u, terminator x%x, is flush? %d, opc: x%x x%x x%x x%x x%x x%x x%x x%x ...", it - ioreq.begin(), ioreq.size(), it->adrs, it->stofs, it->wrlen, it->cmplen, it->ends_with, it - ioreq.begin() >= (int)flush_opcofs, safe_byte(interleaved.frbuf, it->stofs, interleaved.used), safe_byte(interleaved.frbuf, it->stofs + 1, interleaved.used), safe_byte(interleaved.frbuf, it->stofs + 2, interleaved.used), safe_byte(interleaved.frbuf, it->stofs + 3, interleaved.used), safe_byte(interleaved.frbuf, it->stofs + 4, interleaved.used), safe_byte(interleaved.frbuf, it->stofs + 5, interleaved.used), safe_byte(interleaved.frbuf, it->stofs + 6, interleaved.used));
//        showbuf(desc, &outbytes.frbuf[it->stofs], it->wrlen, true);
    size_t ioreq_hdrofs = interleaved.used; //copy I/O req info to outbuf (will be placed ahead of opcodes)
    MyOutStream& final_bytes = interleaved;
//serialize I/O requests before byte stream:
//    size_t data_slew = 3 * sizeof(uint16_t) + 1; //total size of I/O requests; data byte offsets will shift by this amount
    final_bytes.emit_uint16_raw(flush_opcofs); //#non-flush I/O req; each is 8 bytes
    final_bytes.emit_uint16_raw(ioreq.size() - flush_opcofs); //#flush I/O req; each is 6 bytes
    final_bytes.emit_uint32(0); //kludge: pad out to 16 bytes for easier debug
    checkpoint(__LINE__, ioreq.size());
//NOTE: opcode hdrs are stripped out by player before sending
    for (auto it = ioreq.begin(); it != ioreq.end(); ++it)
    {
        final_bytes.emit_uint16_raw(it->stofs);
        final_bytes.emit_uint16_raw(it->wrlen);
        final_bytes.emit_raw(it->cmplen);
        final_bytes.emit_raw(it->adrs);
        final_bytes.emit_raw(it->ends_with);
        final_bytes.emit_raw(it->duration);
    }
    checkpoint(__LINE__, final_bytes.used);
//    final_bytes.emit_rawbuf(&outbytes.frbuf[0], outbytes.used); //avoid redundant mem copy
//    showbuf("interleaved opc bytes", &outbytes.frbuf[0], outbytes.used, false);
 #undef out
#else
    MyOutStream& final_bytes = out;
    size_t ioreq_hdrofs = out.used; //no I/O req info to cpoy
#endif // WANT_OPCODE_INTERLEAVE

    checkpoint(__LINE__);
    size_t bytes_used = ioreq_hdrofs; //#bytes to send to controllers
    if (bytes_used) //final_bytes.used)
    {
        stats->total_outbytes += bytes_used;
        if (bytes_used > outlen) { ++stats->num_ovfl; stats->total_ovfl += bytes_used - outlen; }
        if (seqnum && (bytes_used > stats->max_outbytes)) { stats->max_outbytes = bytes_used; stats->max_occur = 0; }
        if (bytes_used == stats->max_outbytes) { ++stats->max_occur; stats->max_frame = seqnum; }
        if (seqnum && (!stats->min_outbytes || (bytes_used < stats->min_outbytes))) { stats->min_outbytes = bytes_used; stats->min_occur = 0; }
        if (bytes_used == stats->min_outbytes) { ++stats->min_occur; stats->min_frame = seqnum; }
        if (!seqnum) stats->first_outbytes = bytes_used;
//        if (seqnum == 10)
//        {
//            int svlevel = RenXt_debug_level;
//            RenXt_debug_level = -99;
//            showbuf("IN-MAX", svinbuf, std::min<int>(inlen, sizeof(svinbuf)), true);
//            showbuf("MAX-OUT", reinterpret_cast</*const*/ char*>(&out.frbuf[0]), std::min<int>(out.used, outlen), true); //ABS(RenXt_debug_level) >= 20);
//            RenXt_debug_level = svlevel;
//        }
    }
    debug(2, "used %d of %d bytes (%d%%), pad len %d (%d%%), need to send? %d", /*used*/ final_bytes.used, outlen, safe_div(100 * /*out.used*/ final_bytes.used, outlen), wantpad, safe_div(100 * wantpad, final_bytes.used), need_to_send);
    if (final_bytes.used > outlen)
    {
#if 1 //dump bad frame
        char buf[64];
//        int svlevel = RenXt_debug_level;
        RenXt_debug_level = -99;
        sprintf(buf, "IN-MAX[%d]", seqnum);
        showbuf(buf, svinbuf, std::min<int>(inlen, sizeof(svinbuf)), true);
        sprintf(buf, "OUT-MAX[%d]", seqnum);
        showbuf(buf, /*reinterpret_cast</ *const* / char*>(*/ &final_bytes.frbuf[0], std::min<int>(final_bytes.used, outlen), false); //ABS(RenXt_debug_level) >= 20);
#endif // 1
        RenXt_debug_level = svlevel;
        rpt_error(); return error("output buffer overflow: needed %d, only had %d (overflowed by %d) on frame %d", final_bytes.used, outlen, final_bytes.used - outlen, seqnum), -1;
    }
    RenXt_debug_level = svlevel;
    checkpoint(__LINE__);
    memcpy(outbuf, &final_bytes.frbuf[ioreq_hdrofs], final_bytes.used - ioreq_hdrofs); //copy I/O req hdrs first, if any
    memcpy(outbuf + final_bytes.used - ioreq_hdrofs, /*reinterpret_cast<byte*>(*/ &final_bytes.frbuf[0], ioreq_hdrofs); //TODO: avoid this extra memory copy
    if (wantpad) memset(outbuf + final_bytes.used, 0, wantpad);
    return final_bytes.used + wantpad;
}


//alternate version to open port first:
//IFCPP(extern "C")
int RenXt_port_encode(const char* port, const /*byte*/ void* inbuf, const /*byte*/ void* prev_inbuf, size_t inlen, const RenXt_Prop* propdesc, int seqnum)
{
    if (!OpenPorts.Contains(port)) /*OpenPorts.find(port) == OpenPorts.end())*/ return error("Port '%s' not previously opened", port), -1;
    MyPort& port_info = OpenPorts[port]; //creates new entry if not there
    if (!port_info.IsOpen())
        if (!port_info.open()) return -1;
    return RenXt_encode(inbuf, prev_inbuf, inlen, propdesc, &port_info.stats, reinterpret_cast<byte*>(&port_info.io.frbuf[0]), port_info.io.frbuf.size(), port_info.pad_rate, seqnum);
}


//asynchronous read controller memory:
bool RenXt_ReadMem(MyOutStream& out, byte ctlr, uint16_t address, void* buf, size_t rdlen, bool enque, bool first /*= false*/)
{
    if (enque)
    {
        int svlen = out.used;
        if (/*out.used > 20*/ !first) out.StopRead(); //insert kludgey bytes to force clean state after read op
        out.emit_raw(RENARD_SYNC);
        out.emit(ctlr);
        out.emit_opc(RENXt_READ_REG); //read controller memory
        out.emit_uint16(address);
//#ifdef WANT_DEBUG
// #pragma message WARN("TEMP: hard-coded response")
//        if (ctlr < 2) out.emit('A', 2 * len); //placeholder for escaped bytes
//        else
//#endif // WANT_DEBUG
        out.emit(RENXt_NOOP, rdlen + rdlen * 20/100); //placeholder bytes; allow for 20% escaped bytes
        debug(99, "RenXt_ReadMem: %d bytes, caller req %d bytes", out.used - svlen, rdlen);
        return !out.overflow();
    }
    union { byte b; uint16_t i; } retval;
//    byte ch = out.deque();
//    if (ch != RENARD_SYNC) return error("missing sync: ofs %d, got 0x%x for ctlr 0x%x", out.used, ch, ctlr), false;
    std::string debug_buf;
    for (size_t i = 0; (i < 8) && (i < out.used); ++i) { char buf[10]; sprintf(buf, "x%x", out.frbuf[out.rdofs + i] /*, sizeof(buf), 16*/); if (!debug_buf.empty()) debug_buf += ", "; debug_buf += buf; }
    if (debug_buf.empty()) debug_buf = "(none)";
    debug(99, "next 8 of %d bytes @'0x%x for ctlr 0x%x: %s", out.used, out.rdofs, ctlr, debug_buf.c_str());
    for (;;)
    {
        if (!out.used) return false;
        if (!out.deque_sync(true)) { debug(70, "no sync for ctlr 0x%x @'0x%x", ctlr, out.rdofs); return false; } //skip multiple syncs (useful for loopback test)
        if (((retval.b = out.deque()) & 0x7F) != ctlr) { debug(70, "wrong controller: got 0x%x vs expected 0x%x @'0x%x", retval.b, ctlr, out.rdofs); continue; }
        else if (retval.b != (ctlr | 0x80)) { debug(70, "no response from controller 0x%x @'0x%x", ctlr, out.rdofs); continue; }
        if ((retval.b = out.deque()) != RENXt_READ_REG) { debug(70, "wrong command: got 0x%x vs. expected 0x%x for ctlr 0x%x @'0x%x", retval.b, RENXt_READ_REG, ctlr, out.rdofs); continue; }
        if ((retval.i = out.deque_uint16()) != address) { debug(70, "wrong address: got 0x%x vs. expected 0x%x for ctlr 0x%x @'0x%x", retval.i, address, ctlr, out.rdofs); continue; }
        break;
    }
//    while (len -- > 0) *(((byte*)buf)++) = out.deque();
    out.deque_buf(buf, rdlen);
    return true;
}


typedef struct { uint8_t bytes[3]; } uint24_t;

//enumerate controllers/props on a port:
//parameters:
// port = comm port to use
// props = array of props returned
// maxprops = max #entries to return
//return value:
// actual number of props found
//IFCPP(extern "C")
int RenXt_enum(const char* port, RenXt_Ctlr* ctlrs, int maxctlr)
{
    if (!OpenPorts.Contains(port)) /*OpenPorts.find(port) == OpenPorts.end())*/ return error("Port '%s' not open", port), -1;
    MyPort& port_info = OpenPorts[port];
    if (!port_info.IsOpen())
        if (!port_info.open()) return -1;
//send an enum request:
    if (!port_info.flush()) return -1; //start with empty buffer
    MyOutStream& out = port_info.io; //(&iobuf[0], iobuf.size(), pad_rate);
    out.emit_raw(RENARD_SYNC, 5); //allow controllers to auto-detect baud rate or stop what they were doing and start parsing new packet
//    out.emit(ADRS_ALL);
//    out.emit(RenXt_ENUM); //ask controllers to identify themselves
//    out.emit(RenXt_NOOP, MIN(out.unused() - 1, 10)); //placeholder for ctlr responses
    struct //CAUTION: must match firmware manifest
    {
        uint16_t stamp[3]; //magic/stamp; "RenXt\0" == 2965, 3758, 3A00
        uint16_t version; //firmware version#
        uint16_t device; //device code (which uController)
        uint16_t pins; //#I/O pins available for nodes, which I/O pin for series nodes
        uint16_t iotypes; //which node types are supported
        uint16_t dimsteps; //#steps (resolution) of dimming curve
        uint16_t ram; //total RAM available for node + palette data
        uint16_t max_baud[2]; //max baud rate (won't fit in 14 bits); little endian
        uint16_t clock[2]; //ext clock freq (if any); little endian
        uint16_t config; //config bits/ccp options
    } manif;
    struct
    {
        uint8_t demo_var; //demo/test var
        uint8_t demo_pattern;
        uint8_t junk; //last_prerr; //last protocol error
        uint8_t state; //misc state bits
// #define UNUSED_BIT0_ADDR  (8 * MISCBITS_ADDR + 0)
// #define WANT_ECHO_ADDR  (8 * MISCBITS_ADDR + 1)
// #define IO_BUSY_ADDR /*ZC_STABLE_ADDR*/  (8 * MISCBITS_ADDR + 2)
// #define PROTOCOL_LISTEN_ADDR  (8 * MISCBITS_ADDR + 3) //whether to rcv protocol byte
        uint8_t adrs; //config; controller address
        uint8_t node_config; //config; currently active node type and packing
// #define NODETYPE_MASK  0xF0 //node type in upper nibble
// #define PARALLEL_NODES_ADDR  (8 * NODE_CONFIG_ADDR + 4) //bottom bit of node type indicates series vs. parallel for smart nodes
// #define COMMON_CATHODE_ADDR  (8 * NODE_CONFIG_ADDR + 4) //bottom bit of node type indicates common anode vs. cathode for dumb nodes
// #define UNUSED_NODECONFIG_BIT3_ADDR  (8 * NODE_CONFIG_ADDR + 3)
// #define UNUSED_NODECONFIG_BIT2_ADDR  (8 * NODE_CONFIG_ADDR + 2)
// #define BPP_MASK  0x03 //bottom 2 bits: 0x00 == 4 bpp, 0x01 == 1 bpp, 0x02 == 2 bpp, 0x03 == reserved for either 6 bpp or variable (tbd)
        uint8_t node_bytes; //config; currently active node data size (scaled)
        uint24_t iochars; //stats; 24-bit counter allows minimum of 11 minutes at 250 kbaud sustained
        uint8_t protocol_errors; //stats; 4-bit counter in upper nibble(doesn't wrap), latest reason in lower nibble
        uint8_t ioerrs; //stats; 8-bit counter (doesn't wrap)
        uint8_t more_junk[3+1]; //stack frame and temps
    } nbram;
    struct
    {
        uint8_t node_config; //default node type + packing
        uint8_t node_bytes; //default node data size
        uint8_t adrs; //current controller address
        uint8_t bkg[3]; //demo bkg color
//        uint8_t strlen; //demo string length
        uint8_t demo_pattern; //demo/test pattern
        uint8_t name[sizeof(ctlrs->name)]; //prop/controller user-assigned name
    } eedata;
//    uint8_t name[32 -22];
//    int chunk_size = 0; //RENARD_SPECIAL_MIN;
    RenXt_Ctlr* svctlrs = ctlrs;
//    int svmaxprops = maxprops;
//    int pass = 1; //enqueue then dequeue
//    props = svprops;
//    maxprops = svmaxprops;
    for (bool enque = true;; enque = !enque) //enqueue then dequeue controllers in groups
    {
//        if (enque) { props = svprops; maxprops = svmaxprops; } //rewind
//        el
        int last_adrs = 0;
        bool didone = false;
        for (int adrs = ctlrs - svctlrs + 1; (adrs < /*ADRS_ALL*/ RENARD_SPECIAL_MIN) && (adrs <= maxctlr); ++adrs) //adrsofs += chunk_size)
//        for (int pass = 1; pass <= 2; ++pass) //enqueue then dequeue
        {
//            int svadrs = adrs;
//            for (;;) //int adrs = adrsofs; adrs < adrsofs + (chunk_size? ch; ++adrs)
//            while (adrs < RENARD_SPECIAL_MIN)
//            {
//                if (adrs >= RENARD_SPECIAL_MIN) break;
//              while (IsRenardSpecial(adrs)) { debug(10, "skip inv adrs 0x%x", adrs); continue; } //skip invalid addresses
            bool ok = true;
            if (!RenXt_ReadMem(out, adrs, INROM(RENXt_MANIF_ADDR), &manif /*propinfo->fwver*/, sizeof(manif /*propinfo->fwver*/), enque, !didone)) { debug(60, "no %srom", enque? "room for ": ""); /*break*/ ok = false; } //not enough room in buffer
            if (!RenXt_ReadMem(out, adrs, INRAM(WORKING_REGS + 3), &nbram.state /*&propinfo->fwver*/, sizeof(nbram /*propinfo->fwver*/) - (3+4), enque, false)) { debug(60, "no %sram", enque? "room for ": ""); /*break*/ ok = false; } //run-time data (#nodes, I/O type, etc)
            if (!RenXt_ReadMem(out, adrs, INEEPROM(0), &eedata, sizeof(eedata), enque, false)) { debug(60, "no %seeprom", enque? "room for ": ""); /*break*/ ok = false; }
            debug(60, "%sque ctlr# %d vs. max %d, ok? %d", enque? "en": "de", adrs, maxctlr, ok);
//            showbuf("PARTIAL OUT", (char*)&port_info.frbuf[0], out.used);
            if (!ok) { if (enque) break; else continue; }
            else last_adrs = adrs;
            didone = true;
            if (enque) { debug(99, "enqued"); continue; } //enque until buf full
            if (!manif.device) { debug(1, "!manif"); out.used = 0; return ctlrs - svctlrs; } //end of list; return #props found
#define A2(char1, char2)  (((char1) << 7) | (char2))
            if ((manif.stamp[0] != A2('R', 'e')) || (manif.stamp[1] != A2('n', 'X')) || (manif.stamp[2] != A2('t', '\0')))
            {
//A2(52, 65), A2(6E, 58), A2(74, 0) => 0010 1001 0110 0101, 0011 0111 0101 1000, 0011 1010 0000 0000 == 2965, 3758, 3A00
                debug(20, "invalid manifest stamp: 0x%x 0x%x 0x%x (expected 0x%x 0x%x 0x%x)", manif.stamp[0], manif.stamp[1], manif.stamp[2], A2('R', 'e'), A2('n', 'X'), A2('t', '\0'));
//                out.used = 0; //leave buf empty
//                return props - svprops;
            }
            ctlrs->address = adrs;
  //                    props->uctlr_type = 0;
//                  props->fwver = 0; //unknown
//                    props->name[0] = '\0'; //, "UNKNOWN");
//                  props->node_type = RenXt_NULLIO;
//                  props->num_nodes = 0; //unknown
//                  async_reads.emplace_back(adrs, NONBANKED_REQ, runtime, sizeof(runtime));
//                  async_reads.emplace_back(adrs, RenXt_MANIF_ADDR, &propinfo->fwver, sizeof(propinfo->fwver));
//                  async_reads.emplace_back(adrs, RenXt_EEADDR, propinfo->name, sizeof(propinfo->name));
//                    byte ch = out.deque();
//                    if (ch != RENARD_SYNC) return error("missing sync: ofs %d, got 0x%x (prop# %d)", out.used, ch, props - svprops), -1;
            ctlrs->uctlr_type = manif.device;
            ctlrs->fwver = manif.version; //unknown
            ctlrs->pins = manif.pins;
            ctlrs->ram = manif.ram;
            ctlrs->max_baud = manif.max_baud[0] + 0x1000 * manif.max_baud[1];
            ctlrs->clock = manif.clock[0] + 0x1000 * manif.clock[1];
            ctlrs->iochars = nbram.iochars.bytes[0] + 256 * nbram.iochars.bytes[1] + 256 * 256 * nbram.iochars.bytes[2];
            ctlrs->protoerrs = nbram.protocol_errors; //stats; 8-bit counter (doesn't wrap)
//            ctlrs->last_prerr = nbram.last_prerr;
            ctlrs->ioerrs = nbram.ioerrs; //stats; 8-bit counter (doesn't wrap)
//    debug(1, "baud: 0x%x 0x%x, clock 0x%x 0x%x", manif.max_baud[0], manif.max_baud[1], manif.clock[0], manif.clock[1]);
            ctlrs->node_type = nbram.node_config >> 4;
            ctlrs->num_nodes = nbram.node_bytes * 2; //* 4; //divup(manif.ram, 256); //2 nodes/byte (4 bpp)
            if (manif.ram > 256) ctlrs->num_nodes *= 2; //byte pairs
            if (manif.ram > 512) ctlrs->num_nodes *= 2; //byte quads
            eedata.name[sizeof(eedata.name) - 1] = '\0'; //make sure it's null-terminated
            strcpy(ctlrs->name, (char*)eedata.name);
            if (ctlrs->name[0] == 0xFF) ctlrs->name[0] = '\0'; //unwritten EEPROM
            if (!ctlrs->name[0]) strcpy(ctlrs->name, "(no name)");
#if 0
'[0]: x7e x7e x7e x7e x7e x7e x81 x71 x87 xf3 x65 x29 x58 x37 0 x3a  '
'[x10]: x1c 0 x68 0 0 4 0 1 xf0 0 x90 0 x3d 0 ...+2x  '
'[x20]: x94 x11 0 x1e 8 x30 xff 0 x83 x7e x81 x71 x40 x73 0 1  '
'[x30]: xa0 xc0 x13 9 0 0 0 x4f x7e x81 x71 x20 0 xff ...+2x  '
'[x60]: xff xff x7e x82 x71 x87 xf3 x65 x29 x58 x37 0 x3a x1c 0 x68  '
'[x70]: 0 0 4 0 1 xf0 0 x90 0 x3d 0 0 0 x94 x11 0  '
'[x80]: x1e 8 x30 xff 0 x83 x7e x82 x71 x40 x73 0 2 xa0 xc0 x70  '
'[x90]: 9 0 0 0 x4f x7e x82 x71 x20 0 xff ...+5x  '
'[xb0]: xff xff xff xff xff xff xff xff xff xff xff xff xff xff xff x7e  '
'[xc0]: x83 x71 x87 xf3 x65 x29 x58 x37 0 x3a x1c 0 x68 0 0 4  '
'[xd0]: 0 1 xf0 0 x90 0 x3d 0 0 0 x94 x11 0 x1e 8 x30  '
'[xe0]: xff 0 x83 x7e x83 x71 x40 x73 0 3 xa0 xc0 x75 9 0 ...+1x  '
'[xf0]: x0a x4f x7e x83 x71 x20 0 xff ...+8x  '
'[x110]: xff xff xff xff xff xff xff xff xff xff xff xff x7e x84 x71 x87  '
'[x120]: xf3 x65 x29 x58 x37 0 x3a x1c 0 x68 0 0 4 0 1 xf0  '
'[x130]: 0 x90 0 x3d 0 0 0 x94 x11 0 x1e 8 x30 xff 0 x83  '
'[x140]: x7e x84 x71 x40 x73 0 4 xa0 xc0 xc8 9 0 0 0 x4f x7e  '
'[x150]: x84 x71 x20 0 xff ...+11x  '
'[x170]: xff xff xff xff xff xff xff xff xff x7e 5 x71 x87 xf3 0 ...+1x  '
'[x190]: 0 0 0 0 0 0 0 0 0 0 0 0 0 x7e 5 x71  '
'[x1a0]: x40 x73 0 0 0 0 0 0 0 0 0 0 x7e 5 x71 x20  '
'[x1b0]: 0 ...+15x  '
#endif // 0
            ++ctlrs;
//            if (--maxctlr < 1) { out.used = 0; return ctlrs - svctlrs; } //#props found
        }
        if (!enque) { if (didone) continue; break; }
        out.emit_raw(RENARD_SYNC); //send out a final sync to mark end of last packet
//          if (out.deque() != ADRS_ALL) return debug(10, "didn't get adrs_all: 0x%x", out.PrevByte), 0;
        debug(1, "used %d of %d bytes (or tried to), last id was 0x%x, %d last adrs found", out.used, port_info.io.frbuf.size(), last_adrs);
//        if (out.overflow()) return error("output buffer overflow: needed %d, only had %d (overflowed by %d)", out.used, port_info.frbuf.size(), out.overflow()), -1;
        if (!port_info.flush()) { debug(1, "flush failed"); out.used = 0; return -1; }
        if (!port_info.input(2 * 1 K / MAX(port_info.fps, 1))) { debug(1, "input failed"); out.used = 0; return -1; } //wait 2 frames
//        out.used = 0; //clear out buf so we can fill it again
#if 0 //hard-coded test
#pragma message WARN("TEMP: hard-coded response")
//            error("TODO: send request, receive response");
        for (int i = 0; (i < port_info.frbuf.size()) && (i < 20); ++i) //fill in a few placeholder bytes
            if (frbuf[i] == RENXT_NOOP) frbuf[i] = 'A';
//        port_info.frbuf[53] = 0x24; port_info.frbuf[54] = 0x28; port_info.frbuf[58] = 0x28; //simulate a few controllers
//            out.rewind(&port_info.frbuf[0], 60); //, port_info.pad_rate);
//            out.remove_pads();
#endif
    }
//first pass: create list of controllers found:
//    out.deque_sync();
//    if (out.deque() != ADRS_ALL) return debug(10, "didn't get adrs_all: 0x%x", out.PrevByte), 0;
//    if (out.deque() != RenXt_ENUM) return debug(10, "didn't get enum cmd: 0x%x", out.PrevByte), 0;
//    std::vector<AsyncRead> async_reads;
//    RenXt_Prop* svprops = props;
#if 0
    for (int adrs = 1; adrs < ADRS_ALL; ++adrs)
    {
//        while (IsRenardSpecial(adrs)) { debug(10, "skip inv adrs 0x%x", adrs); continue; } //skip invalid addresses
//        byte ch = out.deque();
//        if (ch == RENXT_NOOP) { debug(10, "got noop 0x%x", ch); continue; }
        props->address = adrs;
//        out.deque_buf(&manif, sizeof(manif));
//        out.deque_buf(&nbram, sizeof(nbram) - 7);
//        out.deque_buf(props->name, sizeof(props->name));
        if (!RenXt_ReadMem(out, adrs, INROM(RENXT_MANIF_ADDR), &manif /*propinfo->fwver*/, sizeof(manif /*propinfo->fwver*/), FALSE)) break; //not enough room in buffer
        if (!RenXt_ReadMem(out, adrs, INRAM(WORKING_REGS + 3), &nbram /*&propinfo->fwver*/, sizeof(nbram /*propinfo->fwver*/) - 7, FALSE)) break; //run-time data (#nodes, I/O type, etc)
        if (!RenXt_ReadMem(out, adrs, INEEPROM(6), props->name, sizeof(props->name), FALSE)) break;
        byte ch = out.deque();
        if (ch != RENARD_SYNC) return error("missing sync: ofs %d, got 0x%x (prop# %d)", out.used, ch, props - svprops), -1;
        props->uctlr_type = manif.device;
        props->fwver = manif.version; //unknown
        props->node_type = nbram.node_config >> 4;
        props->num_nodes = nbram.node_bytes * 2; //2 nodes/byte (4 bpp)
        if (manif.ram >= 256) props->num_nodes *= 2; //byte pairs
        if (manif.ram >= 512) props->num_nodes *= 2; //byte quads
        ++props;
        if (--maxprops < 1) break;
    }
#endif
#if 0
//second pass: fill in additional info for each controller:
    byte runtime[16]; //holding area for run-time data from controller
    out.rewind(&port_info.frbuf[0], 0); //, port_info.pad_rate);
    for (RenXt_Prop* propinfo = svprops; propinfo != props; ++propinfo)
    {
        switch (propinfo->name[0]) //kludge: first byte of name is used as temp state info
        {
            case 0: //attempt enque
            case 3: //attempt deque
            case 1+10: //resume after I/O interrupt
                if (!RenXt_ReadMem(out, propinfo->address, RenXt_MANIF_ADDR, &propinfo->fwver, sizeof(propinfo->fwver), propinfo->name[0] < 3)) break;
                if (propinfo->name[0] >= 10) { propinfo->name[0] -= 10; --propinfo; continue; }
                ++propinfo->name[0];
                //fall thru
            case 1: //attempt enque
            case 4: //attempt deque
            case 2+10: //resume after I/O interrupt
                if (!RenXt_ReadMem(out, propinfo->address, NONBANKED_REG, runtime, sizeof(runtime), propinfo->name[0] < 3)) break; //run-time data (#nodes, I/O type, etc)
                if (propinfo->name[0] >= 10) { propinfo->name[0] -= 10; --propinfo; continue; }
                propinfo->node_type = runtime[3]; //TODO
                propinfo->num_nodes = runtime[5]; //TODO
                ++propinfo->name[0];
                //fall thru
            case 2: //attempt enque
            case 5: //attempt deque
                char name[sizeof(propinfo->name)];
                if (!RenXt_ReadMem(out, propinfo->address, RenXt_EEADR, (byte*)name, sizeof(name), propinfo->name[0] < 3)) break;
                if (propinfo->name[0] >= 3)
                {
                    strcpy(name, propinfo->name);
                    /*if (found)*/ continue; //all reads for this controller were complete; move to next
                }
                ++propinfo->name[0];
                if (propinfo < props) continue; //enqueue reads for next controller
                break;
        }
        if (propinfo->name[0]) propinfo->name[0] += 10; //mark as partially enqueued
        if (out.used) out.emit_raw(RENARD_SYNC); //send out a final sync to mark end of last packet
        if (!port_info.flush()) return -1;
        if (!port_info.input(2 * 1 K / MIN(port_info.fps, 1))) return -1; //wait 2 frames
        propinfo = svprops - 1; //start another pass
    }
#endif
    out.used = 0; //leave buf empty
    return ctlrs - svctlrs; //#props found
}


//check if any controllers are out there listening:
//IFCPP(extern "C")
int RenXt_discover(const char* port, byte* adrsptr, int maxadrs)
{
    byte* svptr = adrsptr;
    if (!OpenPorts.Contains(port)) /*OpenPorts.find(port) == OpenPorts.end())*/ return error("Port '%s' not open", port), -1;
    MyPort& port_info = OpenPorts[port];
    if (!port_info.IsOpen())
        if (!port_info.open()) return -1;
//send an enum request:
    if (!port_info.flush()) return -1; //start with empty buffer
    MyOutStream& out = port_info.io; //(&iobuf[0], iobuf.size(), pad_rate);
    out.emit_raw(RENARD_SYNC, 5); //allow controllers to auto-detect baud rate or stop what they were doing and start parsing new packet
    for (bool enque = true;; enque = !enque) //enqueue then dequeue controllers in groups
    {
        for (int adrs = 0; adrs < MIN(maxadrs, RENARD_SPECIAL_MIN); ++adrs)
        {
            debug(99, "%sque, adrs 0x%x, used %d, rdofs %d", enque? "en": "de", adrs, out.used, out.rdofs);
            if (enque)
            {
                out.emit(adrs);
                out.emit_raw(RENARD_SYNC);
                if (out.overflow()) break;
            }
            else
            {
//                debug(1, "decode[%d] ofs %d", adrs, out.used);
                if (!out.deque_sync(true)) { debug(10, "no sync for ctlr 0x%x", adrs); break; }
                byte ack = out.deque();
                if ((ack & 0x7F) != adrs) return error("wrong controller (packet garbled?): 0x%x vs 0x%x", ack, adrs), -1;
                if (!(ack & 0x80)) { debug(90, "no response from controller 0x%x", adrs); continue; }
                debug(10, "ack from adrs 0x%x", adrs);
                *adrsptr++ = adrs;
            }
        }
        if (!enque) break;
        debug(1, "used %d of %d bytes", out.used, port_info.io.frbuf.size());
        if (!port_info.flush()) { out.used = 0; return -1; }
        if (!port_info.input(2 * 1 K / MAX(port_info.fps, 1))) { out.used =0; return -1; } //wait 2 frames
    }
    int retval = adrsptr - svptr;
    memset(adrsptr, 0, svptr + maxadrs - adrsptr);
    return retval;
}


//enqueue command byte(s) to a controller:
//NOTE: bytes are not flushed
//parameters:
// port = comm port to use
// ctlr = controller address
//return value:
// none
// >= 0 => #bytes enqueued
// < 0 => error (#bytes overflowed)
//IFCPP(extern "C")
int RenXt_command(const char* port, byte ctlr, const byte* bytes, size_t numbytes)
{
    if (!OpenPorts.Contains(port)) /*OpenPorts.find(port) == OpenPorts.end())*/ return error("Port '%s' not open", port), -1;
    MyPort& port_info = OpenPorts[port];
    if (!port_info.IsOpen())
        if (!port_info.open()) return -1;
//send an enum request:
//    if (!port_info.flush()) return -1; //start with empty buffer
    MyOutStream& out = port_info.io; //(&iobuf[0], iobuf.size(), pad_rate);
    int svused = out.used;
    if ((int)numbytes > 0)
    {
        out.emit_raw(RENARD_SYNC); //start new packet
        out.emit(ctlr);
        out.emit_buf(bytes, numbytes);
        debug(10, "%d bytes were waiting, %d now waiting to go to %s", svused, out.used, port);
        showbuf("cur buf", reinterpret_cast<byte*>(&port_info.io.frbuf[0]), out.used, true);
    }
    else if ((int)numbytes < 0) //raw
    {
        out.emit_rawbuf(bytes, -numbytes);
        debug(10, "%d bytes were waiting, %d now waiting to go to %s", svused, out.used, port);
        showbuf("cur buf", reinterpret_cast<byte*>(&port_info.io.frbuf[0]), out.used, true);
    }
    else
    {
    debug(1, "  %d", numbytes);
        if (!port_info.flush()) return -1;
        out.used = 0;
        if (!port_info.input(2 * 1 K / MAX(port_info.fps, 1))) return -1; //wait 2 frames
        debug(10, "%d bytes were sent, %d bytes came back for port %s", svused, out.used, port);
        showbuf("in buf", reinterpret_cast</*const*/ byte*>(&port_info.io.frbuf[0]), out.used, true);
        out.used = 0; //clear in case user sends command manually next time
    }
    return out.overflow()? -out.overflow(): out.used; //numbytes;
}


//close one or all ports:
//parameters:
// port = comm port to use; NULL => all
//return value:
// none
// >= 0 => closed
// < 0 => error#
//IFCPP(extern "C")
int RenXt_close(const char* port)
{
//    int oldsize = OpenPorts.size();
    if (!port)
    {
        int retval = std::numeric_limits<int>::max();
        for (auto it = OpenPorts.begin(); it != OpenPorts.end(); ++it)
            if (it->second.IsOpen())
                retval = min_safe<int>(retval, RenXt_close(it->second.name.c_str())); //remember most severe error
        return retval;
    }
    if (!OpenPorts.Contains(port)) /*.find(port) == OpenPorts.end())*/ return error("Port '%s' not open", port), -1;
    MyPort& port_info = OpenPorts[port];
    MyOutStream& out = port_info.io; //(&iobuf[0], iobuf.size(), pad_rate);
    if (out.used && !port_info.IsOpen()) debug(3, "tossing %d bytes for port %s", out.used, port);
    if (out.used && port_info.IsOpen()) //flush buffered data, wait for response
    {
        out.emit_raw(RENARD_SYNC); //send out a final sync to mark end of last packet
//          if (out.deque() != ADRS_ALL) return debug(10, "didn't get adrs_all: 0x%x", out.PrevByte), 0;
        debug(1, "used %d of %d bytes", out.used, port_info.io.frbuf.size());
//        if (out.overflow()) return error("output buffer overflow: needed %d, only had %d (overflowed by %d)", out.used, port_info.frbuf.size(), out.overflow()), -1;
        if (!port_info.flush()) return -1;
        if (!port_info.input(2 * 1 K / MAX(port_info.fps, 1))) return -1; //wait 2 frames
    }
    int retval = port_info.close()? 0: -1;
    debug(10, "port %s closed now? %d", port, !port_info.IsOpen());
    return retval;
}

//eof