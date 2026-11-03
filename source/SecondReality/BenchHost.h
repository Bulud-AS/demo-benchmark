#pragma once
#include <stdexcept>
bool bench_init(int argc, char** argv);
void bench_close(const char* error);
int bench_vsync();
void bench_part(int index);
int bench_start_part();
bool bench_muted();
void bench_finished();
