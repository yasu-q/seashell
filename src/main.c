#include "seashell.h"

int main(int argc, char** argv) {
    seashell_print_startup_header();
    int stat = seashell_loop();

    return stat;
}