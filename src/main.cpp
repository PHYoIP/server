/*
author          Oliver Blaser
date            01.02.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <iostream>



int main(int argc, char** argv)
{
    for (int i = 0; i < argc; ++i) { std::cout << i << ": " << *(argv + i) << std::endl; }

    return 0;
}
