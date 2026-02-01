/*
author          Oliver Blaser
date            01.02.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#ifndef IG_PROJECT_H
#define IG_PROJECT_H



namespace prj {

static const char* const appName = "PHYoIP Server";
static const char* const binName = "phyoip-server";
static const char* const dirName = "phyoip-server";

static const char* const website = "https://github.com/PHYoIP/server";

constexpr int copyrightYear = 2026;

}

#define PRJ_VERSION_MAJ   (1)
#define PRJ_VERSION_MIN   (0)
#define PRJ_VERSION_PAT   (0)
#define PRJ_VERSION_PRSTR ("alpha") // empty string ("") if it's not a pre-release
#define PRJ_VERSION_BUILD ("")
#define PRJ_VERSION_STR   PRJ___versionStr___()
#ifdef PRJ_VERSION_STR
const char* PRJ___versionStr___();
#endif



#if (defined(DEBUG) || defined(_DEBUG))
#define PRJ_DEBUG (1)
#endif



#endif // IG_PROJECT_H
