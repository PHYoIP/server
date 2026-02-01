/*
author          Oliver Blaser
date            01.02.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include "common/ansi-esc.h"
#include "common/util/macros.h"
#include "common/windows.h"
#include "project.h"


#define LOG_MODULE_LEVEL LOG_LEVEL_DBG
#define LOG_MODULE_NAME  MAIN
#include "common/log.h"



using std::cout;
using std::endl;
using std::setw;



namespace argstr {
static const char* const noColour = "--no-colour";
static const char* const help = "--help";
static const char* const version = "--version";

static bool contains(int argc, char** argv, const std::string& arg);
}



static const std::string usageString = std::string(prj::binName) + " PORT [SPCFG DEV BAUD [CONFIG]]";



static int parseGatewayArgs(int argc, char** argv);
static void printHelp();
static void printUsageAndTryHelp();
static void printVersion();



int main(int argc, char** argv)
{
#if PRJ_DEBUG && 1
    char* dbgArgs[] = {
        argv[0],

        //"--help",
        //"--version",

        "5055",

        "A,LF",
        //"A,LF,24",

        "COM7",

        "115200",

        "7E2",

        //"--no-colour",

        NULL,
    };
    argc = (sizeof(dbgArgs) / sizeof(dbgArgs[0])) - 1; // don't count the NULL terminator element
    argv = dbgArgs;
#endif

    if (argstr::contains(argc, argv, argstr::noColour)) { ansi::initEscapeCodes(false); }
    else
    {
        ansi::initEscapeCodes(true);
#ifdef _WIN32
        windows::enableVTProcessing();
#endif
    }

    if (argstr::contains(argc, argv, argstr::help))
    {
        printHelp();
        return 0;
    }

    if (argstr::contains(argc, argv, argstr::version))
    {
        printVersion();
        return 0;
    }



    if (argc < 2)
    {
        LOG_ERR("missing PORT");
        printUsageAndTryHelp();
        return 1;
    }

    const int port = std::atoi(argv[1]);
    if ((port < 1) || (port > UINT16_MAX))
    {
        LOG_ERR("invalid port: %s", argv[1]);
        printUsageAndTryHelp();
        return 1;
    }

    if (argc > 2)
    {
        const int err = parseGatewayArgs(argc, argv);
        if (err) { return 1; }
        else
        {
            // TODO
            LOG_ERR("the gateway is not yet implemented");
            return 1;
        }
    }



    // TODO
    LOG_INF("%sStart Server", ansi::esc[SGR_FG_BGREEN]);



    return 0;
}



int parseGatewayArgs(int argc, char** argv)
{
    if (argc < 5)
    {
        LOG_ERR("missing gateway arguments");
        printUsageAndTryHelp();
        return -(__LINE__);
    }

    const char* const spcfgStr = argv[2];
    const char* const portStr = argv[3];
    const char* const baudStr = argv[4];

    LOG_ERR("%s is not yet implemented", UTIL__FUNCNAME__);
    return -(__LINE__);

    if (argc > 5) { const char* const cfgStr = argv[5]; }
}

void printHelp()
{
    constexpr int lw = 20;

    cout << prj::appName << endl;
    cout << endl;
    cout << "Usage:" << endl;
    cout << "  " << usageString << endl;
    cout << endl;
    cout << "PORT:   TCP port to listen for clients" << endl;
    cout << "SPCFG:  serial protocol config" << endl;
    cout << "DEV:    serial port, e.g.: \"/dev/ttyS4\", \"COM3\"" << endl;
    cout << "BAUD:   baudrate of the serial port" << endl;
    cout << "CONFIG: serial config, default: 8N1" << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << std::left << setw(lw) << std::string("  ") + argstr::noColour << "monochrome console output" << endl;
    cout << std::left << setw(lw) << std::string("  ") + argstr::help << "prints this help text" << endl;
    cout << std::left << setw(lw) << std::string("  ") + argstr::version << "prints version info" << endl;
    cout << endl;
    cout << endl;
    cout << "Serial Protocol Config:" << endl;
    cout << endl;
    cout << "  ASCII Protocol: \"A,STOP[,START]\"" << endl;
    cout << "    Start and stop symbols can be a hexadecimal number or an identifier: \"LF\"," << endl;
    cout << "    \"CR\", \"CRLF\", \"NULL\"" << endl;
    cout << endl;
    cout << "  Binary Protocol: \"B,TO[,LEN]\"" << endl;
    cout << "    TO:  timout as microseconds" << endl;
    cout << "    LEN: byte offset of the length specifier of the message" << endl;
    cout << endl;
    cout << endl;
    cout << "Website: <" << prj::website << ">" << endl;
}

void printUsageAndTryHelp()
{
    cout << "Usage: " << usageString << "\n\n";
    cout << "Try '" << prj::binName << " --help' for more options." << endl;
}

void printVersion()
{
    const int maj = PRJ_VERSION_MAJ;
    const int min = PRJ_VERSION_MIN;
    const int pat = PRJ_VERSION_PAT;
    const char* const pr = PRJ_VERSION_PRSTR;
    const char* const build = PRJ_VERSION_BUILD;

    const int hasPr = (pr[0] != 0);
    const int hasBuild = (build[0] != 0);

    cout << prj::appName << "   ";
    if (hasPr) { cout << ansi::esc[SGR_FG_BMAGENTA]; }
    cout << PRJ_VERSION_STR;
    if (hasPr) { cout << ansi::esc[SGR_FG_DEFAULT]; }
#if PRJ_DEBUG
    cout << "   " << ansi::esc[SGR_FG_BRED] << "DEBUG" << ansi::esc[SGR_FG_DEFAULT] << "   " << __DATE__ << " " << __TIME__;
#endif
    cout << endl;

    cout << endl;
    cout << "project page: " << prj::website << endl;
    cout << endl;
    cout << "Copyright (c) " << prj::copyrightYear << " Oliver Blaser." << endl;
    cout << "License: GNU GPLv3 <http://gnu.org/licenses/>." << endl;
    cout << "This is free software. There is NO WARRANTY." << endl;
}

bool argstr::contains(int argc, char** argv, const std::string& arg)
{
    for (int i = 1; i < argc; ++i)
    {
        if (*(argv + i) == arg) { return true; }
    }

    return false;
}



#ifdef PRJ_VERSION_STR
#include <stdio.h>
#include <string.h>
const char* PRJ___versionStr___()
{
    static char tmp[30] = "X.Y.Z-pr+build";

    if (tmp[0] == 'X')
    {
        int printRes;

        const int maj = PRJ_VERSION_MAJ;
        const int min = PRJ_VERSION_MIN;
        const int pat = PRJ_VERSION_PAT;
        const char* const pr = PRJ_VERSION_PRSTR;
        const char* const build = PRJ_VERSION_BUILD;

        const int hasPr = (pr[0] != 0);
        const int hasBuild = (build[0] != 0);

        if (hasPr && !hasBuild) { printRes = snprintf(tmp, sizeof(tmp), "%i.%i.%i-%s", maj, min, pat, pr); }
        else if (!hasPr && hasBuild) { printRes = snprintf(tmp, sizeof(tmp), "%i.%i.%i+%s", maj, min, pat, build); }
        else if (hasPr && hasBuild) { printRes = snprintf(tmp, sizeof(tmp), "%i.%i.%i-%s+%s", maj, min, pat, pr, build); }
        else { printRes = snprintf(tmp, sizeof(tmp), "%i.%i.%i", maj, min, pat); }

        if (!((printRes > 0) && (printRes < sizeof(tmp))))
        {
#ifdef _MSC_VER
#pragma warning(suppress :4996)
            strncpy(tmp, "X.Y.Z-pr+build", sizeof(tmp));
#else
            strncpy(tmp, "X.Y.Z-pr+build", sizeof(tmp));
#endif
            tmp[sizeof(tmp) - 1] = 0;
        }
    }

    return tmp;
}
#endif // PRJ_VERSION_STR



#define COMMON_LOG_DEFINE_FUNCTIONS
#include "common/log.h"
