#!/bin/bash

# author        Oliver Blaser
# date          16.10.2026
# copyright     GPL-3.0 - Copyright (c) 2026 Oliver Blaser

prjName="PHYoIP Server"
prjDisplayName=$prjName
prjBinName="phyoip-server"
prjDirName=$prjBinName
repoDirName="server"
repoUrlBase="https://github.com/PHYoIP"
licenseIdentifier="GPL-3.0"
copyrightYear="2026"

versionstr=$(head -n 1 dep_vstr.txt)
versionstrDeb=$(echo "$versionstr" | tr - "~") # convert semver pre-release delimiter to debian compatible char

function ptintTitle()
{
    if [ "$2" = "" ]
    then
        echo "  --=====#   $1   #=====--"
    else
        echo -e "\033[3$2m  --=====#   \033[9$2m$1\033[3$2m   #=====--\033[39m"
    fi
}

# pass output filename as argument
function writeReadmeFile()
{
    echo ""                                 > $1
    echo "${prjDisplayName} v${versionstr}" >> $1
    echo ""                                 >> $1
    echo "${licenseIdentifier} - Copyright (c) ${copyrightYear} Oliver Blaser" >> $1
    echo ""                                 >> $1
    echo "${repoUrlBase}/${repoDirName}"    >> $1
}
